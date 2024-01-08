/************************************************************************
Copyright 2020-2021 eBay Inc.
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

#ifndef SERVER_SRC_ADMIN_OLDROCKSDBKVSTORE_H_
#define SERVER_SRC_ADMIN_OLDROCKSDBKVSTORE_H_

#include <memory>
#include <string>
#include <vector>

#include <infra/monitor/MonitorCenter.h>
#include <infra/util/TestPointProcessor.h>
#include <infra/util/Util.h>
#include <rocksdb/compaction_filter.h>
#include <rocksdb/db.h>
#include <rocksdb/env.h>
#include <rocksdb/options.h>
#include <rocksdb/sst_file_manager.h>
#include <rocksdb/utilities/checkpoint.h>

#include "../../../protocols/generated/oldstorage.pb.h"
#include "../kv-engine/store/SnapshottedKVStore.h"
#include "OldKVStore.h"

namespace goblin {
namespace mock {
template <typename ClusterType>
class MockAppCluster;
}
}

namespace goblin::kvengine::store {

class RocksDBKVStoreTest;

class OldRocksDBKVStore final: public OldKVStore, std::enable_shared_from_this<OldRocksDBKVStore> {
 public:
  struct RocksDBConf {
    // key name for version
    static constexpr const char *kPersistedMilestone = "persistedMilestone";

    /**
     * ColumnFamily Names
     */
    static constexpr const char *kCFMetaDefault = "default";
  };

  OldRocksDBKVStore(
      const std::string &walDir,
      const std::string &dbDir,
      const std::vector<WSName> &cfs,
      WSLookupFunc defaultWSLookupFunc);
  ~OldRocksDBKVStore() override;

  utils::Status open() override;
  utils::Status close() override;

  utils::Status writeKV(const KeyType &key,
                        const ValueType &value,
                        const VersionType &version,
                        WSLookupFunc wsLookup = nullptr) override;
  utils::Status writeTTLKV(const KeyType &key,
                           const ValueType &value,
                           const VersionType &version,
                           const TTLType &ttl,
                           const utils::TimeType& deadline,
                           WSLookupFunc wsLookup = nullptr) override;
  utils::Status readKV(const KeyType &key,
                       ValueType *outValue,
                       TTLType *outTTL,
                       VersionType *outVersion,
                       WSLookupFunc wsLookup = nullptr) override;
  utils::Status deleteKV(const KeyType &key, WSLookupFunc wsLookup = nullptr) override;
  utils::Status readMeta(const KeyType &key, proto::old::Meta *meta, WSLookupFunc wsLookup = nullptr) override;
  utils::Status commit(const MilestoneType &milestone, WSLookupFunc wsLookup = nullptr) override;
  utils::Status loadMilestone(MilestoneType *milestone, WSLookupFunc wsLookup = nullptr) override;

  void clear() override;

  std::shared_ptr<SnapshottedKVStore> takeSnapshot() override {
    return std::make_shared<RocksDBSnapshottedKVStore>(this);
  }

 protected:
  class TTLCompactionFilter : public rocksdb::CompactionFilter {
   public:
    TTLCompactionFilter() {}
    bool Filter(int level, const rocksdb::Slice& key, const rocksdb::Slice& value,
                std::string* new_value, bool* value_changed) const override {
      if (reinterpret_cast<const Header*>(value.data())->mType != DataType::NORMAL_WITH_TTL) {
        return false;
      }
      proto::old::Meta meta;
      utils::Status s = parseMeta(value.data(), value.size(), &meta);
      return s.isOK() && isTimeOut(meta.deadline(), meta.ttl());
    }

    const char* Name() const override { return "TTLCompactionFilter"; }
  };
  class RocksDBSnapshottedKVStore final:
    public SnapshottedKVStore, std::enable_shared_from_this<RocksDBSnapshottedKVStore> {
   public:
     class RocksDBKVStoreIterator final: public KVIterator {
      public:
         RocksDBKVStoreIterator(RocksDBSnapshottedKVStore *source,
             rocksdb::ColumnFamilyHandle* handle): mSnapshotKVStore(source) {
           rocksdb::ReadOptions readOptions;
           readOptions.snapshot = mSnapshotKVStore->mSnapshot;
           mIt = mSnapshotKVStore->mOrigin->mRocksDB->NewIterator(readOptions, handle);
         }
         ~RocksDBKVStoreIterator() {
           delete mIt;
         }
         utils::Status seekToBegin() override {
           mIt->SeekToFirst();
           auto s = utils::Status::ok();
           if (!mIt->Valid()) {
             s = utils::Status::error(mIt->status().ToString());
           }
           return s;
         }
         utils::Status next() override {
           mIt->Next();
           auto s = utils::Status::ok();
           if (!mIt->Valid()) {
             s = utils::Status::error(mIt->status().ToString());
           }
           return s;
         }
         utils::Status prev() override {
           mIt->Prev();
           auto s = utils::Status::ok();
           if (!mIt->Valid()) {
             s = utils::Status::error(mIt->status().ToString());
           }
           return s;
         }
         utils::Status get(std::tuple<KeyType, ValueType, TTLType, VersionType> &kv) override {
           /// SPDLOG_INFO("debug: getting from iterator {}", mIt->Valid());
           if (!mIt->Valid()) {
             return utils::Status::error(mIt->status().ToString());
           }
           auto &[key, value, ttl, version] = kv;
           key = mIt->key().ToString();
           auto raw = mIt->value().ToString();
           /// TODO: support ttl
           fromRawValue(raw, &value, &ttl, &version);
           return utils::Status::ok();
         }
         utils::Status seekTo(const KeyType &key) override {
           mIt->Seek(key);
           auto s = utils::Status::ok();
           if (!mIt->Valid()) {
             s = utils::Status::error(mIt->status().ToString());
           }
           return s;
         }
         bool hasValue() override {
           assert(mIt->status().ok());
           return mIt->Valid();
         }

      private:
         rocksdb::Iterator* mIt = nullptr;
         /// keep this source in case it is invalidated
         RocksDBSnapshottedKVStore *mSnapshotKVStore;
     };

     explicit RocksDBSnapshottedKVStore(OldRocksDBKVStore *origin): SnapshottedKVStore(), mOrigin(origin) {
       mSnapshot = mOrigin->mRocksDB->GetSnapshot();
     }
     ~RocksDBSnapshottedKVStore() {
       if (mSnapshot != nullptr) {
         mOrigin->mRocksDB->ReleaseSnapshot(mSnapshot);
       }
     }
     std::shared_ptr<KVIterator> newIterator(WSName wsName) override {
       auto handle = mOrigin->findCFHandle(wsName);
       return std::make_shared<RocksDBKVStoreIterator>(this, handle);
     }

   private:
     OldRocksDBKVStore *mOrigin;
     const rocksdb::Snapshot* mSnapshot = nullptr;
  };

 private:
  using RawValueType = std::string;
  /// each raw value in rocksdb will be Header+Meta+Data
  enum DataType {
    NORMAL = 100,
    NORMAL_WITH_TTL = 101,
    OTHERS = 126
  };
  struct Header {
    char mType;
    char mMetaLen;
  };

  utils::Status flushToRocksDB();
  static void toRawValue(const ValueType &value, const VersionType &version, RawValueType *outRaw);
  static void toRawValue(
      const ValueType &value,
      const VersionType &version,
      const TTLType &ttl,
      const utils::TimeType& deadline,
      RawValueType *outRaw);
  static utils::Status fromRawValue(
      const RawValueType &raw,
      ValueType *outValue,
      TTLType *outTTL,
      VersionType *outVersion);
  static utils::Status parseMeta(const char* data, size_t size, proto::old::Meta* meta);
  rocksdb::ColumnFamilyHandle* findCFHandle(
      const KeyType &key,
      WSLookupFunc wsLookup);
  rocksdb::ColumnFamilyHandle* findCFHandle(WSName wsName);

  std::string mWalDir;
  std::string mDBDir;
  WSLookupFunc mDefaultWSLookupFunc;
  std::unique_ptr<rocksdb::DB> mRocksDB;
  std::vector<WSName> mColumnFamilyNames;
  std::vector<rocksdb::ColumnFamilyHandle *> mColumnFamilyHandles;
  rocksdb::WriteBatch mWriteBatch;
  rocksdb::ReadOptions mReadOptions;
  std::unique_ptr<TTLCompactionFilter> mCompactionFilter;

  /// metrics, rocksdb total size
  santiago::MetricsCenter::GaugeType mRocksdbTotalDataSize;

  /// UT
  utils::Status testCompact();
  FRIEND_TEST(RocksDBKVStoreTest, CompactionFilterTest);
  OldRocksDBKVStore(
      const std::string &walDir,
      const std::string &dbDir,
      const std::vector<WSName> &cfs,
      WSLookupFunc defaultWSLookupFunc,
      gringofts::TestPointProcessor *processor);
  gringofts::TestPointProcessor *mTPProcessor = nullptr;
  template <typename ClusterType>
  friend class mock::MockAppCluster;
  friend class RocksDBKVStoreTest;
};

}  /// namespace goblin::kvengine::store

#endif  // SERVER_SRC_ADMIN_OLDROCKSDBKVSTORE_H_


