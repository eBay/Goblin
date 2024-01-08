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

#include "ClusterSetManager.h"
#include "../route/HashRing.h"
#include "../migration/ClusterProxy.h"

namespace goblin::objectmanager::module {
  ClusterSetManager::ClusterSetManager(const std::shared_ptr<kvengine::KVEngine> &kvEngine)
      : Base(kvEngine), mVersion(0) {
  }

  void ClusterSetManager::init(ResponseHeader &header) {
    proto::Get_Request getRequest;
    getRequest.mutable_entry()->set_key(KEY_CLUSTER_GLOBAL_INFO);
    auto getResponse = mKVEngine->get(getRequest);

    header = getResponse.header();
    if (header.code() != proto::OK) {
      SPDLOG_INFO("ClusterSetManager::init, return code in header: {}", header.code());
      header.set_message("ClusterSetManager::init, get global cluster info failed");
      return;
    }

    auto code = getResponse.result().code();
    if (code != proto::OK) {
      SPDLOG_INFO("ClusterSetManager::init, return code in result: {}", code);
      header.set_code(code == proto::KEY_NOT_EXIST ? proto::OK : code);
      header.set_message("ClusterSetManager::init, get global cluster result failed");
      return;
    }

    SPDLOG_INFO("ClusterSetManager::init, return version {}", getResponse.result().version());
    assert(mClusterSetGlobalInfo.ParseFromString(getResponse.result().value()));
    setVersion(getResponse.result().version());
    assert(getVersion() != 0);

    proto::Transaction_Request transReq;
    auto cond = transReq.add_preconds()->mutable_versioncond();
    cond->set_key(KEY_CLUSTER_GLOBAL_INFO);
    cond->set_op(proto::Precondition_CompareOp_EQUAL);
    cond->set_version(getVersion());

    for (auto clusterId : mClusterSetGlobalInfo.clusterids()) {
      const auto &key = buildClusterKey(clusterId);
      fillExistPrecondition(transReq.add_preconds()->mutable_existcond(), key, true);
      transReq.add_entries()->mutable_readentry()->set_key(key);
    }

    for (auto migrationTaskId : mClusterSetGlobalInfo.migrationtaskids()) {
      const auto &key = buildMigrationTaskKey(migrationTaskId);
      fillExistPrecondition(transReq.add_preconds()->mutable_existcond(), key, true);
      transReq.add_entries()->mutable_readentry()->set_key(key);
    }

    auto transResponse = mKVEngine->trans(transReq);
    header = transResponse.header();
    if (header.code() != proto::OK) {
      header.set_message("ClusterSetManager::init, load global info in transaction failed");
      return;
    }

    const int clusterSize = mClusterSetGlobalInfo.clusterids_size();
    for (int i = 0; i < clusterSize; ++i) {
      assert(transResponse.results(i).has_readresult());
      ClusterIdType clusterId = mClusterSetGlobalInfo.clusterids(i);
      auto readResult = transResponse.results(i).readresult();
      if (readResult.code() != proto::OK) {
        SPDLOG_WARN("ClusterSetManager::init, load cluster info by id {} failed, return {}", clusterId,
                    readResult.code());
        buildHeader(header, readResult.code(), "ClusterSetManager::init, load cluster info failed");
        return;
      }
      ClusterInfo clusterInfo;
      assert(clusterInfo.ParseFromString(readResult.value()));
      mClusterVector.push_back(std::make_unique<Cluster>(clusterInfo, clusterId, readResult.version(), mKVEngine));
    }
    for (auto& cluster : mClusterVector) {
      migration::ClusterProxy::instance().registerCluster(cluster->getClusterId(), cluster->getClusterAddress());
    }

    const int migrationSize = mClusterSetGlobalInfo.migrationtaskids_size();
    for (int i = 0; i < migrationSize; ++i) {
      const int index = i + clusterSize;
      assert(transResponse.results(index).has_readresult());
      MigrationTaskIdType migrationId = mClusterSetGlobalInfo.migrationtaskids(i);
      auto readResult = transResponse.results(index).readresult();
      if (readResult.code() != proto::OK) {
        SPDLOG_WARN("ClusterSetManager::init, load migration info by id {} failed, return {}",
                    migrationId, readResult.code());
        buildHeader(header, readResult.code(), "ClusterSetManager::init, load migration info failed");
        return;
      }
      MigrationTask migrationTask;
      assert(migrationTask.ParseFromString(readResult.value()));
      mMigrationVector.push_back(std::make_unique<Migration>(migrationTask, readResult.version(), mKVEngine));
    }
  }

