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

#include "Cluster.h"

namespace goblin::objectmanager::module {
  Cluster::Cluster(ClusterInfo clusterInfo, ClusterIdType clusterId, VersionType version,
                   const std::shared_ptr<kvengine::KVEngine> &kvEngine)
        : Base(kvEngine), mClusterInfo(std::move(clusterInfo)), mClusterId(clusterId), mVersion(version) {
  }

  void Cluster::fillPrecondition(proto::Transaction_Request& transReq, ClusterIdType clusterId) {
    auto cond = transReq.add_preconds()->mutable_versioncond();
    cond->set_key(buildClusterKey(clusterId));
    cond->set_version(getVersion());
    cond->set_op(proto::Precondition_CompareOp_EQUAL);
    SPDLOG_INFO("Cluster2::fillPrecondition, add cluster exist precondition, version is {}", cond->version());
  }

  void Cluster::updateInTransaction(ClusterStatus newStatus, proto::Transaction_Request &transReq) {
    if (this->getStatus() == newStatus) {
      SPDLOG_INFO("Cluster::updateInTransaction, newStatus {} equals to the current status", newStatus);
      return;
    }
    setStatus(newStatus);
    auto pWriteEntry = transReq.add_entries()->mutable_writeentry();
    pWriteEntry->set_key(buildClusterKey(this->getClusterId()));
    pWriteEntry->set_value(mClusterInfo.SerializeAsString());
  }

  void Cluster::updateClusterInTransaction(const ServerAddressList& addresses, proto::Transaction_Request& transReq) {
    SPDLOG_INFO("Cluster::updateClusterInTransaction, updatedClusterId is {}", mClusterId);
    auto existCond = transReq.add_preconds()->mutable_existcond();
    const auto& clusterKey = buildClusterKey(mClusterId);

    existCond->set_key(clusterKey);
    existCond->set_shouldexist(true);

    ClusterInfo clusterInfo;
    clusterInfo.set_status(this->getStatus());
    auto mutableAddr = clusterInfo.mutable_addr();

    for (auto& address : addresses) {
      mutableAddr->add_servers()->set_hostname(address.hostname());
      SPDLOG_INFO("Cluster::updateClusterInTransaction, host name is {}", address.hostname());

      auto pWriteEntry = transReq.add_entries()->mutable_writeentry();
      pWriteEntry->set_key(address.hostname());
      pWriteEntry->set_value(std::to_string(mClusterId));
    }

    auto pWriteEntry = transReq.add_entries()->mutable_writeentry();
    pWriteEntry->set_value(clusterInfo.SerializeAsString());
    pWriteEntry->set_key(clusterKey);
  }

  /// return: status, sameClusterAddressSize, clusterId
  std::tuple<proto::ResponseCode, int, ClusterIdType> Cluster::isAddressInSameCluster(
          const ServerAddressList &addresses, const std::shared_ptr<kvengine::KVEngine> &kvEngine) {
    proto::ExeBatch_Request batchReq;
    for (auto &addr : addresses) {
      batchReq.add_entries()->mutable_readentry()->set_key(addr.hostname());
    }
    const auto& batchResponse = kvEngine->exeBatch(batchReq);
    if (batchResponse.header().code() != proto::OK) {
      SPDLOG_WARN("Cluster::isAddressInSameCluster, exeBatch failed, return code {}", batchResponse.header().code());
      return {batchResponse.header().code(), 0, NON_CLUSTER_ID};
    }

    int existingKeyCount = 0;
    std::string clusterIdStr;
    for (int i = 0, size = batchResponse.results_size(); i < size; ++i) {
      assert(batchResponse.results(i).has_readresult());
      auto& result = batchResponse.results(i).readresult();
      auto code = result.code();
      if (code != proto::OK && code != proto::KEY_NOT_EXIST) {
        return {code, 0, NON_CLUSTER_ID};
      }
      if (code == proto::KEY_NOT_EXIST || result.value().empty()) {
        continue;
      }

      existingKeyCount += 1;
      if (clusterIdStr.empty()) {
        clusterIdStr = result.value();
      } else if (clusterIdStr != result.value()) {
        return {proto::BAD_REQUEST, 0, NON_CLUSTER_ID};
      }
    }
    if (clusterIdStr.empty()) {
      return {proto::OK, existingKeyCount, NON_CLUSTER_ID};
    }
    return {proto::OK, existingKeyCount, std::stoi(clusterIdStr)};
  }

  void Cluster::createInTransaction(ClusterIdType newClusterId,
                                    const ServerAddressList& addresses,
                                    proto::Transaction_Request& transReq) {
    SPDLOG_INFO("Cluster::createInTransaction, newClusterId is {}", newClusterId);
    auto existCond = transReq.add_preconds()->mutable_existcond();
    const auto& clusterKey = buildClusterKey(newClusterId);

    existCond->set_key(clusterKey);
    existCond->set_shouldexist(false);

    ClusterInfo clusterInfo;
    clusterInfo.set_status(proto::Cluster_ClusterStatus_INIT);
    auto mutableAddr = clusterInfo.mutable_addr();

    for (auto& address : addresses) {
      mutableAddr->add_servers()->set_hostname(address.hostname());
      SPDLOG_INFO("Cluster::createInTransaction, host name is {}", address.hostname());

      auto cond = transReq.add_preconds()->mutable_existcond();
      cond->set_key(address.hostname());
      cond->set_shouldexist(false);

      auto pWriteEntry = transReq.add_entries()->mutable_writeentry();
      pWriteEntry->set_key(address.hostname());
      pWriteEntry->set_value(std::to_string(newClusterId));
    }

    auto pWriteEntry = transReq.add_entries()->mutable_writeentry();
    pWriteEntry->set_value(clusterInfo.SerializeAsString());
    pWriteEntry->set_key(clusterKey);
  }

  void Cluster::removeInTransaction(proto::Transaction_Request &transReq) {
    transReq.add_entries()->mutable_removeentry()->set_key(buildClusterKey(getClusterId()));
    SPDLOG_INFO("Cluster::removeInTransaction, begin to remove cluster {} with key {} in transaction",
                getClusterId(), buildClusterKey(getClusterId()));
    for (auto &server : getServerAddresses()) {
      SPDLOG_INFO("Cluster::removeInTransaction, begin to remove cluster host name {} in transaction",
                  server.hostname());
      transReq.add_entries()->mutable_removeentry()->set_key(server.hostname());
    }
  }
}  //  namespace goblin::objectmanager::module
