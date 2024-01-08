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

#include "WatchEntry.h"

#include <utility>

namespace goblin::objectstore::watch {
  WatchEntry::WatchEntry(const kvengine::store::KeyType &key,
      const kvengine::store::ValueType &value,
      const kvengine::store::VersionType &version,
      const kvengine::store::VersionType &recordVersion,
      bool exists)
    : mKey(key), mValue(value), mVersion(version), mRecordVersion(recordVersion), mExists(exists) {
  }

  WatchEntryWrap::WatchEntryWrap(WatchIdType watchId) : mWatchId(watchId) {
  }

  void WatchEntryWrap::valueChange(const kvengine::store::KeyType &key,
      const kvengine::store::ValueType &value,
      const kvengine::store::VersionType &version) {
    std::lock_guard<std::mutex> lock(mMutex);
    // SPDLOG_INFO("WatchEntryWrap::valueChange, watch id {}, key and version is {} - {}", mWatchId, key, version);

    mEntries.push_back(std::make_unique<WatchEntry>(key, value, version, version, true));
  }

  void WatchEntryWrap::keyDeleted(const kvengine::store::KeyType &key,
      const kvengine::store::VersionType &version,
      const kvengine::store::VersionType &recordVersion) {
    std::lock_guard<std::mutex> lock(mMutex);
    // SPDLOG_INFO("WatchEntryWrap::keyDeleted, watch id is {}, key is {}", mWatchId, key);

    mEntries.push_back(std::make_unique<WatchEntry>(key, "", version, recordVersion, false));
  }

  void WatchEntryWrap::purgeValueChange(std::vector<std::unique_ptr<WatchEntry>> *pVec) {
    std::lock_guard<std::mutex> lock(mMutex);
    std::move(std::begin(mEntries), std::end(mEntries), std::back_inserter(*pVec));
    mEntries.clear();
  }
}  ///  end namespace goblin::objectstore::watch
