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

#ifndef SERVER_SRC_OBJECT_MANAGER_OBJECTMANAGER_H_
#define SERVER_SRC_OBJECT_MANAGER_OBJECTMANAGER_H_

#include <tuple>

#include <INIReader.h>
#include <spdlog/spdlog.h>

#include "../kv-engine/KVEngine.h"
#include "../kv-engine/types.h"
#include "network/RequestReceiver.h"

namespace goblin {
namespace mock {
template <typename ClusterType>
class MockAppCluster;
}
}

namespace goblin::objectmanager {

class ObjectManager final {
 public:
  explicit ObjectManager(const char *configPath);
  ~ObjectManager();

  // disallow copy ctor and copy assignment
  ObjectManager(const ObjectManager &) = delete;
  ObjectManager &operator=(const ObjectManager &) = delete;

  // disallow move ctor and move assignment
  ObjectManager(ObjectManager &&) = delete;
  ObjectManager &operator=(ObjectManager &&) = delete;

  void run();

  void shutdown();

 private:
  void initMonitor(const INIReader &reader);

  void startRequestReceiver();

  void startNetAdminServer();

  void startPostServerLoop();

  kvengine::utils::Status becomeLeaderCallBack(
      const std::vector<gringofts::raft::MemberInfo> &clusterInfo) {
    return kvengine::utils::Status::ok();
  }
  kvengine::utils::Status preExecuteCallBack(
      const kvengine::model::CommandContext &context,
      const kvengine::store::KVStore &kvStore) {
    return kvengine::utils::Status::ok();
  }

 private:
  std::shared_ptr<kvengine::KVEngine> mKVEngine;
  std::unique_ptr<network::RequestReceiver> mRequestReceiver;

  bool mIsShutdown;

  const kvengine::store::WSName mWSName = "ObjectManagerData";

  /// for UT
  ObjectManager(const char *configPath,
      std::shared_ptr<kvengine::KVEngine> engine);
  template <typename ClusterType>
  friend class mock::MockAppCluster;
};
}  ///  namespace goblin::objectmanager

#endif  //  SERVER_SRC_OBJECT_MANAGER_OBJECTMANAGER_H_
