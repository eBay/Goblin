/************************************************************************
Copyright 2021-2022 eBay Inc.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
**************************************************************************/

#include "RocksDBKVStore.h"

#include <infra/monitor/MonitorTypes.h>
#include <absl/strings/str_cat.h>
#include <rocksdb/db.h>
#include <spdlog/spdlog.h>

#include "../utils/TimeUtil.h"
#include "../utils/TPRegistryEx.h"
#include "VersionStore.h"

namespace goblin::kvengine::store {

RocksDBKVStore::RocksDBKVStore(
    const std::string &walDir,
    const std::string &dbDir,
    const std::vector<WSName> &cfs,
    WSLookupFunc defaultWSLookupFunc):
  mWalDir(walDir),
  mDBDir(dbDir),
  mDefaultWSLookupFunc(defaultWSLookupFunc),
  mRocksdbTotalDataSize(gringofts::getGauge("rocksdb_total_data_size", {})),
  mRocksdbEstimatedKeyNum(gringofts::getGauge("rocksdb_estimated_key_num", {})) {
    assert(cfs.size() > 0);
    /// default ws lookup func shouldn't be null
    assert(mDefaultWSLookupFunc);
    for (auto cf : cfs) {
      SPDLOG_INFO("init rocksdb with cf: {}", cf);
      mColumnFamilyNames.push_back(cf);
    }
    mCompactionFilter = std::make_unique<TTLCompactionFilter>();
    assert(open().isOK());
}

RocksDBKVStore::~RocksDBKVStore() {
  assert(close().isOK());
}

utils::Status RocksDBKVStore::open() {
  /// db options
  rocksdb::DBOptions dbOptions;

  dbOptions.IncreaseParallelism();
  dbOptions.create_if_missing = true;
  dbOptions.create_missing_column_families = true;
  dbOptions.wal_dir = mWalDir;

  /// column family options
  rocksdb::ColumnFamilyOptions columnFamilyDefaultOptions;
  rocksdb::ColumnFamilyOptions columnFamilyOptions;

  /// default CompactionStyle for column family is kCompactionStyleLevel
  columnFamilyDefaultOptions.OptimizeLevelStyleCompaction();
  columnFamilyOptions.OptimizeLevelStyleCompaction();
  columnFamilyOptions.compaction_filter = mCompactionFilter.get();

  std::vector <rocksdb::ColumnFamilyDescriptor> columnFamilyDescriptors;
  columnFamilyDescriptors.emplace_back(RocksDBConf::kCFMetaDefault, columnFamilyDefaultOptions);
  for (auto name : mColumnFamilyNames) {
    columnFamilyDescriptors.emplace_back(name, columnFamilyOptions);
  }

  /// open DB
  auto ts1InNano = utils::TimeUtil::currentTimeInNanos();
  rocksdb::DB *db;
  auto status = rocksdb::DB::Open(dbOptions, mDBDir,
                                  columnFamilyDescriptors, &mColumnFamilyHandles, &db);
  auto ts2InNano = utils::TimeUtil::currentTimeInNanos();

  assert(status.ok());
  mRocksDB.reset(db);

  SPDLOG_INFO("open RocksDB, wal.dir: {}, db.dir: {}, cf num: {}, handler: {}, timeCost: {}ms",
              mWalDir, mDBDir, columnFamilyDescriptors.size(), mColumnFamilyHandles.size(),
              (ts2InNano - ts1InNano) / 1000.0 / 1000.0);

  return utils::Status::ok();
}

utils::Status RocksDBKVStore::close() {
  auto ts1InNano = utils::TimeUtil::currentTimeInNanos();

  /// close column families
  for (auto handle : mColumnFamilyHandles) {
    delete handle;
  }

  /// close DB
  /// dbPtr should be the last shared_ptr pointing to DB, we leverage it to delete DB.
  mRocksDB.reset();

  auto ts2InNano = utils::TimeUtil::currentTimeInNanos();
  SPDLOG_INFO("close RocksDB, timeCost: {}ms",
              (ts2InNano - ts1InNano) / 1000.0 / 1000.0);
  return utils::Status::ok();
}

void RocksDBKVStore::toRawValue(
    const ValueType &value,
    const VersionType &version,
    const proto::Meta::OpType opType,
    RawValueType *outRaw,
    const utils::TimeType &updateTime,
    const goblin::proto::UserDefinedMeta &udfMeta) {
  proto::Payload payload;
  auto meta = payload.mutable_meta();
  meta->set_version(version);
  meta->set_updatetime(updateTime);
  for (auto &val : udfMeta.uintfield()) {
    meta->mutable_udfmeta()->add_uintfield(val);
  }
  for (auto &str : udfMeta.strfield()) {
    meta->mutable_udfmeta()->add_strfield(str);
  }
  meta->set_optype(opType);
  meta->set_datatype(proto::Meta::NORMAL);
  auto data = payload.mutable_data();
  data->mutable_singlevalue()->set_value(value);

  const uint32_t headerLen = sizeof(Header);
  Header header{};
  header.mType = proto::Meta::NORMAL;
  header.mMetaLen = meta->ByteSizeLong();

  *outRaw = std::string(reinterpret_cast<const char*>(&header), headerLen);
  *outRaw += meta->SerializeAsString();
  *outRaw += data->SerializeAsString();
}

void RocksDBKVStore::toRawValue(
    const ValueType &value,
    const VersionType &version,
    const TTLType &ttl,
    const utils::TimeType& deadline,
    const proto::Meta::OpType opType,
    RawValueType *outRaw,
    const utils::TimeType &updateTime,
    const goblin::proto::UserDefinedMeta &udfMeta) {
  proto::Payload payload;
  auto meta = payload.mutable_meta();
  meta->set_version(version);
  meta->set_optype(opType);
  meta->set_datatype(proto::Meta::NORMAL_WITH_TTL);
  meta->set_ttl(ttl);
  meta->set_deadline(deadline);
  meta->set_updatetime(updateTime);
  for (auto &val : udfMeta.uintfield()) {
    meta->mutable_udfmeta()->add_uintfield(val);
  }
  for (auto &str : udfMeta.strfield()) {
    meta->mutable_udfmeta()->add_strfield(str);
  }
  auto data = payload.mutable_data();
  data->mutable_singlevalue()->set_value(value);

  const uint32_t headerLen = sizeof(Header);
  Header header{};
  header.mType = proto::Meta::NORMAL_WITH_TTL;
  header.mMetaLen = meta->ByteSizeLong();

  *outRaw = std::string(reinterpret_cast<const char*>(&header), headerLen);
  *outRaw += meta->SerializeAsString();
  *outRaw += data->SerializeAsString();
}

utils::Status RocksDBKVStore::fromRawValue(const RawValueType &raw, proto::Payload *payload) {
  const uint32_t headerLen = sizeof(Header);
  auto size = raw.size();
  if (size <= headerLen) {
    SPDLOG_ERROR("corrupted data, data size: {}, expected header size: {}", size, headerLen);
    return utils::Status::error("corrupted header size");
  }
  const auto *header = reinterpret_cast<const Header*>(raw.data());
  if (header->mType != proto::Meta::NORMAL && header->mType != proto::Meta::NORMAL_WITH_TTL) {
    SPDLOG_ERROR("invalid data type: {}", header->mType);
    return utils::Status::error("invalid type in header");
  }
  auto metaOffset = headerLen;
  if (size <= metaOffset + header->mMetaLen ||
      !payload->mutable_meta()->ParseFromArray(
        reinterpret_cast<const char*>(raw.data() + metaOffset), header->mMetaLen)) {
    SPDLOG_ERROR("corrupted data, data size: {}, expected meta offset: {}, len: {}",
        size, metaOffset, header->mMetaLen);
    return utils::Status::error("corrupted payload");
  }
  auto dataOffset = headerLen + header->mMetaLen;
  if (size <= dataOffset ||
      !payload->mutable_data()->ParseFromArray(
        reinterpret_cast<const char*>(raw.data() + dataOffset), size - dataOffset)) {
    SPDLOG_ERROR("corrupted data, data size: {}, expected data offset: {}", size, dataOffset);
    return utils::Status::error("corrupted payload");
  }
  return utils::Status::ok();
}

utils::Status RocksDBKVStore::parseMeta(const RawValueType &raw, proto::Meta* meta) {
  const uint32_t headerLen = sizeof(Header);
  auto size = raw.size();
  if (size <= headerLen) {
    SPDLOG_ERROR("corrupted data, data size: {}, expected header size: {}", size, headerLen);
    return utils::Status::error("corrupted header size");
  }
  const auto *header = reinterpret_cast<const Header*>(raw.data());
  if (header->mType != proto::Meta::NORMAL && header->mType != proto::Meta::NORMAL_WITH_TTL) {
    SPDLOG_ERROR("invalid data type: {}", header->mType);
    return utils::Status::error("invalid type in header");
  }
  auto metaOffset = headerLen;
  if (size <= metaOffset + header->mMetaLen ||
      !meta->ParseFromArray(
        reinterpret_cast<const char*>(raw.data() + metaOffset), header->mMetaLen)) {
    SPDLOG_ERROR("corrupted data, data size: {}, expected meta offset: {}, len: {}",
        size, metaOffset, header->mMetaLen);
    return utils::Status::error("corrupted payload");
  }
  return utils::Status::ok();
}

rocksdb::ColumnFamilyHandle* RocksDBKVStore::findCFHandle(
    const KeyType &key,
    WSLookupFunc wsLookup) {
  WSName wsName;
  if (key == RocksDBConf::kCFMetaDefault) {
    return mColumnFamilyHandles[0];
  } else {
    if (wsLookup) {
      wsName = wsLookup(key);
    } else {
      wsName = mDefaultWSLookupFunc(key);
    }
    /// SPDLOG_INFO("debug: look up cf {} for key {}", wsName, key);
    auto it = std::find(mColumnFamilyNames.begin(), mColumnFamilyNames.end(), wsName);
    assert(it != mColumnFamilyNames.end());
    int index = std::distance(mColumnFamilyNames.begin(), it);
    return mColumnFamilyHandles[index + 1];
  }
}

rocksdb::ColumnFamilyHandle* RocksDBKVStore::findCFHandle(WSName wsName) {
  if (wsName == RocksDBConf::kCFMetaDefault) {
    return mColumnFamilyHandles[0];
  } else {
    auto it = std::find(mColumnFamilyNames.begin(), mColumnFamilyNames.end(), wsName);
    assert(it != mColumnFamilyNames.end());
    int index = std::distance(mColumnFamilyNames.begin(), it);
    return mColumnFamilyHandles[index + 1];
  }
}

/// @todo This function is implemented as prometheus counter now. It's not a best practise.
///       It's better to refactor code to use prometheus historgam instead.
///       https://jirap.corp.ebay.com/browse/RTCUTOFF-6927
void RocksDBKVStore::reportWriteMetrics(const KeyType &key, const ValueType &value) const {
  // each bucket's range is ( prev ceil , this ceil ]
  // each metric has 10 buckets
  static const uint64_t BUCKET_SIZE = 10, KEY_FACTOR = 2, VALUE_FACTOR = 10;
  static const auto getLabelValueFunc = [](const uint64_t factor, const uint64_t value) {
    uint64_t ceil = 1;
    for (size_t i = 0; i < BUCKET_SIZE; ++i) {
      ceil *= factor;
      if (value <= ceil) {
        return std::to_string(ceil);
      }
    }
    return std::string("infinite");
  };

  /// @note If a bucket is empty, it's confuse, so it's better to use prometheus historgam.
  std::string labelValue = getLabelValueFunc(KEY_FACTOR, key.size());
  gringofts::getCounter("write_key_size", {{"bucket_ceil", labelValue}}).increase();

  labelValue = getLabelValueFunc(VALUE_FACTOR, value.size());
  gringofts::getCounter("write_value_size", {{"bucket_ceil", labelValue}}).increase();
}

utils::Status RocksDBKVStore::writeKV(
    const KeyType &key,
    const ValueType &value,
    const VersionType &version,
    const utils::TimeType &updateTime,
    const goblin::proto::UserDefinedMeta &udfMeta,
    WSLookupFunc wsLookup) {
  auto cfHandle = findCFHandle(key, wsLookup);
  RawValueType raw;
  toRawValue(value, version, proto::Meta::SINGLE, &raw, updateTime, udfMeta);
  auto s = mWriteBatch.Put(cfHandle, key, raw);
  if (!s.ok()) {
    SPDLOG_ERROR("Error writing RocksDB: {}. Exiting...", s.ToString());
    assert(0);
  }
  onWriteValue(key, value, version);
  reportWriteMetrics(key, value);
  return utils::Status::ok();
}

utils::Status RocksDBKVStore::writeTTLKV(
    const KeyType &key,
    const ValueType &value,
    const VersionType &version,
    const TTLType &ttl,
    const utils::TimeType& deadline,
    const utils::TimeType &updateTime,
    const goblin::proto::UserDefinedMeta &udfMeta,
    WSLookupFunc wsLookup) {
  auto cfHandle = findCFHandle(key, wsLookup);
  RawValueType raw;
  toRawValue(value, version, ttl, deadline, proto::Meta::SINGLE, &raw, updateTime, udfMeta);

  auto s = mWriteBatch.Put(cfHandle, key, raw);
  if (!s.ok()) {
    SPDLOG_ERROR("Error writing RocksDB: {}. Exiting...", s.ToString());
    assert(0);
  }
  reportWriteMetrics(key, value);
  return utils::Status::ok();
}

utils::Status RocksDBKVStore::readKV(
    const KeyType &key,
    ValueType *outValue,
    TTLType *outTTL,
    VersionType *outVersion,
    WSLookupFunc wsLookup) {
  auto cfHandle = findCFHandle(key, wsLookup);
  RawValueType raw;
  auto s = mRocksDB->Get(mReadOptions, cfHandle, key, &raw);
  TEST_POINT_WITH_TWO_ARGS(
      mTPProcessor,
      utils::TPRegistryEx::RocksDBKVStore_readKV_mockGetResult,
      &s, outValue);
  if (s.ok()) {
    proto::Payload payload;
    auto status = fromRawValue(raw, &payload);
    if (!status.isOK()) {
      SPDLOG_ERROR("Error reading kv {} from RocksDB: deserialization failed.", key);
      assert(0);
    }
    const auto &data = payload.data();
    if (!data.has_singlevalue()) {
      SPDLOG_ERROR("Error reading kv {} from RocksDB: single value payload expected.", key);
      assert(0);
    }
    if (isTimeOut(payload.meta().deadline(), payload.meta().ttl())) {
      /// deleting a timeout kv doesn't need a record version
      /// concurrent issue (CPL: in-memory store -> rocksdb store)
      // deleteKV(key, VersionStore::kInvalidVersion);
      return utils::Status::notFound(key + " not found");
    }
    *outValue = data.singlevalue().value();
    *outVersion = payload.meta().version();
    return utils::Status::ok();
  } else if (s.IsNotFound()) {
    return utils::Status::notFound(key + " not found");
  } else {
    SPDLOG_ERROR("Error reading kv from RocksDB: {}.", s.ToString());
    return utils::Status::error(s.ToString());
  }
}

utils::Status RocksDBKVStore::deleteKV(
    const KeyType &key,
    const VersionType &deleteRecordVersion,
    const VersionType &deletedVersion,
    WSLookupFunc wsLookup) {
  auto cfHandle = findCFHandle(key, wsLookup);
  auto s = mWriteBatch.Delete(cfHandle, key);
  if (!s.ok()) {
    SPDLOG_ERROR("Error deleting key RocksDB: {}. Exiting...", s.ToString());
    assert(0);
  }
  this->onDeleteKey(key, deletedVersion, deleteRecordVersion);

  return utils::Status::ok();
}

utils::Status RocksDBKVStore::readMeta(const KeyType &key, proto::Meta* meta, WSLookupFunc wsLookup) {
  auto cfHandle = findCFHandle(key, wsLookup);
  RawValueType raw;
  auto s = mRocksDB->Get(mReadOptions, cfHandle, key, &raw);
  if (s.ok()) {
    return parseMeta(raw, meta);
  } else if (s.IsNotFound()) {
    return utils::Status::notFound(s.ToString());
  } else {
    SPDLOG_ERROR("Error reading meta in RocksDB: {}.", s.ToString());
    return utils::Status::error(s.ToString());
  }
}

utils::Status RocksDBKVStore::commit(const MilestoneType &milestone, WSLookupFunc wsLookup) {
  auto cfHandle = findCFHandle(RocksDBConf::kCFMetaDefault, wsLookup);
  auto s = mWriteBatch.Put(cfHandle, RocksDBConf::kPersistedMilestone, std::to_string(milestone));
  if (!s.ok()) {
    SPDLOG_ERROR("Error writing RocksDB: {}. Exiting...", s.ToString());
    assert(0);
  }
  return flushToRocksDB();
}

utils::Status RocksDBKVStore::loadMilestone(MilestoneType *milestone, WSLookupFunc wsLookup) {
  auto cfHandle = findCFHandle(RocksDBConf::kCFMetaDefault, wsLookup);
  RawValueType raw;
  auto s = mRocksDB->Get(mReadOptions, cfHandle, RocksDBConf::kPersistedMilestone, &raw);
  if (s.ok()) {
    /// crash if value is corrupted
    *milestone = std::stoull(raw);
    return utils::Status::ok();
  } else if (s.IsNotFound()) {
    return utils::Status::notFound(s.ToString());
  } else {
    return utils::Status::error(s.ToString());
  }
}

utils::Status RocksDBKVStore::flushToRocksDB() {
  /// TODO: accumulate large batch size
  rocksdb::WriteOptions writeOptions;
  writeOptions.sync = true;

  auto s = mRocksDB->Write(writeOptions, &mWriteBatch);
  if (!s.ok()) {
    SPDLOG_ERROR("failed to write RocksDB, reason: {}", s.ToString());
    assert(0);
  }
  /// clear write batch since we will reuse it.
  mWriteBatch.Clear();

  /// metrics, rocksdb total size
  mRocksdbTotalDataSize.set(mRocksDB->GetDBOptions().sst_file_manager->GetTotalSize());
  /// metrics, rocksdb estimated key num
  mRocksdbEstimatedKeyNum.set(size());

  return utils::Status::ok();
}

utils::Status RocksDBKVStore::testCompact() {
  rocksdb::Status result;
  for (auto handle : mColumnFamilyHandles) {
    result = mRocksDB->CompactRange(rocksdb::CompactRangeOptions(), handle, nullptr, nullptr);
    if (!result.ok()) {
      break;
    }
  }
  return result.ok()? utils::Status::ok() : utils::Status::error(result.ToString());
}

void RocksDBKVStore::clear() {
  /// not supported
  assert(0);
}

utils::Status RocksDBKVStore::flush() {
  return flushToRocksDB();
}

uint64_t RocksDBKVStore::size() const {
  uint64_t rocksdbEstimatedKeyNum = 0;

  if (mColumnFamilyHandles.empty()) {
    return 0;
  }

  for (int i = 1; i < mColumnFamilyHandles.size(); ++i) {
    uint64_t tempNum = 0;
    if (mRocksDB->GetIntProperty(mColumnFamilyHandles[i], "rocksdb.estimate-num-keys", &tempNum)) {
      rocksdbEstimatedKeyNum += tempNum;
    } else {
      return 0;
    }
  }

  return rocksdbEstimatedKeyNum;
}

utils::Status RocksDBKVStore::open(
    const std::string &walDir,
    const std::string &dbDir,
    std::shared_ptr<rocksdb::DB> *dbPtr,
    std::vector<rocksdb::ColumnFamilyHandle *> *columnFamilyHandles) {
  /// db options
  rocksdb::DBOptions dbOptions;

  dbOptions.IncreaseParallelism();
  dbOptions.create_if_missing = true;
  dbOptions.create_missing_column_families = true;
  dbOptions.wal_dir = walDir;

  /// column family options
  rocksdb::ColumnFamilyOptions columnFamilyDefaultOptions;
  rocksdb::ColumnFamilyOptions columnFamilyOptions;

  /// default CompactionStyle for column family is kCompactionStyleLevel
  columnFamilyDefaultOptions.OptimizeLevelStyleCompaction();
  columnFamilyOptions.OptimizeLevelStyleCompaction();
  columnFamilyOptions.compaction_filter = mCompactionFilter.get();

  std::vector<rocksdb::ColumnFamilyDescriptor> columnFamilyDescriptors;
  columnFamilyDescriptors.emplace_back(RocksDBConf::kCFMetaDefault, columnFamilyDefaultOptions);
  for (auto name : mColumnFamilyNames) {
    columnFamilyDescriptors.emplace_back(name, columnFamilyOptions);
  }

  /// open DB
  auto ts1InNano = gringofts::TimeUtil::currentTimeInNanos();
  rocksdb::DB *db;
  auto status = rocksdb::DB::Open(dbOptions, dbDir,
                                  columnFamilyDescriptors, columnFamilyHandles, &db);
  auto ts2InNano = gringofts::TimeUtil::currentTimeInNanos();

  if ( !status.ok() ) {
    SPDLOG_ERROR("failed to open RocksDB: {}.", status.ToString());
  }
  assert(status.ok());
  (*dbPtr).reset(db);

  SPDLOG_INFO("open RocksDB, wal.dir: {}, db.dir: {}, timeCost: {}ms",
              walDir, dbDir, (ts2InNano - ts1InNano) / 1000.0 / 1000.0);
  return utils::Status::ok();
}

utils::Status RocksDBKVStore::close(
    std::shared_ptr<rocksdb::DB> *dbPtr,
    std::vector<rocksdb::ColumnFamilyHandle *> *columnFamilyHandles) {
  auto ts1InNano = gringofts::TimeUtil::currentTimeInNanos();

  /// close column families
  for (auto handle : *columnFamilyHandles) {
    delete handle;
  }

  /// close DB
  /// dbPtr should be the last shared_ptr pointing to DB, we leverage it to delete DB.
  (*dbPtr).reset();

  auto ts2InNano = gringofts::TimeUtil::currentTimeInNanos();
  SPDLOG_INFO("close RocksDB, timeCost: {}ms",
              (ts2InNano - ts1InNano) / 1000.0 / 1000.0);
  return utils::Status::ok();
}

std::string RocksDBKVStore::createCheckpoint(const std::string &baseDir) {
  auto ts1InNano = gringofts::TimeUtil::currentTimeInNanos();
  auto tmpPath = baseDir + "/" + std::to_string(ts1InNano);

  rocksdb::Checkpoint *checkpoint;
  auto status = rocksdb::Checkpoint::Create(mRocksDB.get(), &checkpoint);
  if (!status.ok()) {
    SPDLOG_ERROR("failed to create a checkpoint object: {}.", status.ToString());
    assert(0);
  }

  status = checkpoint->CreateCheckpoint(tmpPath);
  if (!status.ok()) {
    SPDLOG_ERROR("failed to create a checkpoint: {}.", status.ToString());
    assert(0);
  }

  auto ts2InNano = gringofts::TimeUtil::currentTimeInNanos();
  SPDLOG_INFO("create a checkpoint, timeCost: {}ms.", (ts2InNano - ts1InNano) / 1000000.0);

  std::shared_ptr<rocksdb::DB> rocksDB;
  std::vector<rocksdb::ColumnFamilyHandle *> columnFamilyHandles;
  uint64_t lastAppliedIndex = 0;

  /// open db
  open(tmpPath, tmpPath, &rocksDB, &columnFamilyHandles);

  /// load last flushed index from checkpoint
  std::string value;
  auto cfHandle = findCFHandle(RocksDBConf::kCFMetaDefault, nullptr);
  status = rocksDB->Get(mReadOptions, cfHandle, RocksDBConf::kPersistedMilestone, &value);

  if (status.ok()) {
    lastAppliedIndex = std::stoull(value);
  } else if (status.IsNotFound()) {
    lastAppliedIndex = 0;
  } else {
    SPDLOG_ERROR("Error in RocksDB: {}. Exiting...", status.ToString());
    assert(0);
  }

  /// close db
  close(&rocksDB, &columnFamilyHandles);

  /// rename checkpoint
  // @formatter:off
  auto checkpointPath = baseDir + "/" + std::to_string(lastAppliedIndex)
      + "." + std::to_string(ts1InNano) + ".checkpoint";
  // @formatter:on

  assert(::rename(tmpPath.c_str(), checkpointPath.c_str()) == 0);
  return checkpointPath;
}

RocksDBKVStore::RocksDBKVStore(
    const std::string &walDir,
    const std::string &dbDir,
    const std::vector<WSName> &cfs,
    WSLookupFunc defaultWSLookupFunc,
    gringofts::TestPointProcessor *processor) : RocksDBKVStore(walDir, dbDir, cfs, defaultWSLookupFunc) {
    mTPProcessor = processor;
}

}  /// namespace goblin::kvengine::store

