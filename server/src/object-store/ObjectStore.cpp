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

#include "ObjectStore.h"

#include <future>

#include <infra/raft/RaftInterface.h>

#include "model/StartupCommand.h"
#include "../kv-engine/utils/FileUtil.h"
#include "watch/WatchObserver.h"

namespace goblin::objectstore {

ObjectStore::ObjectStore(const char *configPath) :
  mIsShutdown(false),
  mReader(configPath) {
  SPDLOG_INFO("current working dir: {}", kvengine::utils::FileUtil::currentWorkingDir());

  if (mReader.ParseError() < 0) {
    SPDLOG_WARN("Cannot load config file {}, exiting", configPath);
    throw std::runtime_error("Cannot load config file");
  }

  std::vector<kvengine::store::WSName> allWS;
  auto wsNum = 1 << kWorkingSetFactor;
  for (auto i = 0; i < wsNum; ++i) {
    allWS.push_back(kWorkingSetNamePrefix + std::to_string(i));
  }
  auto wsLookupFunc = [](const kvengine::store::KeyType &key) {
    auto shardId = keyToShardId(key);
    auto wsName = shardIdToColumnFamily(kWorkingSetNamePrefix, shardId, kWorkingSetFactor);
    return wsName;
  };
  auto becomeLeaderFunc = [this](
      const std::vector<gringofts::raft::MemberInfo> &clusterInfo) {
    return this->becomeLeaderCallBack(clusterInfo);
  };
  auto preExecuteFunc = [this](kvengine::model::CommandContext &context, kvengine::store::KVStore &kvStore) {
    return this->preExecuteCallBack(context, kvStore);
  };
  mKVEngine = std::make_shared<kvengine::KVEngine>();
  mKVEngine->Init(
    configPath,
    allWS,
    wsLookupFunc,
    becomeLeaderFunc,
    preExecuteFunc,
    std::make_shared<watch::WatchObserver>());

  mRequestReceiver = std::make_unique<network::RequestReceiver>(mReader, *mKVEngine, *this);

  mNetAdminService = std::make_unique<kvengine::network::NetAdminServer>
                        (mReader, std::make_shared<network::OSNetAdminServiceProvider>(mKVEngine));

  mObjectManagerClient = std::make_unique<network::ObjectManagerClient>(mReader);
}

kvengine::utils::Status ObjectStore::becomeLeaderCallBack(
      const std::vector<gringofts::raft::MemberInfo> &clusterInfo) {
  proto::OnStartup::Request req;
  auto ipPortStr = mReader.Get("receiver", "ip.port", "UNKNOWN");
  auto ipWithPort = kvengine::utils::StrUtil::tokenize(ipPortStr, ':');
  assert(ipWithPort.size() == 2);

  /// TODO: remove these
  // std::string raftConfigPath = mReader.Get("store", "raft.config.path", "UNKNOWN");
  // assert(raftConfigPath != "UNKNOWN");
  // INIReader raftReader(raftConfigPath);
  // auto selfId = raftReader.GetInteger("raft.default", "self.id", -1);
  // if (selfId == -1) {
  //   selfId = mKVEngine->getSelfMemId();
  // }
  auto selfId = mKVEngine->getSelfMemId();
  assert(selfId != -1);
  SPDLOG_INFO("getting selfId={} before os reports startup", selfId);

  *req.mutable_cluster() = raftMembersToClusterAddress(std::stoi(ipWithPort[1]), selfId, clusterInfo);
  SPDLOG_INFO("report startup after on power");
  while (true) {
    auto promise = std::make_shared<std::promise<proto::OnStartup::Response>>();
    auto future = promise->get_future();
    mObjectManagerClient->reportStartup(req, [promise](const proto::OnStartup::Response &resp) {
        promise->set_value(resp);
        });
    auto resp = future.get();
    if (resp.header().code() != proto::ResponseCode::OK) {
      SPDLOG_INFO("keep retry reporting {}", resp.header().code());
      continue;
    }
    mGlobalInfo.updateLatestInfo(req.cluster(), resp.status(), resp.routeinfo(), resp.migrationtask());
    break;
  }
  return kvengine::utils::Status::ok();
}

kvengine::utils::Status ObjectStore::preExecuteCallBack(
    kvengine::model::CommandContext &context,
    kvengine::store::KVStore &kvStore) {
  auto startTime = gringofts::TimeUtil::currentTimeInNanos();
  auto header = context.getRequestHeader();
  ClusterAddressFormat *myClusterAddrFormat;
  proto::Cluster::ClusterStatus *status = nullptr;
  GlobalInfo::RouteInfoHelper *routeInfo = nullptr;
  GlobalInfo::MigrationInfoHelp *migratingTask = nullptr;
  /// copy current global info
  mGlobalInfo.getCachedInfo(&myClusterAddrFormat, &status, &routeInfo, &migratingTask);
  if (routeInfo->getRouteVersion() > header.routeversion()) {
    SPDLOG_WARN("client version: {}, server version: {}", header.routeversion(), routeInfo->getRouteVersion());
    return kvengine::utils::Status::clientRouteOutOfDate("client needs to refresh route info");
  }
  if (routeInfo->getRouteVersion() < header.routeversion()) {
    /// kick off an async request to update server route info
    SPDLOG_WARN("client version: {}, server version: {}", header.routeversion(), routeInfo->getRouteVersion());
    refreshRouteInfo();
    return kvengine::utils::Status::serverRouteOutOfDate("server refreshing route, please try again");
  }
  if (*status == proto::Cluster::OUT_OF_SERVICE) {
    return kvengine::utils::Status::outOfService("server is not in good state");
  }
  if (routeInfo->getPartionStrategy() != proto::Partition::HASH_MOD) {
    return kvengine::utils::Status::notSupported("invalid partition strategy");
  }
  auto routeTime = gringofts::TimeUtil::currentTimeInNanos();
  auto keys = context.getTargetKeys();

  bool isMiniTrxKey = true;
  for (auto key : keys) {
    isMiniTrxKey &= absl::StartsWith(key, TRX_KEY_PREFIX);
    if (!isMiniTrxKey) {
      break;
    }
  }
  if (isMiniTrxKey) {
    SPDLOG_INFO("debug: All the {} keys are trx keys", keys.size());
  }

  /// filter out keys that are not in the scope of current node
  for (auto key : keys) {
    auto shardId = keyToShardId(key);
    // shard id for trx is hardcoded here. It's better to move the code to keyToShardId, but now keyToShardId() is also
    // used to calculate column family. So its logic cannot be changed, otherwise column family lookup may return non-
    // compatible value.
    if (isMiniTrxKey) {
      shardId = RMSTRX_SHARD_IDX;
    }
    if (!routeInfo->checkIfInClusterScope(*myClusterAddrFormat, shardId)) {
      if (*status == proto::Cluster::IMPORTING) {
        SPDLOG_INFO("debug: importing for shard {} in {}", shardId, *myClusterAddrFormat);
        if (migratingTask->checkIfInMigrationShards(shardId)) {
          /// this key is migrating to current node
          continue;
        }
      }
      /// one of the keys doesn't belong to this node
      /// TODO: support cross node transactions
      SPDLOG_ERROR("Key {} should be routed to shard {} in cluster {}, but was routed to this cluster {} wrongly. ",
          key, shardId, routeInfo->getClusterByShardId(shardId), *myClusterAddrFormat);
      return kvengine::utils::Status::wrongRoute("routed to a wrong cluster");
    } else if (*status == proto::Cluster::EXPORTING) {
      if (migratingTask->checkIfInMigrationShards(shardId)) {
        SPDLOG_INFO("debug: exporting for shard {} in {}", shardId, *myClusterAddrFormat);
        /// this key is migrating to another node
        proto::Meta meta;
        auto s = kvStore.readMeta(key, &meta);
        if (s.isNotFound()) {
          /// this key is already migrated to another node
          /// we should tell the client to redirect to another node
          std::string routeHint = migratingTask->getTargetCluster();
          return kvengine::utils::Status::migratedRoute(routeHint);
        }
      }
    }
  }
  auto endTime = gringofts::TimeUtil::currentTimeInNanos();
  /// SPDLOG_INFO("debug: route time: {}, preExe time: {}",
      /// (routeTime - startTime)/ 1000000.0, (endTime - startTime)/ 1000000.0);
  return kvengine::utils::Status::ok();
}

kvengine::utils::Status ObjectStore::refreshRouteInfo() {
  if (mRefreshingRouteInfo) {
    SPDLOG_INFO("already refreshing route info");
    return kvengine::utils::Status::ok();
  }
  SPDLOG_INFO("refreshing route info");
  mRefreshingRouteInfo = true;
  proto::Router::Request request;
  mObjectManagerClient->requestRouteInfo(request, [this](const proto::Router::Response &resp) {
      this->mGlobalInfo.updateLatestRouteInfo(resp.routeinfo());
      this->mRefreshingRouteInfo = false;
      });
  return kvengine::utils::Status::ok();
}

kvengine::utils::Status ObjectStore::triggerMigration(
    const proto::Cluster::ClusterStatus &status,
    const proto::RouteInfo &routeInfo,
    const proto::Migration::MigrationTask &task) {
  if (mMigrationManager) {
    return kvengine::utils::Status::notSupported("already has a migration task running");
  }

  /// send a get command to see if I am the leader
  /// TODO: create a new cmd for leadership check
  proto::Get::Request getRequest;
  getRequest.mutable_entry()->set_key("dummykeytocheckleadership");
  auto getResponse = mKVEngine->get(getRequest);
  auto code = getResponse.header().code();
  if (code == proto::ResponseCode::NOT_LEADER) {
    return kvengine::utils::Status::notLeader();
  }
  /// assert(code == proto::ResponseCode::OK);
  SPDLOG_INFO("trigger a new migration for {}, migration task {}", mGlobalInfo.getMyClusterAddrFormat(), task.taskid());

  mMigrationManager = std::make_unique<migration::MigrationManager>(
      mReader, *mKVEngine, [this](const proto::Migration::MigrationProgress &progress) {
      this->reportMigration(progress);
      });
  /// update the global info before kick off the migration
  mGlobalInfo.updateLatestInfo(status, routeInfo, task);
  mMigrationManager->startMigration(task);
  return kvengine::utils::Status::ok();
}

kvengine::utils::Status ObjectStore::reportMigration(const proto::Migration::MigrationProgress &progress) {
  proto::OnMigration::Request req;
  *req.mutable_progress() = progress;
  mObjectManagerClient->reportMigrationProgress(req, [](const proto::OnMigration::Response &) {});
  return kvengine::utils::Status::ok();
}

kvengine::utils::Status ObjectStore::finishMigration(
    const proto::Cluster::ClusterStatus &status,
    const proto::RouteInfo &routeInfo,
    const proto::Migration::MigrationTask &task) {
  /// send a get command to see if I am the leader
  /// TODO: create a new cmd for leadership check
  proto::Get::Request getRequest;
  getRequest.mutable_entry()->set_key("dummykeytocheckleadership");
  auto getResponse = mKVEngine->get(getRequest);
  auto code = getResponse.header().code();
  if (code == proto::ResponseCode::NOT_LEADER) {
    return kvengine::utils::Status::notLeader();
  }
  auto curStatus = getCurClusterStatus();
  SPDLOG_INFO("finish a migration for {}, migration task {}, cur status {}, new status {}",
      mGlobalInfo.getMyClusterAddrFormat(), task.taskid(), curStatus, status);
  if (curStatus == proto::Cluster::EXPORTING && mMigrationManager
      || curStatus == proto::Cluster::IMPORTING && !mMigrationManager) {
    if (mMigrationManager) {
      mMigrationManager->endMigration();
      mMigrationManager = nullptr;
    }
    /// update the global info after shutting down the migration
    mGlobalInfo.updateLatestInfo(status, routeInfo, task);
    return kvengine::utils::Status::ok();
  }
  return kvengine::utils::Status::notSupported("no on-going migration");
}

void ObjectStore::startRequestReceiver() {
  mRequestReceiver->run();
}

void ObjectStore::startNetAdminServer() {
  mNetAdminService->run();
}

void ObjectStore::run() {
  SPDLOG_INFO("ObjectStore starts running");
  if (mIsShutdown) {
    SPDLOG_WARN("ObjectStore is already down. Will not run again.");
  } else {
    startRequestReceiver();
    startNetAdminServer();
    uint32_t maxStartupCmdRetry = 10000000;
    auto status = proto::Cluster::INIT;
    while (!mIsShutdown && maxStartupCmdRetry-- > 0) {
      status = getCurClusterStatus();
      if (status == proto::Cluster::INIT) {
        /// run a startup command in order to update latest global info when leader is on power
        sleep(1);
        mKVEngine->exeCustomCommand(std::make_shared<model::StartupCommand>());
      } else {
        break;
      }
    }
    SPDLOG_INFO("ObjectStore current status: {}", status);
    mRequestReceiver->join();
    mNetAdminService->shutdown();
  }
  SPDLOG_INFO("ObjectStore finishes running");
}

void ObjectStore::shutdown() {
  SPDLOG_INFO("Shutting down ObjectStore");
  if (mIsShutdown) {
    SPDLOG_INFO("ObjectStore is already down");
  } else {
    mRequestReceiver->shutdown();
    mNetAdminService->shutdown();
    mKVEngine->Destroy();
    mIsShutdown = true;
  }
}

ObjectStore::ObjectStore(
    const char *configPath,
    std::shared_ptr<kvengine::KVEngine> engine) :
  mIsShutdown(false),
  mReader(configPath) {
  if (mReader.ParseError() < 0) {
    SPDLOG_WARN("Cannot load config file {}, exiting", configPath);
    throw std::runtime_error("Cannot load config file");
  }
  /// this mock engine should be inited
  mKVEngine = engine;
  mRequestReceiver = std::make_unique<network::RequestReceiver>(mReader, *mKVEngine, *this);
  mNetAdminService = std::make_unique<kvengine::network::NetAdminServer>
                        (mReader, std::make_shared<network::OSNetAdminServiceProvider>(mKVEngine));

  mObjectManagerClient = std::make_unique<network::ObjectManagerClient>(mReader);
}

}  /// namespace goblin::objectstore