  void ClusterSetManager::fillPrecondition(proto::Transaction_Request &transReq) {
    if (isEmtpy()) {
      auto cond = transReq.add_preconds()->mutable_existcond();
      cond->set_key(KEY_CLUSTER_GLOBAL_INFO);
      cond->set_shouldexist(false);
      SPDLOG_INFO("ClusterSetManager::fillPrecondition, fill not exist precondition");
      return;
    }

    auto globalCond = transReq.add_preconds()->mutable_versioncond();
    globalCond->set_key(KEY_CLUSTER_GLOBAL_INFO);
    globalCond->set_version(getVersion());
    globalCond->set_op(proto::Precondition_CompareOp_EQUAL);
    SPDLOG_INFO("ClusterSetManager add global cluster config exist precondition, version is {}", globalCond->version());

    for (int i = 0, size = mClusterVector.size(); i < size; ++i) {
      mClusterVector[i]->fillPrecondition(transReq, mClusterSetGlobalInfo.clusterids(i));
    }

    for (auto &migration : mMigrationVector) {
      migration->fillPrecondition(transReq);
    }
  }

  void ClusterSetManager::addCluster(ResponseHeader &header, const ServerAddressList &addresses) {
    auto[code, sameClusterCount, clusterId] = module::Cluster::isAddressInSameCluster(addresses, mKVEngine);
    SPDLOG_INFO("ClusterSetManager, isAddressInSameCluster return: code({}), sameClusterCount({}), clusterId({})",
                code, sameClusterCount, clusterId);
    if (code != proto::OK) {
      buildHeader(header, code, "ClusterSetManager::addCluster, call isAddressInSameCluster failed");
      return;
    }
    if (hasMigrationTask()) {
      SPDLOG_WARN("ClusterSetManager::addCluster, hasMigrationTask return true");
      buildHeader(header, proto::BAD_REQUEST, "ClusterSetManager::addCluster, migration task in progress");
      return;
    }

    proto::Transaction_Request transReq;
    this->fillPrecondition(transReq);

    if (sameClusterCount == 0) {  // add new cluster
      clusterId = this->assignNewClusterId();
      code = this->appendNewClusterId(clusterId);
      if (code != proto::OK) {
        SPDLOG_WARN("ClusterSetManager::appendNewClusterId, cluster id {} has been added yet", clusterId);
        buildHeader(header, code, "new cluster id has been occupied");
        return;
      }
      Cluster::createInTransaction(clusterId, addresses, transReq);
    } else {  // update existing cluster info, maybe increasing or decreasing cluster size
      auto pCluster = findClusterById(clusterId);
      if (pCluster == nullptr) {
        SPDLOG_WARN("ClusterSetManager::addCluster, can't find cluster by cluster id: {}", clusterId);
        buildHeader(header, proto::BAD_REQUEST, "cluster not found");
        return;
      }
      pCluster->updateClusterInTransaction(addresses, transReq);
    }

    this->updateInTransaction(transReq);

    SPDLOG_INFO("ClusterSetManager::addCluster, begin to submit transaction");
    header = mKVEngine->trans(transReq).header();
  }

