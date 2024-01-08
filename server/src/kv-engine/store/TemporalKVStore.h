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

#ifndef SERVER_SRC_KV_ENGINE_STORE_TEMPORALKVSTORE_H_
#define SERVER_SRC_KV_ENGINE_STORE_TEMPORALKVSTORE_H_

#include <map>
#include <set>

#include "ProxyKVStore.h"
#include "ReadOnlyKVStore.h"

namespace goblin::kvengine::store {

class TemporalKVStore: public ProxyKVStore {
 public:
  explicit TemporalKVStore(std::shared_ptr<ReadOnlyKVStore> delegateKVStore);
  ~TemporalKVStore() override;

  utils::Status open() override;
  utils::Status close() override;

  utils::Status writeKV(const KeyType &key,
                        const ValueType &value,
                        const VersionType &version,
                        const utils::TimeType &updateTime = store::NOT_UPDATED,
                        const goblin::proto::UserDefinedMeta &udfMeta = store::DEFAULT_UDFMETA,
                        WSLookupFunc wsLookup = nullptr) override;
  utils::Status writeTTLKV(const KeyType &key,
                           const ValueType &value,
                           const VersionType &version,
                           const TTLType &ttl,
                           const utils::TimeType& deadline,
                           const utils::TimeType &updateTime = store::NOT_UPDATED,
                           const goblin::proto::UserDefinedMeta &udfMeta = store::DEFAULT_UDFMETA,
                           WSLookupFunc wsLookup = nullptr) override;
  utils::Status readKV(const KeyType &key,
                       ValueType *outValue,
                       TTLType *outTTL,
                       VersionType *outVersion,
                       WSLookupFunc wsLookup = nullptr) override;
  utils::Status readKVFromProxy(const KeyType &key,
                       ValueType *outValue,
                       TTLType *outTTL,
                       VersionType *outVersion,
                       WSLookupFunc wsLookup = nullptr) override;
  utils::Status deleteKV(
      const KeyType &key,
      const VersionType &deleteRecordVersion,
      const VersionType &deletedVersion,
      WSLookupFunc wsLookup = nullptr) override;

  utils::Status commit(const MilestoneType &milestone, WSLookupFunc wsLookup = nullptr) override;

  void clear() override;

  utils::Status copyTo(const std::shared_ptr<KVStore> &kvStore);

 private:
  std::map<KeyType, std::tuple<ValueType, VersionType, TTLType, utils::TimeType, utils::TimeType>> mData;
  std::unordered_map<KeyType, goblin::proto::UserDefinedMeta> mUdfMetaMap;
//   std::unordered_map<KeyType, std::vector<uint64_t>, std::hash<KeyType>> mUintFieldMap;
//   std::unordered_map<KeyType, std::vector<std::string>, std::hash<KeyType>> mStrFieldMap;
  std::set<KeyType> mDeletedData;
};

}  /// namespace goblin::kvengine::store

#endif  // SERVER_SRC_KV_ENGINE_STORE_TEMPORALKVSTORE_H_

