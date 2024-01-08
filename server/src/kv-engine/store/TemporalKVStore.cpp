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


#include "TemporalKVStore.h"

#include "VersionStore.h"

namespace goblin::kvengine::store {

TemporalKVStore::TemporalKVStore(std::shared_ptr<ReadOnlyKVStore> delegateKVStore): ProxyKVStore(delegateKVStore) {
}

TemporalKVStore::~TemporalKVStore() {
}

utils::Status TemporalKVStore::open() {
  this->clear();
  return utils::Status::ok();
}

utils::Status TemporalKVStore::close() {
  this->clear();
  return utils::Status::ok();
}

utils::Status TemporalKVStore::writeKV(
    const KeyType &key,
    const ValueType &value,
    const VersionType &version,
    const utils::TimeType &updateTime,
    const goblin::proto::UserDefinedMeta &udfMeta,
    WSLookupFunc wsLookup) {
  return writeTTLKV(key, value, version, INFINITE_TTL, NEVER_EXPIRE,
                    updateTime, udfMeta, wsLookup);
}

utils::Status TemporalKVStore::writeTTLKV(
    const KeyType &key,
    const ValueType &value,
    const VersionType &version,
    const TTLType &ttl,
    const utils::TimeType& deadline,
    const utils::TimeType &updateTime,
    const goblin::proto::UserDefinedMeta &udfMeta,
    WSLookupFunc wsLookup) {
  mData[key] = std::tuple<ValueType, VersionType, TTLType, utils::TimeType, utils::TimeType>(
      value, version, ttl, deadline, updateTime);
  if (!udfMeta.uintfield().empty() || !udfMeta.strfield().empty()) {
    mUdfMetaMap[key] = udfMeta;
  }
  mDeletedData.erase(key);
  return utils::Status::ok();
}

utils::Status TemporalKVStore::readKV(
    const KeyType &key,
    ValueType *outValue,
    TTLType *outTTL,
    VersionType *outVersion,
    WSLookupFunc wsLookup) {
  auto it = mData.find(key);
  if (it == mData.end()) {
    if (mDeletedData.find(key) != mDeletedData.end()) {
      return utils::Status::notFound(key + " not found");
    }
    return mDelegateKVStore->readKV(key, outValue, outTTL, outVersion);
  }
  auto &[value, version, ttl, deadline, updateTime] = it->second;
  if (isTimeOut(deadline, ttl)) {
    mData.erase(it);
    return utils::Status::notFound(key + " not found");
  }

  *outValue = value;
  *outTTL = ttl;
  *outVersion = version;
  return utils::Status::ok();
}

utils::Status TemporalKVStore::readKVFromProxy(
    const KeyType &key,
    ValueType *outValue,
    TTLType *outTTL,
    VersionType *outVersion,
    WSLookupFunc wsLookup) {
  return mDelegateKVStore->readKV(key, outValue, outTTL, outVersion);
}

utils::Status TemporalKVStore::deleteKV(
    const KeyType &key,
    const VersionType &deleteRecordVersion,
    const VersionType &deletedVersion,
    WSLookupFunc wsLookup) {
  mData.erase(key);
  mUdfMetaMap.erase(key);
  mDeletedData.insert(key);
  return utils::Status::ok();
}

utils::Status TemporalKVStore::commit(const MilestoneType &milestone, WSLookupFunc wsLookup) {
  /// not supported
  assert(0);
}

void TemporalKVStore::clear() {
  mData.clear();
  mUdfMetaMap.clear();
  mDeletedData.clear();
}

utils::Status TemporalKVStore::copyTo(const std::shared_ptr<KVStore> &kvStore) {
  utils::Status s = utils::Status::ok();
  for (auto &[key, tuple] : mData) {
    auto &[value, version, ttl, deadline, updateTime] = tuple;
    goblin::proto::UserDefinedMeta udfMeta;
    if (mUdfMetaMap.find(key) != mUdfMetaMap.end()) {
      udfMeta = mUdfMetaMap[key];
    }
    if (ttl == store::INFINITE_TTL) {
      s = kvStore->writeKV(key, value, version, updateTime, udfMeta);
    } else {
      s = kvStore->writeTTLKV(key, value, version, ttl, deadline, updateTime, udfMeta);
    }
    if (!s.isOK()) {
      return s;
    }
  }
  for (auto &key : mDeletedData) {
    s = kvStore->deleteKV(key, VersionStore::kInvalidVersion, VersionStore::kInvalidVersion);
    if (!s.isOK()) {
      return s;
    }
  }
  return s;
}

}  /// namespace goblin::kvengine::store