  std::tuple<ClusterStatus, bool> ClusterSetManager::startCluster(ResponseHeader &header,
                                                                  const ServerAddressList& addresses,
                                                                  RouteInfo &routeInfo,
                                                                  MigrationTask &migrationTask) {
    auto[code, sameClusterCount, clusterId] = module::Cluster::isAddressInSameCluster(addresses, mKVEngine);
    SPDLOG_INFO("ClusterSetManager, isAddressInSameCluster return: code({}), sameClusterCount({}), clusterId({})",
                code, sameClusterCount, clusterId);
    if (code != proto::OK) {
      buildHeader(header, code, "ClusterSetManager::startCluster, call isAddressInSameCluster failed");
      return {proto::Cluster_ClusterStatus_INIT, false};
    }

    if (sameClusterCount < addresses.size() || clusterId == NON_CLUSTER_ID) {
      SPDLOG_WARN("ClusterSetManager::startCluster, bad cluster status: sameClusterCount({}), clusterId({})",
                  sameClusterCount, clusterId);
      buildHeader(header, proto::BAD_REQUEST, "ClusterSetManager::startCluster, bad cluster status");
      return {proto::Cluster_ClusterStatus_INIT, false};
    }

    SPDLOG_INFO("ClusterSetManager, sameClusterCount equals to addresses size, all addresses are in same cluster");

    auto pCluster = findClusterById(clusterId);
    if (pCluster == nullptr) {
      SPDLOG_WARN("ClusterSetManager::startCluster, can't find cluster by cluster id: {}", clusterId);
      buildHeader(header, proto::BAD_REQUEST, "cluster not found");
      return {proto::Cluster_ClusterStatus_INIT, false};
    }

    if (pCluster->getStatus() == proto::Cluster_ClusterStatus_OUT_OF_SERVICE) {
      /// TODO: handle out-of-service cluster startup
      SPDLOG_WARN("ClusterSetManager::startCluster, out of service cluster {} found", clusterId);
      buildHeader(header, proto::OK, "cluster status is out-of-service");
      return {pCluster->getStatus(), false};
    }

    if (pCluster->getStatus() == proto::Cluster_ClusterStatus_IN_SERVICE) {
      SPDLOG_WARN("ClusterSetManager::startCluster, in service cluster {} found", clusterId);
      buildHeader(header, fillRouteInfo(routeInfo), "cluster status is in-service");
      return {pCluster->getStatus(), false};
    }

    if (pCluster->getStatus() == proto::Cluster_ClusterStatus_IMPORTING
        || pCluster->getStatus() == proto::Cluster_ClusterStatus_EXPORTING) {
      SPDLOG_WARN("ClusterSetManager::startCluster, importing/exporting service cluster {} found", clusterId);
      buildHeader(header, fillRouteAndMigrationInfo(routeInfo, migrationTask), "cluster status is importing/exporting");
      return {pCluster->getStatus(), false};
    }

    if (pCluster->getStatus() == proto::Cluster_ClusterStatus_INIT) {
      if (this->isOnlyOneCluster()) {
        /// 1, update status;   2, fill routeInfo;  3, fill migrationInfo
        proto::Transaction_Request transReq;
        this->fillPrecondition(transReq);
        pCluster->updateInTransaction(proto::Cluster_ClusterStatus_IN_SERVICE, transReq);
        increaseGlobalRouteVersion();
        updateInTransaction(transReq);
        auto transResponse = mKVEngine->trans(transReq);
        header = transResponse.header();
        SPDLOG_INFO("ClusterSetManager::startCluster, transaction for only one cluster return code {}", header.code());
        if (header.code() != proto::OK) {
          return {pCluster->getStatus(), false};
        }
        buildHeader(header, fillRouteInfo(routeInfo));
        return {pCluster->getStatus(), false};
      }

      if (hasMigrationTask()) {
        SPDLOG_WARN("ClusterSetManager::startCluster, migration task in process, do nothing for cluster {}",
            pCluster->getClusterId());
        return {pCluster->getStatus(), false};
      }

      /// 1, update status;   2, fill routeInfo;  3, fill migrationInfo;  4, notify migration
      code = this->onboardCluster(*pCluster);
      buildHeader(header, code);
      SPDLOG_ERROR("ClusterSetManager::startCluster, onboardCluster return code {}", code);
      if (code == proto::OK) {
        this->fillRouteAndMigrationInfo(routeInfo, migrationTask);
      }
      //  Skip migration as only static sharding is supported currently
      //  needMigration = false
      return {pCluster->getStatus(), false};
      // return {pCluster->getStatus(), code == proto::OK};
    }
    SPDLOG_ERROR("ClusterSetManager::startCluster, Should not reach here");
    assert(0);
  }

  std::tuple<Cluster*, Cluster*> ClusterSetManager::handleMigrationSuccess(ResponseHeader &header,
                                                                           const MigrationProgress &progress) {
    auto pMigrationTask = findMigrationById(progress.taskid());
    assert(pMigrationTask != nullptr);

    auto pSourceCluster = this->findClusterByAddress(pMigrationTask->getMigrationTask().sourcecluster().servers());
    auto pTargetCluster = this->findClusterByAddress(pMigrationTask->getMigrationTask().targetcluster().servers());
    assert(pSourceCluster);
    assert(pTargetCluster);

    proto::Transaction_Request transReq;
    this->fillPrecondition(transReq);
    pSourceCluster->updateInTransaction(proto::Cluster_ClusterStatus_IN_SERVICE, transReq);
    pTargetCluster->updateInTransaction(proto::Cluster_ClusterStatus_IN_SERVICE, transReq);
    pMigrationTask->updateInTransaction(proto::Migration_MigrationStatus_SUCCEEDED, transReq);
    this->increaseGlobalRouteVersion();
    this->removeSuccessMigrationId(progress.taskid());
    this->updateInTransaction(transReq);

    auto transResponse = mKVEngine->trans(transReq);
    header = transResponse.header();
    if (header.code() != proto::OK) {
      SPDLOG_WARN("ClusterSetManager::handleMigrationSuccess, transaction return code {}", header.code());
      return {nullptr, nullptr};
    }
    return {pSourceCluster, pTargetCluster};
  }

