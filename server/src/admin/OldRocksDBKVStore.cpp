/**
 * Copyright (c) 2020 eBay Software Foundation. All rights reserved.
 */

#include <infra/monitor/MonitorTypes.h>
#include <absl/strings/str_cat.h>
#include <rocksdb/db.h>
#include <spdlog/spdlog.h>

#include "../kv-engine/utils/TimeUtil.h"
#include "../kv-engine/utils/TPRegistryEx.h"
#include "OldRocksDBKVStore.h"

namespace goblin::kvengine::store {

OldRocksDBKVStore::OldRocksDBKVStore(
    const std::string &walDir,
    const std::string &dbDir,
    const std::vector<WSName> &cfs,
    WSLookupFunc defaultWSLookupFunc):
  mWalDir(walDir),
  mDBDir(dbDir),
  mDefaultWSLookupFunc(defaultWSLookupFunc),
  mRocksdbTotalDataSize(gringofts::getGauge("rocksdb_total_data_size", {})) {
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

OldRocksDBKVStore::~OldRocksDBKVStore() {
  assert(close().isOK());
}

utils::Status OldRocksDBKVStore::open() {
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

  SPDLOG_INFO("open RocksDB, wal.dir: {}, db.dir: {}, timeCost: {}ms",
              mWalDir, mDBDir, (ts2InNano - ts1InNano) / 1000.0 / 1000.0);

  return utils::Status::ok();
}

utils::Status OldRocksDBKVStore::close() {
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

void OldRocksDBKVStore::toRawValue(const ValueType &value, const VersionType &version, RawValueType *outRaw) {
  using goblin::proto::old::Meta;
  Meta meta;
  meta.set_version(version);

  const uint32_t headerLen = sizeof(Header);
  Header header{};
  header.mType = DataType::NORMAL;
  header.mMetaLen = meta.ByteSizeLong();

  *outRaw = std::string(reinterpret_cast<const char*>(&header), headerLen);
  *outRaw += meta.SerializeAsString();
  *outRaw += value;
}

void OldRocksDBKVStore::toRawValue(
    const ValueType &value,
    const VersionType &version,
    const TTLType &ttl,
    const utils::TimeType& deadline,
    RawValueType *outRaw) {
  using goblin::proto::old::Meta;
  Meta meta;
  meta.set_version(version);
  meta.set_ttl(ttl);
  meta.set_deadline(deadline);

  const uint32_t headerLen = sizeof(Header);
  Header header{};
  header.mType = DataType::NORMAL_WITH_TTL;
  header.mMetaLen = meta.ByteSizeLong();

  /// Maybe better ways?
  *outRaw = std::string(reinterpret_cast<const char*>(&header), headerLen);
  *outRaw += meta.SerializeAsString();
  *outRaw += value;
}

utils::Status OldRocksDBKVStore::fromRawValue(
    const RawValueType &raw,
    ValueType *outValue,
    TTLType *outTTL,
    VersionType *outVersion) {
  goblin::proto::old::Meta meta;
  utils::Status s = parseMeta(raw.data(), raw.size(), &meta);
  if (!s.isOK()) {
    return s;
  }
  if (isTimeOut(meta.deadline(), meta.ttl())) {
    return utils::Status::expired("ttl expired");
  }
  *outTTL = meta.ttl();
  *outVersion = meta.version();
  *outValue = raw.substr(sizeof(Header) + meta.ByteSizeLong());
  return utils::Status::ok();
}

utils::Status OldRocksDBKVStore::parseMeta(const char* data, size_t size, goblin::proto::old::Meta* meta) {
  const uint32_t headerLen = sizeof(Header);
  if (size <= headerLen) {
    SPDLOG_ERROR("corrupted data, data size: {}, expected header size: {}", size, headerLen);
    return utils::Status::error("corrupted header size");
  }
  const auto *header = reinterpret_cast<const Header*>(data);
  if (header->mType != DataType::NORMAL && header->mType != DataType::NORMAL_WITH_TTL) {
    SPDLOG_ERROR("invalid data type: {}", header->mType);
    return utils::Status::error("invalid type in header");
  }
  meta->ParseFromArray(reinterpret_cast<const char*>(data + headerLen), header->mMetaLen);
  return utils::Status::ok();
}

rocksdb::ColumnFamilyHandle* OldRocksDBKVStore::findCFHandle(
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
    return mColumnFamilyHandles[index];
  }
}

rocksdb::ColumnFamilyHandle* OldRocksDBKVStore::findCFHandle(WSName wsName) {
  if (wsName == RocksDBConf::kCFMetaDefault) {
    return mColumnFamilyHandles[0];
  } else {
    auto it = std::find(mColumnFamilyNames.begin(), mColumnFamilyNames.end(), wsName);
    assert(it != mColumnFamilyNames.end());
    int index = std::distance(mColumnFamilyNames.begin(), it);
    return mColumnFamilyHandles[index];
  }
}

utils::Status OldRocksDBKVStore::writeKV(
    const KeyType &key,
    const ValueType &value,
    const VersionType &version,
    WSLookupFunc wsLookup) {
  auto cfHandle = findCFHandle(key, wsLookup);
  RawValueType raw;
  toRawValue(value, version, &raw);
  auto s = mWriteBatch.Put(cfHandle, key, raw);
  if (!s.ok()) {
    SPDLOG_ERROR("Error writing RocksDB: {}. Exiting...", s.ToString());
    assert(0);
  }
  return utils::Status::ok();
}

utils::Status OldRocksDBKVStore::writeTTLKV(
    const KeyType &key,
    const ValueType &value,
    const VersionType &version,
    const TTLType &ttl,
    const utils::TimeType& deadline, WSLookupFunc wsLookup) {
  auto cfHandle = findCFHandle(key, wsLookup);
  RawValueType raw;
  toRawValue(value, version, ttl, deadline, &raw);

  auto s = mWriteBatch.Put(cfHandle, key, raw);
  if (!s.ok()) {
    SPDLOG_ERROR("Error writing RocksDB: {}. Exiting...", s.ToString());
    assert(0);
  }
  return utils::Status::ok();
}

utils::Status OldRocksDBKVStore::readKV(
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
    if (fromRawValue(raw, outValue, outTTL, outVersion).isExpired()) {
      deleteKV(key);
      return utils::Status::notFound();
    }
    return utils::Status::ok();
  } else if (s.IsNotFound()) {
    return utils::Status::notFound(s.ToString());
  } else {
    SPDLOG_ERROR("Error reading kv from RocksDB: {}.", s.ToString());
    return utils::Status::error(s.ToString());
  }
}

utils::Status OldRocksDBKVStore::deleteKV(const KeyType &key, WSLookupFunc wsLookup) {
  auto cfHandle = findCFHandle(key, wsLookup);
  auto s = mWriteBatch.Delete(cfHandle, key);
  if (!s.ok()) {
    SPDLOG_ERROR("Error deleting key RocksDB: {}. Exiting...", s.ToString());
    assert(0);
  }
  return utils::Status::ok();
}

utils::Status OldRocksDBKVStore::readMeta(const KeyType &key, proto::old::Meta* meta, WSLookupFunc wsLookup) {
  auto cfHandle = findCFHandle(key, wsLookup);
  RawValueType raw;
  auto s = mRocksDB->Get(mReadOptions, cfHandle, key, &raw);
  if (s.ok()) {
    return parseMeta(raw.data(), raw.size(), meta);
  } else if (s.IsNotFound()) {
    return utils::Status::notFound(s.ToString());
  } else {
    SPDLOG_ERROR("Error reading meta in RocksDB: {}.", s.ToString());
    return utils::Status::error(s.ToString());
  }
}

utils::Status OldRocksDBKVStore::commit(const MilestoneType &milestone, WSLookupFunc wsLookup) {
  auto cfHandle = findCFHandle(RocksDBConf::kCFMetaDefault, wsLookup);
  auto s = mWriteBatch.Put(cfHandle, RocksDBConf::kPersistedMilestone, std::to_string(milestone));
  if (!s.ok()) {
    SPDLOG_ERROR("Error writing RocksDB: {}. Exiting...", s.ToString());
    assert(0);
  }
  return flushToRocksDB();
}

utils::Status OldRocksDBKVStore::loadMilestone(MilestoneType *milestone, WSLookupFunc wsLookup) {
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

utils::Status OldRocksDBKVStore::flushToRocksDB() {
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

  return utils::Status::ok();
}

utils::Status OldRocksDBKVStore::testCompact() {
  rocksdb::Status result;
  for (auto handle : mColumnFamilyHandles) {
    result = mRocksDB->CompactRange(rocksdb::CompactRangeOptions(), handle, nullptr, nullptr);
    if (!result.ok()) {
      break;
    }
  }
  return result.ok()? utils::Status::ok() : utils::Status::error(result.ToString());
}

void OldRocksDBKVStore::clear() {
  /// not supported
  assert(0);
}

OldRocksDBKVStore::OldRocksDBKVStore(
    const std::string &walDir,
    const std::string &dbDir,
    const std::vector<WSName> &cfs,
    WSLookupFunc defaultWSLookupFunc,
    gringofts::TestPointProcessor *processor) : OldRocksDBKVStore(walDir, dbDir, cfs, defaultWSLookupFunc) {
    mTPProcessor = processor;
}

}  ///  namespace goblin::kvengine::store

