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

#ifndef SERVER_SRC_OBJECT_STORE_WATCH_WATCHOBSERVER_H_
#define SERVER_SRC_OBJECT_STORE_WATCH_WATCHOBSERVER_H_

#include "WatchCenter.h"

namespace goblin::objectstore::watch {

class WatchObserver : public kvengine::store::KVObserver {
 public:
  WatchObserver() = default;
  ~WatchObserver() override = default;
  void onReadValue(const kvengine::store::KeyType &key) override {
    assert(0);
  }
  void onDeleteKey(const kvengine::store::KeyType &key,
      const kvengine::store::VersionType &version,
      const kvengine::store::VersionType &deleteRecordVersion) override;
  void onEvictKeys(const std::set<kvengine::store::KeyType> &keys) override {
    assert(0);
  };
  void onWriteValue(const kvengine::store::KeyType &key,
      const kvengine::store::ValueType &value,
      const kvengine::store::VersionType &version) override;
};
}  /// end namespace goblin::objectstore::watch

#endif  //  SERVER_SRC_OBJECT_STORE_WATCH_WATCHOBSERVER_H_
