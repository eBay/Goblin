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

#ifndef SERVER_SRC_OBJECT_STORE_OBJECTSTORE_H_
#define SERVER_SRC_OBJECT_STORE_OBJECTSTORE_H_

#include <tuple>

#include <INIReader.h>
#include <spdlog/spdlog.h>

#include "../kv-engine/utils/AppUtil.h"
#include "../kv-engine/KVEngine.h"
#include "../kv-engine/types.h"
#include "../kv-engine/utils/Status.h"
#include "../kv-engine/utils/StrUtil.h"
#include "../kv-engine/network/NetAdminServer.h"
#include "migration/MigrationManager.h"
#include "network/ObjectManagerClient.h"
#include "network/RequestReceiver.h"
#include "network/OSNetAdminServiceProvider.h"

namespace goblin {
namespace mock {
template <typename ClusterType>
class MockAppCluster;
}
}

namespace goblin::objectstore {

/// id1@ip1:port1,id2@ip2:port2,id3@ip3:port3
using ClusterAddressFormat = std::string;
using ShardIdType = uint64_t;
using RouteVersion = uint64_t;
static constexpr auto TRX_KEY_PREFIX = "/RMSTRX";
static const ShardIdType RMSTRX_SHARD_IDX = 1;

class ObjectStore final {
 public:
  explicit ObjectStore(const char *configPath);
  ~ObjectStore() = default;

  // disallow copy ctor and copy assignment
  ObjectStore(const ObjectStore &) = delete;
  ObjectStore &operator=(const ObjectStore &) = delete;

  // disallow move ctor and move assignment
  ObjectStore(ObjectStore &&) = delete;
  ObjectStore &operator=(ObjectStore &&) = delete;

  void run();

  void shutdown();

  kvengine::utils::Status triggerMigration(
      const proto::Cluster::ClusterStatus &status,
      const proto::RouteInfo &routeInfo,
      const proto::Migration::MigrationTask &task);
  kvengine::utils::Status reportMigration(
      const proto::Migration::MigrationProgress &progress);
  kvengine::utils::Status finishMigration(
      const proto::Cluster::ClusterStatus &status,
      const proto::RouteInfo &routeInfo,
      const proto::Migration::MigrationTask &task);

  static kvengine::store::WSName shardIdToColumnFamily(
      const std::string &prefixCFName,
      ShardIdType shardId,
      uint64_t factor) {
    auto wsId = shardId & ((1 << factor) - 1);
    return prefixCFName + std::to_string(wsId);
  }
  static ClusterAddressFormat clusterToAddressFormat(const proto::Cluster::ClusterAddress &addr) {
    ClusterAddressFormat addrFormat;
    auto size = addr.servers().size();
    auto i = 0;
    for (auto s : addr.servers()) {
      if (i != 0) {
        addrFormat += "," + s.hostname();
      } else {
        addrFormat += s.hostname();
      }
      i++;
    }
    return addrFormat;
  }
  static proto::Cluster::ClusterAddress addressFormatToCluster(const ClusterAddressFormat &addrFormat) {
    proto::Cluster::ClusterAddress clusterAddr;
    std::vector<std::string> addrs = kvengine::utils::StrUtil::tokenize(addrFormat, ',');
    for (auto addr : addrs) {
      auto server = clusterAddr.add_servers();
      server->set_hostname(addr);
    }
    return clusterAddr;
  }
  static proto::Cluster::ClusterAddress raftMembersToClusterAddress(
      uint64_t appPort,
      uint64_t selfId,
      const std::vector<gringofts::raft::MemberInfo> &raftMemberInfo) {
    std::vector<std::string> ips;
    std::vector<uint64_t> ports;
    assert(selfId <= raftMemberInfo.size());
    for (auto m : raftMemberInfo) {
      auto ipPort = kvengine::utils::StrUtil::tokenize(m.mAddress, ':');
      assert(ipPort.size() == 2);
      ips.push_back(ipPort[0]);
      ports.push_back(std::stoi(ipPort[1]));
    }
    proto::Cluster::ClusterAddress clusterAddr;
    auto basePort = ports[selfId - 1];
    for (auto i = 0; i < ips.size(); ++i) {
      auto &ip = ips[i];
      auto s = clusterAddr.add_servers();
      s->set_hostname(std::to_string(i + 1) + "@" + ip + ":" + std::to_string(appPort + ports[i] - basePort));
    }
    return clusterAddr;
  }
  static ShardIdType keyToShardId(const kvengine::store::KeyType &key) {
    auto hash = kvengine::utils::StrUtil::simpleHash(key.data());
    auto shardId = hash & ((1 << proto::Shard::DEFAULT) - 1);
    return shardId;
  }