  void ClusterSetManager::handleRouteInfoRequest(ResponseHeader &header, RouteInfo &routeInfo) {
    buildHeader(header, fillRouteInfo(routeInfo));
  }

  void ClusterSetManager::removeCluster(ResponseHeader &header, const ServerAddressList &addresses) {
    SPDLOG_WARN("ClusterSetManager::removeCluster, not implemented...");
    buildHeader(header, proto::OK);
  }

  void ClusterSetManager::beginMigration(const RouteInfo &routeInfo, const MigrationTask &migrationTask) {
    proto::StartMigration::Request startMigrationRequest;
    *startMigrationRequest.mutable_routeinfo() = routeInfo;
    *startMigrationRequest.mutable_migrationtask() = migrationTask;
    auto pSourceCluster = this->findClusterByAddress(migrationTask.sourcecluster().servers());
    auto pTargetCluster = this->findClusterByAddress(migrationTask.targetcluster().servers());

    SPDLOG_INFO("ClusterSetManager::beginMigration, migration taskId {}, migration shard size {}, "
                "source cluster {}, target cluster {}",
                migrationTask.taskid(), migrationTask.migrateshards_size(),
                migrationTask.sourcecluster().servers(0).hostname(),
                migrationTask.targetcluster().servers(0).hostname());

    notifyStartMigration(startMigrationRequest, *pSourceCluster, *pTargetCluster);
  }

  void ClusterSetManager::endMigration(Cluster *pSourceCluster, Cluster *pTargetCluster) {
    assert(pSourceCluster != nullptr);
    assert(pTargetCluster != nullptr);

    proto::EndMigration_Request endMigrationRequest;
    this->fillRouteInfo(*endMigrationRequest.mutable_routeinfo());
    notifyEndMigration(endMigrationRequest, *pSourceCluster);
    notifyEndMigration(endMigrationRequest, *pTargetCluster);
  }

  proto::ResponseCode ClusterSetManager::fillRouteInfo(RouteInfo& routeInfo) {
    SPDLOG_INFO("ClusterSetManager::fillRouteInfo, begin to fill route info");
    routeInfo.set_version(this->getRouteVersion());
    std::vector<Cluster*> clusterVec;
    this->getRoutingCluster(clusterVec);

    auto hashRing = route::HashRing::instance();
    for (auto cluster : clusterVec) {
      SPDLOG_INFO("ClusterSetManager::fillRouteInfo, add cluster {} in hash ring", cluster->getClusterId());
      hashRing->addClusterId(cluster->getClusterId());
    }

    auto placement = routeInfo.mutable_placementinfo();
    placement->set_strategy(proto::Placement::CONSIST_HASHING);
    auto allCluster = placement->mutable_allclusters();

    auto placementMap = placement->mutable_placementmap();
    std::map<ClusterIdType, std::vector<route::ShardIdType>> shardMap;
    hashRing->extractShardConfig(shardMap);
    SPDLOG_INFO("ClusterSetManager::fillRouteInfo, shardMap size {}", shardMap.size());

    for (auto cluster : clusterVec) {
      auto c = allCluster->add_clusters();
      auto clusterWithShard = placementMap->add_clusterwithshards();
      auto mc = clusterWithShard->mutable_cluster();
      for (auto &server : cluster->getServerAddresses()) {
        mc->add_servers()->set_hostname(server.hostname());
        c->add_servers()->set_hostname(server.hostname());
      }
      auto& shardVector = shardMap[cluster->getClusterId()];
      for (auto shard : shardVector) {
        clusterWithShard->add_shards()->set_id(shard);
      }
    }

    auto partition = routeInfo.mutable_partitioninfo();
    partition->set_strategy(proto::Partition_PartitionStrategy_HASH_MOD);
    auto[start, end] = hashRing->getHashRange();
    while (start < end) {
      partition->add_allshards()->set_id(start++);
    }

    SPDLOG_INFO("ClusterSetManager::fillRouteInfo, route version: {}, shard size: {}, "
                "strategy: {}, cluster size: {}, cluster with shard size: {}",
                routeInfo.version(), routeInfo.partitioninfo().allshards_size(),
                routeInfo.partitioninfo().strategy(),
                routeInfo.placementinfo().allclusters().clusters_size(),
                routeInfo.placementinfo().placementmap().clusterwithshards_size());
    return proto::OK;
  }

