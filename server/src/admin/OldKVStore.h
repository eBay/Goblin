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

#ifndef SERVER_SRC_ADMIN_OLDKVSTORE_H_
#define SERVER_SRC_ADMIN_OLDKVSTORE_H_

#include <functional>
#include <string>

#include "../../../protocols/generated/oldstorage.pb.h"
#include "../kv-engine/types.h"
#include "../kv-engine/utils/Status.h"
#include "../kv-engine/utils/TimeUtil.h"
#include "../kv-engine/store/Iterator.h"

namespace goblin::kvengine::store {

/// the interface to write/read kv
/// it doesn't gurantee to be thread-safe
class OldKVStore {
 public:
  OldKVStore() = default;
  OldKVStore &operator=(const OldKVStore &) = delete;

  virtual ~OldKVStore() = default;

  virtual utils::Status open() = 0;
  virtual utils::Status close() = 0;

  virtual utils::Status lock(const std::set<KeyType> &targetKeys,
      bool isExclusive,
      WSLookupFunc wsLookup = nullptr) {
    /// by default, a kv store does nothing
    return utils::Status::ok();
  }
  virtual utils::Status lockWS(const WSName &targetWS,
      bool isExclusive) {
    /// by default, a kv store does nothing
    return utils::Status::ok();
  }
  virtual utils::Status unlock(const std::set<KeyType> &targetKeys,
      bool isExclusive,
      WSLookupFunc wsLookup = nullptr) {
    /// by default, a kv store does nothing
    return utils::Status::ok();
  }
  virtual utils::Status unlockWS(const WSName &targetWS,
      bool isExclusive) {
    /// by default, a kv store does nothing
    return utils::Status::ok();
  }

  virtual utils::Status writeKV(const KeyType &key,
                                const ValueType &value,
                                const VersionType &version,
                                WSLookupFunc wsLookup = nullptr) = 0;
  virtual utils::Status writeTTLKV(const KeyType &key,
                                   const ValueType &value,
                                   const VersionType &version,
                                   const TTLType &ttl,
                                   const utils::TimeType &deadline,
                                   WSLookupFunc wsLookup = nullptr) = 0;

  virtual utils::Status readKV(const KeyType &key,
                               ValueType *outValue,
                               TTLType *outTTL,
                               VersionType *outVersion,
                               WSLookupFunc wsLookup = nullptr) = 0;
  virtual utils::Status deleteKV(const KeyType &key,
                                 WSLookupFunc wsLookup = nullptr) = 0;
  virtual utils::Status readMeta(const KeyType &key,
                                 proto::old::Meta *meta,
                                 WSLookupFunc wsLookup = nullptr) = 0;

  /// flush all changes before this milestone
  virtual utils::Status commit(const MilestoneType &milestone,
                               WSLookupFunc wsLookup = nullptr) = 0;

  /// load milestone, typically for recovery
  virtual utils::Status loadMilestone(MilestoneType *milestone,
                                      WSLookupFunc wsLookup = nullptr) = 0;

  virtual void clear() = 0;

  /**
   * the observer will be notified whenever a read/write op is performed
   */
  virtual utils::Status registerObserver(const std::shared_ptr<KVObserver> &observer,
                                         WSLookupFunc wsLookup = nullptr) {
    observers.push_back(observer);
    return utils::Status::ok();
  }

  virtual utils::Status evictKV(const std::set<store::KeyType> &keys,
                                store::VersionType guardVersion,
                                WSLookupFunc wsLookup = nullptr) {
    assert(0);
  }

  virtual std::shared_ptr<SnapshottedKVStore> takeSnapshot() {
    /// by default, a kv store doesn't support snapshot
    assert(0);
  }

  virtual std::shared_ptr<KVIterator> newIterator(WSName wsName) {
    /// by default, a kv store doesn't support iterator
    assert(0);
  }

 protected:
  std::vector<std::shared_ptr<KVObserver>> observers;
  static bool isTimeOut(const utils::TimeType &deadline, const TTLType &ttl) {
    return ttl != INFINITE_TTL && deadline <= utils::TimeUtil::secondsSinceEpoch();
  }

  void onReadValue(const KeyType &key) const {
    for (auto &observer : observers) observer->onReadValue(key);
  }
  void onWriteValue(const KeyType &key, const ValueType &value, const VersionType &version) const {
    for (auto &observer : observers) observer->onWriteValue(key, value, version);
  }
  void onEvictKeys(const std::set<store::KeyType> &keys) const {
    for (auto &observer : observers) observer->onEvictKeys(keys);
  }
};

}  /// namespace goblin::kvengine::store

#endif  // SERVER_SRC_ADMIN_OLDKVSTORE_H_

