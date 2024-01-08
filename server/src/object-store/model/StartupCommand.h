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

#ifndef SERVER_SRC_OBJECT_STORE_MODEL_STARTUPCOMMAND_H_
#define SERVER_SRC_OBJECT_STORE_MODEL_STARTUPCOMMAND_H_

#include "../../kv-engine/model/Command.h"
#include "../../kv-engine/model/Event.h"

namespace goblin::objectstore::model {

/// this command is just to trigger the leader election for requesting route
/// itself will do nothing
class StartupCommand final : public kvengine::model::Command {
 public:
  StartupCommand() {}

  kvengine::utils::Status prepare(const std::shared_ptr<kvengine::store::KVStore> &) override {
     return kvengine::utils::Status::ok();
  }
  kvengine::utils::Status execute(
      const std::shared_ptr<kvengine::store::KVStore> &,
      kvengine::model::EventList *) override {
     SPDLOG_INFO("execute startup command");
     return kvengine::utils::Status::ok();
  }
  kvengine::utils::Status finish(
      const std::shared_ptr<kvengine::store::KVStore> &kvStore,
      const kvengine::model::EventList &events) override {
     return kvengine::utils::Status::ok();
  }
};

}  // namespace goblin::objectstore::model

#endif  // SERVER_SRC_OBJECT_STORE_MODEL_STARTUPCOMMAND_H_
