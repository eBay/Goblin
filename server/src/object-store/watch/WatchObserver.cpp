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

#include "WatchObserver.h"

namespace goblin::objectstore::watch {
  void WatchObserver::onDeleteKey(
      const kvengine::store::KeyType &key,
      const kvengine::store::VersionType &version,
      const kvengine::store::VersionType &deleteRecordVersion) {
    WatchCenter::instance().onDeleteKey(key, version, deleteRecordVersion);
  }

  void WatchObserver::onWriteValue(const kvengine::store::KeyType &key,
      const kvengine::store::ValueType &value,
      const kvengine::store::VersionType &version) {
    // SPDLOG_INFO("WatchObserver::onWriteValue, put key {} with value {}", key, value);
    WatchCenter::instance().onWriteValue(key, value, version);
  }
}  ///  namespace goblin::objectstore::watch
