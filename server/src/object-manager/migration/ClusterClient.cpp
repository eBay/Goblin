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

#include "ClusterClient.h"

#include <utility>

#include "../../kv-engine/utils/TessDNSResolver.h"

namespace goblin::objectmanager::migration {
  ClusterClient::ClusterClient(proto::Cluster::ClusterAddress clusterAddress,
                               const std::optional<kvengine::utils::TlsConf>& tlsConf)
          : mLeaderIndex(0), mClusterAddress(std::move(clusterAddress)), mTlsConf(tlsConf) {
    init(tlsConf);
  }

  void ClusterClient::init(const std::optional<kvengine::utils::TlsConf> &tlsConf) {
    mDNSResolver = std::make_shared<kvengine::utils::TessDNSResolver>();
    for (int i = 0, size = mClusterAddress.servers_size(); i < size; ++i) {
      auto hostName = mClusterAddress.servers(i).hostname();
      NodeIdType nodeId = i + 1;
      auto pos = hostName.find('@');
      if (pos != std::string::npos) {
        nodeId = std::stoi(hostName.substr(0, pos));
        hostName = hostName.substr(pos + 1);
      }
      buildClient(nodeId, hostName);
      mNodeClientVector.push_back(mClientMap[nodeId].get());
      SPDLOG_INFO("Cluster node registration done, node id is {}, address is {}", nodeId, hostName);
    }
  }

  void ClusterClient::buildClient(NodeIdType nodeId, const std::string& hostName) {
    grpc::ChannelArguments chArgs;
    chArgs.SetMaxReceiveMessageSize(INT_MAX);
    auto ipPort = mDNSResolver->resolve(hostName);
    auto clientIndex = -1;
    for (int i = 0, size = mNodeClientVector.size(); i < size; ++i) {
      if (mNodeClientVector[i]->getNodeId() == nodeId) {
        clientIndex = i;
        break;
      }
    }
    mClientMap[nodeId] = std::make_unique<NodeClient>(
            grpc::CreateCustomChannel(ipPort, kvengine::utils::TlsUtil::buildChannelCredentials(mTlsConf), chArgs),
            nodeId,
            hostName);
    if (clientIndex != -1) {
      mNodeClientVector[clientIndex] = mClientMap[nodeId].get();
    }
  }

  uint64_t ClusterClient::getNextNodeIndex(uint64_t index) const {
    assert(index < this->mClusterAddress.servers_size());
    return (index + 1) % this->mClusterAddress.servers_size();
  }

  NodeIdType ClusterClient::getNodeIdByIndex(uint64_t index) {
    return mNodeClientVector[index % mNodeClientVector.size()]->getNodeId();
  }

  int ClusterClient::getIndexByNodeId(NodeIdType nodeId) {
    for (int i = 0, size = mNodeClientVector.size(); i < size; ++i) {
      if (mNodeClientVector[i]->getNodeId() == nodeId) {
        return i;
      }
    }
    assert(0);
  }

  proto::StartMigration::Response ClusterClient::doMigration(const proto::StartMigration::Request &migrationRequest) {
    const auto& [code, response] = doMigrationImpl(migrationRequest, mLeaderIndex, 1);
    if (code == proto::GENERAL_ERROR) {
      SPDLOG_ERROR("Can't reach cluster, one cluster address is {}", mClusterAddress.servers(0).hostname());
    }
    return response;
  }

  proto::EndMigration_Response ClusterClient::endMigration(const proto::EndMigration_Request& request) {
    const auto& [code, response] = endMigrationImpl(request, mLeaderIndex, 1);
    if (code == proto::GENERAL_ERROR) {
      SPDLOG_ERROR("Can't reach cluster, one cluster address is {}", mClusterAddress.servers(0).hostname());
    }
    return response;
  }