  class GlobalInfo {
   public:
     class RouteInfoHelper {
      public:
         RouteVersion getRouteVersion() const {
           return mRouteInfo.version();
         }
         proto::Partition::PartitionStrategy getPartionStrategy() const {
           return mRouteInfo.partitioninfo().strategy();
         }
         bool checkIfInClusterScope(const ClusterAddressFormat &cluster, ShardIdType shardId) const {
           return mCluster2ShardIds.at(cluster).find(shardId) != mCluster2ShardIds.at(cluster).end();
         }
         goblin::objectstore::ClusterAddressFormat getClusterByShardId(const ShardIdType shardId) const {
           for (auto it = mCluster2ShardIds.begin(); it != mCluster2ShardIds.end(); ++it) {
             if (it->second.find(shardId) != it->second.end()) {
               return it->first;
             }
           }
           return goblin::objectstore::ClusterAddressFormat();
         }
         void updateRouteInfo(const proto::RouteInfo &info) {
           mRouteInfo = info;
           updateShardMap();
         }

      private:
         void updateShardMap() {
           auto &placement = mRouteInfo.placementinfo().placementmap();
           for (auto clusterWithShards : placement.clusterwithshards()) {
             auto &shards = clusterWithShards.shards();
             auto &cluster = clusterWithShards.cluster();
             auto addrFormat = clusterToAddressFormat(cluster);
             SPDLOG_INFO("global info update: cluster {} - shard size {}", addrFormat, shards.size());
             for (auto shard : shards) {
               auto id = shard.id();
               SPDLOG_INFO("global info update: version {}, cluster {} shard {}", mRouteInfo.version(), addrFormat, id);
               mCluster2ShardIds[addrFormat].insert(id);
             }
           }
         }
         proto::RouteInfo mRouteInfo;
         std::map<ClusterAddressFormat, std::set<ShardIdType>> mCluster2ShardIds;  /// translated by route info
     };
     class MigrationInfoHelp {
      public:
         ClusterAddressFormat getSourceCluster() const {
           return clusterToAddressFormat(mMigrationTask.sourcecluster());
         }
         ClusterAddressFormat getTargetCluster() const {
           return clusterToAddressFormat(mMigrationTask.targetcluster());
         }
         bool checkIfInMigrationShards(ShardIdType shardId) {
           return mShard2Status.find(shardId) != mShard2Status.end();
         }
         void updateMigrationTask(const proto::Migration::MigrationTask &task) {
           mMigrationTask = task;
           updateMigrationMap();
         }

      private:
         void updateMigrationMap() {
           SPDLOG_INFO("global info update: migration shard size {}", mMigrationTask.migrateshards().size());
           for (auto shard : mMigrationTask.migrateshards()) {
             SPDLOG_INFO("global info update: migration shard {}, status {}", shard.shardinfo().id(), shard.status());
             mShard2Status[shard.shardinfo().id()] = shard.status();
           }
         }
         proto::Migration::MigrationTask mMigrationTask;
         std::map<ShardIdType, proto::Migration::MigrationShardStatus> mShard2Status;
     };

     void updateLatestInfo(
         const proto::Cluster::ClusterAddress clusterAddr,
         const proto::Cluster::ClusterStatus &status,
         const proto::RouteInfo &info,
         const proto::Migration::MigrationTask &task) {
       std::unique_lock<std::shared_mutex> lock(mMutex);
       mMyClusterAddrFormat = clusterToAddressFormat(clusterAddr);
       mStatus = status;
       SPDLOG_INFO("global info update: addr {}, status {}", mMyClusterAddrFormat, status);
       mRouteInfoHelper.updateRouteInfo(info);
       mMigrationInfoHelper.updateMigrationTask(task);

       mCachedMyClusterAddrFormat = clusterToAddressFormat(clusterAddr);
       mCachedStatus = status;
       mCachedRouteInfoHelper.updateRouteInfo(info);
       mCachedMigrationInfoHelper.updateMigrationTask(task);
     }
     void updateLatestInfo(
         const proto::Cluster::ClusterStatus &status,
         const proto::RouteInfo &info,
         const proto::Migration::MigrationTask &task) {
       std::unique_lock<std::shared_mutex> lock(mMutex);
       mStatus = status;
       SPDLOG_INFO("global info update: addr {}, status {}", mMyClusterAddrFormat, status);
       mRouteInfoHelper.updateRouteInfo(info);
       mMigrationInfoHelper.updateMigrationTask(task);

       mCachedStatus = status;
       mCachedRouteInfoHelper.updateRouteInfo(info);
       mCachedMigrationInfoHelper.updateMigrationTask(task);
     }
     void updateLatestRouteInfo(const proto::RouteInfo &info) {
       std::unique_lock<std::shared_mutex> lock(mMutex);
       mRouteInfoHelper.updateRouteInfo(info);

       mCachedRouteInfoHelper.updateRouteInfo(info);
     }

