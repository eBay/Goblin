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
#include "KVEvictCommand.h"
#include <spdlog/spdlog.h>

namespace goblin::kvengine::model {

utils::Status KVEvictCommand::prepare(const std::shared_ptr<store::KVStore> &kvStore) {
  return kvStore->lock(mEvictKeys, true);
}

utils::Status KVEvictCommand::execute(const std::shared_ptr<store::KVStore> &, EventList *) {
  // no events generated
  return utils::Status::ok();
}

utils::Status KVEvictCommand::finish(const std::shared_ptr<store::KVStore> &kvStore, const EventList &events) {
  kvStore->evictKV(mEvictKeys, mEvictVersion);
  assert(kvStore->unlock(mEvictKeys, true).isOK());
  return utils::Status::ok();
}

}  // namespace goblin::kvengine::model