  std::tuple<proto::ResponseCode, proto::StartMigration::Response> ClusterClient::doMigrationImpl(
      const proto::StartMigration::Request &req, uint64_t leaderIndex, int retryTime) {
    proto::StartMigration::Response response;
    if (retryTime > MAX_ROUND * mClusterAddress.servers_size()) {
      return { proto::GENERAL_ERROR, response};
    }

    auto nodeId = this->getNodeIdByIndex(leaderIndex);
    SPDLOG_INFO("Begin to doMigration, leader index is {}, node id is {}, address is {}",
        leaderIndex, nodeId, mClientMap[nodeId]->getHostname());
    mClientMap[nodeId]->doMigration(req, &response);
    if (response.header().code() == proto::OK) {
      if (this->mLeaderIndex != leaderIndex) {
        this->mLeaderIndex = leaderIndex;
      }
      return {proto::OK, response};
    }
    if (response.header().code() == proto::NOT_LEADER) {
      const auto& leaderHint = response.header().leaderhint();
      if (!leaderHint.empty()) {
        NodeIdType leaderNodeId = std::stoi(leaderHint);
        return doMigrationImpl(req, this->getIndexByNodeId(leaderNodeId), retryTime + 1);
      }
    }
    doSleep(retryTime);
    if (response.header().code() == proto::UNKNOWN || response.header().code() == proto::GENERAL_ERROR) {
      buildClient(nodeId, mNodeClientVector[leaderIndex]->getHostname());
    }
    return doMigrationImpl(req, this->getNextNodeIndex(leaderIndex), retryTime + 1);
  }

  std::tuple<proto::ResponseCode, proto::EndMigration_Response> ClusterClient::endMigrationImpl(
      const proto::EndMigration_Request &req, uint64_t leaderIndex, int retryTime) {
    proto::EndMigration_Response response;
    if (retryTime > MAX_ROUND * mClusterAddress.servers_size()) {
      return { proto::GENERAL_ERROR, response};
    }

    auto nodeId = this->getNodeIdByIndex(leaderIndex);
    SPDLOG_INFO("Begin to endMigration, leader index is {}, node id is {}, address is {}",
        leaderIndex, nodeId, mClientMap[nodeId]->getHostname());
    mClientMap[nodeId]->endMigration(req, &response);
    if (response.header().code() == proto::OK) {
      if (this->mLeaderIndex != leaderIndex) {
        this->mLeaderIndex = leaderIndex;
      }
      return {proto::OK, response};
    }
    if (response.header().code() == proto::NOT_LEADER) {
      const auto& leaderHint = response.header().leaderhint();
      if (!leaderHint.empty()) {
        return endMigrationImpl(req, std::stoi(leaderHint), retryTime + 1);
      }
    }
    doSleep(retryTime);
    buildClient(nodeId, mNodeClientVector[leaderIndex]->getHostname());
    return endMigrationImpl(req, this->getNextNodeIndex(leaderIndex), retryTime + 1);
  }

  void ClusterClient::doSleep(int retry) {
    const auto serverSize = mClusterAddress.servers_size();
    const int milliSec = ((retry - 1) / serverSize + 1) * SLEEP_MILLI_SEC_PER_ROUND;
    std::this_thread::sleep_for(std::chrono::milliseconds(milliSec));
  }

  void ClusterClient::update(const proto::Cluster::ClusterAddress & address) {
    /// TODO: for node address update in cluster
  }

  ClusterClient::NodeClient::NodeClient(const std::shared_ptr<grpc::Channel> &channel, NodeIdType nodeId,
                                        std::string hostName)
          : mStub(std::move(proto::KVStore::NewStub(channel))), mNodeId(nodeId), mHostName(std::move(hostName)) {
  }

  void ClusterClient::NodeClient::doMigration(
      const proto::StartMigration::Request& migrationRequest, proto::StartMigration::Response* response) {
    grpc::ClientContext context;
    auto status = mStub->StartMigration(&context, migrationRequest, response);
    SPDLOG_INFO("ClusterClient::NodeClient::doMigration returns {}", status.error_code());
  }

  void ClusterClient::NodeClient::endMigration(
      const proto::EndMigration_Request& request, proto::EndMigration_Response* response) {
    grpc::ClientContext context;
    auto status = mStub->EndMigration(&context, request, response);
    SPDLOG_INFO("endMigration returns {}", status.error_code());
  }
}  // namespace goblin::objectmanager::migration