     void getLatestInfo(
         ClusterAddressFormat *myClusterAddrFormat,
         proto::Cluster::ClusterStatus *status,
         RouteInfoHelper *routeInfoHelper,
         MigrationInfoHelp *migrationTaskHelper) const {
       std::shared_lock<std::shared_mutex> lock(mMutex);
       *myClusterAddrFormat = mMyClusterAddrFormat;
       *status = mStatus;
       *routeInfoHelper = mRouteInfoHelper;
       *migrationTaskHelper = mMigrationInfoHelper;
     }

     /// the fastest way to get global info, but not guarantee to be latest
     void getCachedInfo(
         ClusterAddressFormat **myClusterAddrFormat,
         proto::Cluster::ClusterStatus **status,
         RouteInfoHelper **routeInfoHelper,
         MigrationInfoHelp **migrationTaskHelper) {
       std::shared_lock<std::shared_mutex> lock(mMutex);
       *myClusterAddrFormat = &mCachedMyClusterAddrFormat;
       *status = &mCachedStatus;
       *routeInfoHelper = &mCachedRouteInfoHelper;
       *migrationTaskHelper = &mCachedMigrationInfoHelper;
     }

     ClusterAddressFormat getMyClusterAddrFormat() {
       std::shared_lock<std::shared_mutex> lock(mMutex);
       return mMyClusterAddrFormat;
     }

   private:
     ClusterAddressFormat mMyClusterAddrFormat;
     proto::Cluster::ClusterStatus mStatus = proto::Cluster::INIT;
     RouteInfoHelper mRouteInfoHelper;
     MigrationInfoHelp mMigrationInfoHelper;

     /// TODO(qiacai): remove these cache
     ClusterAddressFormat mCachedMyClusterAddrFormat;
     proto::Cluster::ClusterStatus mCachedStatus = proto::Cluster::INIT;
     RouteInfoHelper mCachedRouteInfoHelper;
     MigrationInfoHelp mCachedMigrationInfoHelper;

     mutable std::shared_mutex mMutex;
  };

  proto::Cluster::ClusterStatus getCurClusterStatus() const {
    ClusterAddressFormat dummy1;
    proto::Cluster::ClusterStatus status;
    GlobalInfo::RouteInfoHelper dummy2;
    GlobalInfo::MigrationInfoHelp dummy3;
    mGlobalInfo.getLatestInfo(&dummy1, &status, &dummy2, &dummy3);
    return status;
  }
  /// in total, we have 2 ^ 4 = 16 working sets
  static constexpr const uint64_t kWorkingSetFactor = 4;
  static constexpr const char* kWorkingSetNamePrefix = "cf_for_app";

 private:
  void initMonitor(const INIReader &reader);

  void startRequestReceiver();

  void startNetAdminServer();

  void startPostServerLoop();

  kvengine::utils::Status becomeLeaderCallBack(
      const std::vector<gringofts::raft::MemberInfo> &clusterInfo);
  kvengine::utils::Status preExecuteCallBack(
      kvengine::model::CommandContext &context,  // NOLINT(runtime/references)
      kvengine::store::KVStore &kvStore);  // NOLINT(runtime/references)
  kvengine::utils::Status refreshRouteInfo();

  std::shared_ptr<kvengine::KVEngine> mKVEngine;
  std::unique_ptr<network::RequestReceiver> mRequestReceiver;
  std::unique_ptr<kvengine::network::NetAdminServer> mNetAdminService;

  std::unique_ptr<network::ObjectManagerClient> mObjectManagerClient;
  std::unique_ptr<migration::MigrationManager> mMigrationManager;
  GlobalInfo mGlobalInfo;

  INIReader mReader;
  std::atomic<bool> mRefreshingRouteInfo = false;
  bool mIsShutdown;

  /// for UT
  ObjectStore(const char *configPath,
      std::shared_ptr<kvengine::KVEngine> engine);
  template <typename ClusterType>
  friend class mock::MockAppCluster;
};
}  /// namespace goblin::objectstore

#endif  // SERVER_SRC_OBJECT_STORE_OBJECTSTORE_H_
