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


#include "InMemoryKVStore.h"

#include <spdlog/spdlog.h>

#include "VersionStore.h"

namespace goblin::kvengine::store {

InMemoryKVStore::InMemoryKVStore(std::shared_ptr<ReadOnlyKVStore> delegateKVStore) : ProxyKVStore(delegateKVStore) {
}

utils::Status InMemoryKVStore::open() {
  this->clear();
  return utils::Status::ok();
}

utils::Status InMemoryKVStore::close() {
  this->clear();
  return utils::Status::ok();
}

utils::Status InMemoryKVStore::writeKV(
    const KeyType &key,
    const ValueType &value,
    const VersionType &version,
    const utils::TimeType &updateTime,
    const goblin::proto::UserDefinedMeta &udfMeta,
    WSLookupFunc wsLookup) {
  mData[key] = std::tuple<ValueType, VersionType, TTLType, utils::TimeType, utils::TimeType>(
      value, version, INFINITE_TTL, NEVER_EXPIRE, updateTime);
  if (!udfMeta.uintfield().empty() || !udfMeta.strfield().empty()) {
    mUdfMetaMap[key] = udfMeta;
  }
  mDeletedData.erase(key);
  onWriteValue(key, value, version);
  return utils::Status::ok();
}

utils::Status InMemoryKVStore::writeTTLKV(const KeyType &key,
                                          const ValueType &value,
                                          const VersionType &version,
                                          const TTLType &ttl,
                                          const utils::TimeType &deadline,
                                          const utils::TimeType &updateTime,
                                          const goblin::proto::UserDefinedMeta &udfMeta,
                                          WSLookupFunc wsLookup) {
  mData[key] = std::tuple<ValueType, VersionType, TTLType, utils::TimeType, utils::TimeType>(
      value, version, ttl, deadline, updateTime);
  if (!udfMeta.uintfield().empty() || !udfMeta.strfield().empty()) {
    mUdfMetaMap[key] = udfMeta;
  }
  mDeletedData.erase(key);
  onWriteValue(key, value, version);
  return utils::Status::ok();
}

/// TODO: remove ttl kv in cache eviction when no get
utils::Status InMemoryKVStore::readKV(
    const KeyType &key,
    ValueType *outValue,
    TTLType *outTTL,
    VersionType *outVersion,
    WSLookupFunc wsLookup) {
  auto it = mData.find(key);
  if (it == mData.end()) {
    if (mDeletedData.find(key) != mDeletedData.end()) {
      *outVersion = mDeletedData[key];
      return utils::Status::notFound(key + " not found");
    } else {
      return mDelegateKVStore->readKV(key, outValue, outTTL, outVersion);
    }
  }

  auto &[value, version, ttl, deadline, updatetime] = it->second;
  if (isTimeOut(deadline, ttl)) {
    /// SPDLOG_INFO("debug: key: {}, deadline {}, ttl: {}", key, deadline, ttl);
    mData.erase(it);
    return utils::Status::notFound(key + " not found");
  }

  *outValue = value;
  *outTTL = ttl;
  *outVersion = version;
  onReadValue(key);
  return utils::Status::ok();
}

utils::Status InMemoryKVStore::readKVFromProxy(
    const KeyType &key,
    ValueType *outValue,
    TTLType *outTTL,
    VersionType *outVersion,
    WSLookupFunc wsLookup) {
  return mDelegateKVStore->readKV(key, outValue, outTTL, outVersion);
}

utils::Status InMemoryKVStore::deleteKV(
    const KeyType &key,
    const VersionType &deleteRecordVersion,
    const VersionType &deletedVersion,
    WSLookupFunc wsLookup) {
  auto it = mData.find(key);
  if (it == mData.end()) {
    return utils::Status::ok();
  }
  auto &[value, version, ttl, deadline, updatetime] = it->second;
  mData.erase(key);
  mUdfMetaMap.erase(key);
  mDeletedData.emplace(key, deleteRecordVersion);
  return utils::Status::ok();
}

utils::Status InMemoryKVStore::readMeta(const KeyType &key, proto::Meta *meta, WSLookupFunc wsLookup) {
  auto it = mData.find(key);
  if (it == mData.end()) {
    return mDelegateKVStore->readMeta(key, meta);
  }

  auto &[value, version, ttl, deadline, updatetime] = it->second;
  meta->set_version(version);
  meta->set_ttl(ttl);
  meta->set_deadline(deadline);
  meta->set_updatetime(updatetime);
  if (mUdfMetaMap.find(key) != mUdfMetaMap.end()) {
    auto &udfMeta = mUdfMetaMap[key];
    for (auto &val : udfMeta.uintfield()) {
      meta->mutable_udfmeta()->add_uintfield(val);
    }
    for (auto &str : udfMeta.strfield()) {
      meta->mutable_udfmeta()->add_strfield(str);
    }
  }
  onReadValue(key);
  return utils::Status::ok();
}

utils::Status InMemoryKVStore::readMetaFromProxy(
    const KeyType &key,
    proto::Meta *meta,
    WSLookupFunc wsLookup) {
  return mDelegateKVStore->readMeta(key, meta);
}

utils::Status InMemoryKVStore::commit(const MilestoneType &milestone, WSLookupFunc wsLookup) {
  /// not supported
  assert(0);
}

utils::Status InMemoryKVStore::loadMilestone(MilestoneType *milestone, WSLookupFunc wsLookup) {
  /// not supported
  assert(0);
}

void InMemoryKVStore::clear() {
  mData.clear();
  mUdfMetaMap.clear();
  mDeletedData.clear();
}

utils::Status InMemoryKVStore::evictKV(
    const std::set<store::KeyType> &keys,
    store::VersionType guardVersion,
    WSLookupFunc wsLookup) {
  for (auto key : keys) {
    store::ValueType value;
    store::TTLType ttl;
    store::VersionType version;
    utils::Status s = readKV(key, &value, &ttl, &version);
    if (s.isOK()) {
      if (version > guardVersion) {
        SPDLOG_WARN("for key {}, evict version {} is older than current version {}, so skip it",
                    key,
                    guardVersion,
                    version);
        continue;
      }
    } else {
      SPDLOG_ERROR("failed to read key {} with evict version {}, so skip it, msg {}",
          key,
          guardVersion,
          s.getDetail());
      continue;
    }
    mData.erase(key);
    mUdfMetaMap.erase(key);
    // remove deleted data
    // then read will forward to rocksdb
    mDeletedData.erase(key);
  }
  onEvictKeys(keys);
  return utils::Status::ok();
}

}  /// namespace goblin::kvengine::store