  proto::ResponseCode ClusterSetManager::fillRouteAndMigrationInfo(RouteInfo& routeInfo, MigrationTask& migrationTask) {
    auto code = this->fillRouteInfo(routeInfo);
    if (code != proto::OK) {
      SPDLOG_WARN("ClusterSetManager::fillRouteAndMigrationInfo, fillRouteInfo failed, return code: {}", code);
      return code;
    }

    if (!mMigrationVector.empty()) {
      for (auto &m : mMigrationVector) {
        migrationTask = m->getMigrationTask();
        /// currently, there are only one migration in process
        break;
      }
    }
    return proto::OK;
  }

  proto::ResponseCode ClusterSetManager::onboardCluster(Cluster& cluster) {
    std::vector<Cluster*> clusterVec;
    this->getRoutingCluster(clusterVec);

    auto hashRing = route::HashRing::instance();
    for (auto& c : clusterVec) {
      hashRing->addClusterId(c->getClusterId());
    }
    std::map<ClusterIdType, std::vector<ShardIdType>> clusterShardMap;
    hashRing->addClusterId(cluster.getClusterId(), &clusterShardMap);
    assert(!clusterShardMap.empty());

    proto::Transaction_Request transReq;
    this->fillPrecondition(transReq);
    cluster.updateInTransaction(proto::Cluster_ClusterStatus_IN_SERVICE, transReq);

    // Skip migration as only static sharding is supported currently
    // to-do: support dynamic sharding
    /*
    auto it = clusterShardMap.begin();
    while (it != clusterShardMap.end()) {
      MigrationTask migrationTask;
      migrationTask.set_taskid(this->assignMigrationTaskId());
      migrationTask.set_overallstatus(proto::Migration_MigrationStatus_NOT_START);
      auto pSourceCluster = findClusterById(it->first);
      pSourceCluster->updateInTransaction(proto::Cluster_ClusterStatus_EXPORTING, transReq);

      fillAddressInfo(migrationTask.mutable_sourcecluster(), pSourceCluster);
      fillAddressInfo(migrationTask.mutable_targetcluster(), &cluster);
      fillShardInfo(migrationTask, it->second);
      Migration::createInTransaction(migrationTask, transReq);

      /// update global info
      mClusterSetGlobalInfo.add_migrationtaskids(migrationTask.taskid());
      mMigrationVector.push_back(std::make_unique<Migration>(migrationTask, VersionType{}, mKVEngine));

      it++;
    } */
    /// update global info since migration id info changed
    this->updateInTransaction(transReq);

    return mKVEngine->trans(transReq).header().code();
  }

  ClusterIdType ClusterSetManager::assignNewClusterId() const {
    auto clusterSize = mClusterSetGlobalInfo.clusterids_size();
    return clusterSize == 0 ? 1 : mClusterSetGlobalInfo.clusterids(clusterSize - 1) + 1;
  }

  Cluster* ClusterSetManager::findClusterById(ClusterIdType clusterId) const {
    for (int i = 0, size = mClusterSetGlobalInfo.clusterids_size(); i < size; ++i) {
      if (clusterId == mClusterSetGlobalInfo.clusterids(i)) {
        return mClusterVector[i].get();
      }
    }
    return nullptr;
  }

  Cluster* ClusterSetManager::findClusterByAddress(const ServerAddressList &addresses) const {
    assert(!addresses.empty());
    auto hostname0 = addresses.begin()->hostname();
    for (auto &cluster : mClusterVector) {
      for (auto &srv : cluster->getServerAddresses()) {
        if (srv.hostname() == hostname0) {
          return cluster.get();
        }
      }
    }
    return nullptr;
  }

