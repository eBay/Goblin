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

#ifndef SERVER_SRC_KV_ENGINE_UTILS_CLUSTERINFO_CPP_
#define SERVER_SRC_KV_ENGINE_UTILS_CLUSTERINFO_CPP_

#include "ClusterInfoUtil.h"
#include <cstdlib>

namespace goblin::kvengine::utils {

std::map<NodeId, HostName> ClusterInfoUtil::mNodeMap;

std::pair<NodeId, gringofts::ClusterInfo> ClusterInfoUtil::resolveAllClusters(const INIReader &iniReader) {
  /// load from local config, the cluster.conf be specified
  std::string clusterConf = iniReader.Get("raft.default", "cluster.conf", "UNKNOWN");
  assert(clusterConf != "UNKNOWN");
  auto clusterInfo = parseToClusterInfo(clusterConf);
  const auto &nodes = clusterInfo.getAllNodeInfo();
  auto selfId = iniReader.GetInteger("raft.default", "self.id", -1);
  // Fix E2E issue
  std::optional<NodeId> myNodeId = std::nullopt;
  if ( selfId != -1 ) {
    SPDLOG_INFO("cluster.size={}, self.id={}", nodes.size(), selfId);
    myNodeId = selfId;
  } else {
    std::string myHostname;
    // fss deployment: we get hostname from env varialbe "SELF_UDNS" providied by FSS
    char * udnsHostname = getenv("SELF_UDNS");
    // statefulset deployment: we get hostname via operating system API
    if (NULL == udnsHostname) {
      myHostname = gringofts::Util::getHostname();
    } else {
      myHostname.assign(udnsHostname);
    }
    for (auto &[nodeId, node] : nodes) {
      if (node.mHostName == myHostname) {
        myNodeId = nodeId;
        break;
      }
    }
    SPDLOG_INFO("cluster.size={}, self.id={}, self.addr={}",
                nodes.size(), *myNodeId, myHostname);
  }
  assert(myNodeId.has_value() && myNodeId != 0);

  return {myNodeId.value(), clusterInfo};
}

std::string ClusterInfoUtil::getIpFromNodeId(std::optional<uint64_t> nodeId) {
  if (nodeId) {
    const auto &resolvedIpAddr = TessDNSResolver::getIpByHostname(mNodeMap[*nodeId]);
    SPDLOG_INFO("leader node id {} ip {}", *nodeId, resolvedIpAddr);
    return resolvedIpAddr;
  } else {
    SPDLOG_ERROR("node id is null");
    return "";
  }
}

gringofts::ClusterInfo ClusterInfoUtil::parseToClusterInfo(const std::string &clusterConf) {
  std::regex regex("([0-9]+)@([^:]+):([0-9]+)");
  std::smatch match;
  gringofts::ClusterInfo info;
  std::vector<std::string> raftCluster = gringofts::StrUtil::tokenize(clusterConf, ',');
  for (auto expectId = 1; expectId <= raftCluster.size(); ++expectId) {
    const auto &raftNode = raftCluster[expectId - 1];
    if (!std::regex_search(raftNode, match, regex)) {
      throw std::runtime_error("Bad cluster conf " + raftNode);
    }

    uint64_t peerId = std::stoul(match[1]);
    assert(peerId == expectId);

    std::string host = match[2];
    std::string port = match[3];
    std::string addr = host + ":" + port;

    gringofts::ClusterInfo::Node node;

    node.mNodeId = peerId;
    node.mHostName = host;
    node.mPortForRaft = std::stoi(port);
    info.addNode(node);
    mNodeMap[node.mNodeId] = host;
    SPDLOG_INFO("cluster.size={}, node.mNodeId={}, node.mHostName={}, node.mAddr={}",
                raftCluster.size(), peerId, host, addr);
  }
  return info;
}

}  /// namespace goblin::kvengine::utils

#endif  // SERVER_SRC_KV_ENGINE_UTILS_CLUSTERINFO_CPP_
