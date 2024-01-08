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

#ifndef SERVER_SRC_KV_ENGINE_UTILS_CLUSTERINFOUTIL_H_
#define SERVER_SRC_KV_ENGINE_UTILS_CLUSTERINFOUTIL_H_

#include <INIReader.h>
#include <spdlog/spdlog.h>
#include <absl/strings/str_format.h>

#include <infra/util/ClusterInfo.h>
#include <infra/util/Util.h>
#include <infra/util/StrUtil.h>
#include "TessDNSResolver.h"

namespace goblin::kvengine::utils {

using NodeId = uint64_t;
using HostName = std::string;
using Addr = std::string;

class ClusterInfoUtil {
 public:
  static std::pair<NodeId, gringofts::ClusterInfo> resolveAllClusters(const INIReader &iniReader);
  static std::string getIpFromNodeId(std::optional<uint64_t> nodeId);

 private:
  static gringofts::ClusterInfo parseToClusterInfo(const std::string &clusterConf);
  static std::map<NodeId, HostName> mNodeMap;
};

}  /// namespace goblin::kvengine::utils

#endif  // SERVER_SRC_KV_ENGINE_UTILS_CLUSTERINFOUTIL_H_
