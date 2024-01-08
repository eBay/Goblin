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

#ifndef SERVER_SRC_OBJECT_STORE_WATCH_WATCHCENTER_H_
#define SERVER_SRC_OBJECT_STORE_WATCH_WATCHCENTER_H_

#include <unordered_map>
#include <memory>
#include <shared_mutex>

#include "WatchEntry.h"

namespace goblin::objectstore::watch {

class WatchCenter {
 public:
  WatchCenter(const WatchCenter&) = delete;
  void operator=(const WatchCenter &) = delete;
  WatchCenter(WatchCenter &&) = delete;
  void operator=(WatchCenter &&) = delete;

  static WatchCenter &instance() {
    static WatchCenter mInstance;
    return mInstance;
  }

  WatchIdType registerWatch(const std::vector<kvengine::store::KeyType> &keyVector);
  bool unRegisterWatch(const WatchIdType& watchId);
  bool purgeWatchValue(const WatchIdType&, std::vector<std::unique_ptr<WatchEntry>> *);

  void onWriteValue(
      const kvengine::store::KeyType &key,
      const kvengine::store::ValueType &value,
      const kvengine::store::VersionType &version);
  void onDeleteKey(const kvengine::store::KeyType &key,
      const kvengine::store::VersionType &version,
      const kvengine::store::VersionType &deleteRecordVersion);

 protected:
  WatchCenter() = default;

 private:
  static WatchIdType allocateId();

 private:
  /// for observer, key is watch key, value is watch entry wrap vector
  std::unordered_map<kvengine::store::KeyType, std::vector<std::unique_ptr<WatchEntryWrap>>> mKey2WatchEntryMap;
  /// for un-register watch, key is watch id while value is key list associated with the id
  std::unordered_map<WatchIdType, std::vector<kvengine::store::KeyType>> mWatchId2KeyMap;

  std::shared_mutex mSharedMutex;
};
}  ///  end namespace goblin::objectstore::watch

#endif  //  SERVER_SRC_OBJECT_STORE_WATCH_WATCHCENTER_H_
