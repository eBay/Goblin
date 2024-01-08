/************************************************************************
Copyright 2019-2020 eBay Inc.
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

#include <spdlog/spdlog.h>

#include "AppUtil.h"

namespace goblin::kvengine::utils {

AppInfo::AppInfo(const INIReader &reader) {
  std::string raftConfigPath = reader.Get("store", "raft.config.path", "UNKNOWN");
  assert(raftConfigPath != "UNKNOWN");
  INIReader raftReader(raftConfigPath);

  auto[myNodeId, myClusterInfo] = ClusterInfoUtil::resolveAllClusters(raftReader);
  mMyNodeId = myNodeId;
  mMyClusterInfo = myClusterInfo;

  mAppVersion = AppUtil::getCurrentVersion();

  SPDLOG_INFO("Global settings: "
              "app.version={}, "
              "app.nodeid={}",
              mAppVersion,
              mMyNodeId);
}
}  /// namespace goblin::kvengine::utils
