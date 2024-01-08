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

#include <boost/uuid/uuid.hpp>             // uuid class
#include <boost/uuid/uuid_generators.hpp>  // generators
#include <boost/uuid/uuid_io.hpp>          // streaming operators etc.

#include "WatchCenter.h"
#include "../../kv-engine/utils/Util.h"

namespace goblin::objectstore::watch {
  std::string WatchCenter::registerWatch(const std::vector<kvengine::store::KeyType>& keyVector) {
    auto watchId = WatchCenter::allocateId();
    std::unique_lock<std::shared_mutex> wLock(mSharedMutex);

    for (auto& key : keyVector) {
      SPDLOG_INFO("registerWatch, add watch id {} for key {}", watchId, key);
      mKey2WatchEntryMap[key].push_back(std::make_unique<WatchEntryWrap>(watchId));
    }
    mWatchId2KeyMap[watchId] = keyVector;
    return watchId;
  }

  bool WatchCenter::unRegisterWatch(const WatchIdType &watchId) {
    std::unique_lock<std::shared_mutex> wLock(mSharedMutex);
    if (mWatchId2KeyMap.find(watchId) == mWatchId2KeyMap.end()) {
      SPDLOG_WARN("watch id {} not found in watch center", watchId);
      return false;
    }

    for (auto &key : mWatchId2KeyMap[watchId]) {
      SPDLOG_INFO("unRegisterWatch - watch id {} for key {}", watchId, key);
      auto &vec = mKey2WatchEntryMap[key];
      for (auto iter = vec.begin(); iter != vec.end(); ++iter) {
        if (iter->get()->getWatchId() == watchId) {
          vec.erase(iter);
          break;
        }
      }
      if (vec.empty()) {
        mKey2WatchEntryMap.erase(key);
      }
    }
    mWatchId2KeyMap.erase(watchId);
    return true;
  }

  bool WatchCenter::purgeWatchValue(const WatchIdType& watchId, std::vector<std::unique_ptr<WatchEntry>> *pVec) {
    std::shared_lock<std::shared_mutex> rLock(mSharedMutex);
    if (mWatchId2KeyMap.find(watchId) == mWatchId2KeyMap.end()) {
      SPDLOG_WARN("WatchCenter::purgeWatchValue, watch id {} not found", watchId);
      return false;
    }

    // SPDLOG_INFO("debug: watchid2key map size {}", mWatchId2KeyMap[watchId].size());
    for (auto &key : mWatchId2KeyMap[watchId]) {
      // SPDLOG_INFO("debug: key2watchentry map size {}", mKey2WatchEntryMap[key].size());
      for (auto &e : mKey2WatchEntryMap[key]) {
        if (e->getWatchId() == watchId) {
          e->purgeValueChange(pVec);
        }
      }
    }
    return true;
  }

  void WatchCenter::onWriteValue(
      const kvengine::store::KeyType &key,
      const kvengine::store::ValueType &value,
      const kvengine::store::VersionType &version) {
    std::shared_lock<std::shared_mutex> rLock(mSharedMutex);
    if (mKey2WatchEntryMap.find(key) == mKey2WatchEntryMap.end()) {
      return;
    }

    for (auto &s : mKey2WatchEntryMap[key]) {
      s->valueChange(key, value, version);
    }
  }

  void WatchCenter::onDeleteKey(const kvengine::store::KeyType &key,
      const kvengine::store::VersionType &version,
      const kvengine::store::VersionType &deleteRecordVersion) {
    std::shared_lock<std::shared_mutex> rLock(mSharedMutex);
    if (mKey2WatchEntryMap.find(key) == mKey2WatchEntryMap.end()) {
      return;
    }
    for (auto &s : mKey2WatchEntryMap[key]) {
      s->keyDeleted(key, version, deleteRecordVersion);
    }
  }

  WatchIdType WatchCenter::allocateId() {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return boost::uuids::to_string(uuid);
  }
}  ///  end namespace goblin::objectstore::watch
