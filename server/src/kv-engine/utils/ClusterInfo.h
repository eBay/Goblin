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

#ifndef SERVER_SRC_KV_ENGINE_UTILS_CLUSTERINFO_H_
#define SERVER_SRC_KV_ENGINE_UTILS_CLUSTERINFO_H_

#include <INIReader.h>
#include <spdlog/spdlog.h>

#include <infra/util/Util.h>
#include <infra/util/StrUtil.h>

namespace goblin::kvengine::utils {

using NodeId = uint64_t;
using HostName = std::string;
using Addr = std::string;
using Port = uint32_t;

static constexpr Port kDefaultNetAdminPort = 50065;

class ClusterInfo final {
 public:
  struct Node {
    NodeId mNodeId;
    HostName mHostName;
    Addr mAddr;
  };

  static std::pair<NodeId, ClusterInfo> resolveAllClusters(const INIReader &iniReader) {
    /// load from local config, the cluster.conf be specified
    std::string clusterConf = iniReader.Get("raft.default", "cluster.conf", "UNKNOWN");
    assert(clusterConf != "UNKNOWN");
    auto clusterInfo = parseToClusterInfo(clusterConf);
    auto selfId = iniReader.GetInteger("raft.default", "self.id", -1);
    // Fix E2E issue
    if ( selfId != -1 ) {
      SPDLOG_INFO("cluster.size={}, self.id={}",
                  clusterInfo.mNodes.size(), selfId);
      return {selfId, clusterInfo};
    } else {
      auto myHostname = gringofts::Util::getHostname();
      std::optional<NodeId> myNodeId = std::nullopt;
      std::optional<Addr> myAddr = std::nullopt;
      for (auto &[nodeId, node] : clusterInfo.mNodes) {
        if (myHostname == node.mHostName) {
          myNodeId = nodeId;
          myAddr = node.mAddr;
          break;
        }
      }
      assert(myNodeId != 0);
      SPDLOG_INFO("cluster.size={}, self.id={}, self.addr={}",
                  clusterInfo.mNodes.size(), *myNodeId, *myAddr);
      return {*myNodeId, clusterInfo};
    }
  }

  void addNode(const Node &node) {
    mNodes[node.mNodeId] = node;
  }

  std::map<NodeId, Node> getAllNodeInfo() const {
    return mNodes;
  }

 private:
  static ClusterInfo parseToClusterInfo(const std::string &clusterConf) {
    std::regex regex("([0-9]+)@([^:]+):([0-9]+)");
    std::smatch match;
    ClusterInfo info;
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

      Node node;

      node.mNodeId = peerId;
      node.mHostName = host;
      node.mAddr = addr;
      info.addNode(node);

      SPDLOG_INFO("cluster.size={}, node.mNodeId={}, node.mHostName={}, node.mAddr={}",
                  raftCluster.size(), peerId, host, addr);
    }
    return info;
  }

  std::map<NodeId, Node> mNodes;
};

}  /// namespace goblin::kvengine::utils

#endif  // SERVER_SRC_KV_ENGINE_UTILS_CLUSTERINFO_H_
