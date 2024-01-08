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

#ifndef SERVER_SRC_OBJECT_STORE_WATCH_WATCHENTRY_H_
#define SERVER_SRC_OBJECT_STORE_WATCH_WATCHENTRY_H_

#include "../../kv-engine/KVWatch.h"

namespace goblin::objectstore::watch {

using WatchIdType = std::string;

class WatchEntry {
 public:
  WatchEntry(const kvengine::store::KeyType &key,
      const kvengine::store::ValueType &value,
      const kvengine::store::VersionType &version,
      const kvengine::store::VersionType &recordVersion,
      bool exists);

  kvengine::store::KeyType getKey() const { return mKey; }
  kvengine::store::ValueType getValue() const { return mValue; }
  kvengine::store::VersionType getVersion() const { return mVersion; }
  kvengine::store::VersionType getRecordVersion() const { return mRecordVersion; }
  bool isDeleted() const { return !mExists; }

 private:
  kvengine::store::KeyType mKey;
  kvengine::store::ValueType mValue;
  kvengine::store::VersionType mVersion;
  /// for delete entry, the record version and value version could be different
  kvengine::store::VersionType mRecordVersion;
  bool mExists;
};

class WatchEntryWrap {
 public:
  explicit WatchEntryWrap(WatchIdType  watchId);
  void valueChange(const kvengine::store::KeyType &,
      const kvengine::store::ValueType &,
      const kvengine::store::VersionType &);
  void keyDeleted(const kvengine::store::KeyType &,
      const kvengine::store::VersionType &version,
      const kvengine::store::VersionType &recordVersion);
  const WatchIdType& getWatchId() const { return mWatchId; }
  void purgeValueChange(std::vector<std::unique_ptr<WatchEntry>> *);

 private:
  WatchIdType mWatchId;
  std::vector<std::unique_ptr<WatchEntry>> mEntries;
  std::mutex mMutex;
};
}  // namespace goblin::objectstore::watch

#endif  //  SERVER_SRC_OBJECT_STORE_WATCH_WATCHENTRY_H_