  Migration* ClusterSetManager::findMigrationById(MigrationTaskIdType taskId) const {
    for (int i = 0, size = mClusterSetGlobalInfo.migrationtaskids_size(); i < size; ++i) {
      if (taskId == mClusterSetGlobalInfo.migrationtaskids(i)) {
        return mMigrationVector[i].get();
      }
    }
    return nullptr;
  }

  void ClusterSetManager::getRoutingCluster(std::vector<Cluster*>& vec) {
    for (auto&c : mClusterVector) {
      if (c->getStatus() == proto::Cluster_ClusterStatus_EXPORTING
          || c->getStatus() == proto::Cluster_ClusterStatus_IN_SERVICE) {
        vec.push_back(c.get());
      }
    }
  }

  MigrationTaskIdType ClusterSetManager::assignMigrationTaskId() {
    mClusterSetGlobalInfo.set_currentmaxmigrationtaskid(mClusterSetGlobalInfo.currentmaxmigrationtaskid() + 1);
    return mClusterSetGlobalInfo.currentmaxmigrationtaskid();
  }

  void ClusterSetManager::increaseGlobalRouteVersion() {
    mClusterSetGlobalInfo.set_routeversion(mClusterSetGlobalInfo.routeversion() + 1);
  }

  void ClusterSetManager::updateInTransaction(proto::Transaction_Request &transReq) {
    auto pWriteEntry = transReq.add_entries()->mutable_writeentry();
    pWriteEntry->set_key(KEY_CLUSTER_GLOBAL_INFO);
    pWriteEntry->set_value(mClusterSetGlobalInfo.SerializeAsString());
  }

  void ClusterSetManager::fillAddressInfo(proto::Cluster_ClusterAddress* pAddr, Cluster *cluster) {
    assert(pAddr && cluster);
    for (auto &address : cluster->getServerAddresses()) {
      pAddr->add_servers()->set_hostname(address.hostname());
    }
  }

  void ClusterSetManager::fillShardInfo(MigrationTask &migrationTask, const std::vector<ShardIdType>& shardVec) {
    assert(!shardVec.empty());
    for (auto shard : shardVec) {
      auto migrateShard = migrationTask.add_migrateshards();
      migrateShard->set_status(proto::Migration_MigrationShardStatus_INITED);
      migrateShard->mutable_shardinfo()->set_id(shard);
    }
  }

  proto::ResponseCode ClusterSetManager::appendNewClusterId(ClusterIdType clusterId) {
    for (auto id : mClusterSetGlobalInfo.clusterids()) {
      if (id == clusterId) {
        return proto::BAD_REQUEST;
      }
    }
    mClusterSetGlobalInfo.add_clusterids(clusterId);
    return proto::OK;
  }

  void ClusterSetManager::removeSuccessMigrationId(MigrationTaskIdType taskId) {
    std::vector<MigrationTaskIdType> taskIdVector;
    for (auto &id : mClusterSetGlobalInfo.migrationtaskids()) {
      if (id != taskId) {
        taskIdVector.push_back(id);
        continue;
      }
      SPDLOG_INFO("ClusterSetManager::removeSuccessMigrationId, migration task id {} removed", taskId);
    }
    mClusterSetGlobalInfo.clear_migrationtaskids();
    for (auto &id : taskIdVector) {
      mClusterSetGlobalInfo.add_migrationtaskids(id);
    }
  }

  void ClusterSetManager::notifyStartMigration(proto::StartMigration_Request &request,
                                               Cluster &sourceCluster, Cluster &targetCluster) {
    auto sourceClusterId = sourceCluster.getClusterId();
    auto sourceClusterStatus = sourceCluster.getStatus();
    request.set_status(sourceClusterStatus);
    auto targetClusterId = targetCluster.getClusterId();
    auto targetClusterStatus = targetCluster.getStatus();

    SPDLOG_INFO("begin to notify start migration from cluster {} in status {} to cluster {} in status {}",
                sourceClusterId, sourceClusterStatus, targetClusterId, targetClusterStatus);
    migration::ClusterProxy::instance().startMigration(request, sourceClusterId);
  }

  void ClusterSetManager::notifyEndMigration(proto::EndMigration_Request& request, Cluster &cluster) {
    auto &clusterProxy = migration::ClusterProxy::instance();
    SPDLOG_INFO("begin to notify endMigration for cluster {}, route info version {}",
                cluster.getClusterId(), request.routeinfo().version());
    request.set_status(cluster.getStatus());
    clusterProxy.endMigration(request, cluster.getClusterId());
  }
}  ///  end namespace goblin::objectmanager::module
