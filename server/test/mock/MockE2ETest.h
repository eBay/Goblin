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

#ifndef SERVER_TEST_MOCK_MOCKE2ETEST_H_
#define SERVER_TEST_MOCK_MOCKE2ETEST_H_

#include <future>

#include <gtest/gtest-spi.h>
#include <infra/util/Util.h>

#include "../../src/object-manager/ObjectManager.h"
#include "../../src/object-store/ObjectStore.h"
#include "../../src/object-store/network/ObjectManagerClient.h"
#include "../mock/MockAppClient.h"
#include "../mock/MockAppCluster.h"
#include "../mock/MockNetAdminClient.h"

namespace goblin::mock {

using gringofts::raft::MemberInfo;

using ServerAddress = std::string;

class MockE2ETest : public ::testing::Test {
 protected:
  explicit MockE2ETest(uint64_t clusterNum): mClusterNum(clusterNum) {}
  void SetUp() override {
     SPDLOG_INFO("setting up {} store clusters with 1 manager cluster", mClusterNum);
     assert(mClusterNum <= 3);
     gringofts::Util::executeCmd("rm -rf ../test/output");
     gringofts::Util::executeCmd("mkdir -p ../test/output/" + mObjectManagerFolderPrefix + "/" + mServer1Name);
     gringofts::Util::executeCmd("mkdir -p ../test/output/" + mObjectManagerFolderPrefix + "/" + mServer2Name);
     gringofts::Util::executeCmd("mkdir -p ../test/output/" + mObjectManagerFolderPrefix + "/" + mServer3Name);
     for (auto i = 0; i < mClusterNum; ++i) {
       gringofts::Util::executeCmd(
           "mkdir -p ../test/output/" + mObjectStoreFolderPrefix + std::to_string(i + 1) + "/" + mServer1Name);
       gringofts::Util::executeCmd(
           "mkdir -p ../test/output/" + mObjectStoreFolderPrefix + std::to_string(i + 1) + "/" + mServer2Name);
       gringofts::Util::executeCmd(
           "mkdir -p ../test/output/" + mObjectStoreFolderPrefix + std::to_string(i + 1) + "/" + mServer3Name);
     }
     SPDLOG_INFO("initializing object manager servers");
     mObjectManagerCluster = std::make_unique<mock::MockAppCluster<objectmanager::ObjectManager>>();
     mObjectManagerCluster->setupAllServers({
         mConfFolderPrefix + mObjectManagerFolderPrefix + "/" + mServer1Name + ".ini",
         mConfFolderPrefix + mObjectManagerFolderPrefix + "/" + mServer2Name + ".ini",
         mConfFolderPrefix + mObjectManagerFolderPrefix + "/" + mServer3Name + ".ini"});
     SPDLOG_INFO("initializing object store servers");
     for (auto i = 0; i < mClusterNum; ++i) {
       std::vector<MemberInfo> clusterMembers(3);
       INIReader reader1(
           mConfFolderPrefix + mObjectStoreFolderPrefix + std::to_string(i + 1) + "/" + mServer1Name + ".ini");
       const auto server1Addr = reader1.Get("receiver", "ip.port", "UNKNOWN");
       const auto server1NetAdminAddr = reader1.Get("netadmin", "ip.port", "UNKNOWN");
       clusterMembers[0].mId = 1;
       clusterMembers[0].mAddress = server1Addr;
       member2NetadminAddr[server1Addr] = server1NetAdminAddr;
       INIReader reader2(
           mConfFolderPrefix + mObjectStoreFolderPrefix + std::to_string(i + 1) + "/" + mServer2Name + ".ini");
       const auto server2Addr = reader2.Get("receiver", "ip.port", "UNKNOWN");
       const auto server2NetAdminAddr = reader2.Get("netadmin", "ip.port", "UNKNOWN");
       clusterMembers[1].mId = 2;
       clusterMembers[1].mAddress = server2Addr;
       member2NetadminAddr[server2Addr] = server2NetAdminAddr;
       INIReader reader3(
           mConfFolderPrefix + mObjectStoreFolderPrefix + std::to_string(i + 1) + "/" + mServer3Name + ".ini");
       const auto server3Addr = reader3.Get("receiver", "ip.port", "UNKNOWN");
       const auto server3NetAdminAddr = reader3.Get("netadmin", "ip.port", "UNKNOWN");
       clusterMembers[2].mId = 3;
       clusterMembers[2].mAddress = server3Addr;
       member2NetadminAddr[server3Addr] = server3NetAdminAddr;
       /// mock admin operations to add each cluster manually
       adminAddCluster(clusterMembers);
       auto cluster = std::make_unique<mock::MockAppCluster<objectstore::ObjectStore>>();
       cluster->setupAllServers({
           mConfFolderPrefix + mObjectStoreFolderPrefix + std::to_string(i + 1) + "/" + mServer1Name + ".ini",
           mConfFolderPrefix + mObjectStoreFolderPrefix + std::to_string(i + 1) + "/" + mServer2Name + ".ini",
           mConfFolderPrefix + mObjectStoreFolderPrefix + std::to_string(i + 1) + "/" + mServer3Name + ".ini"});
       mObjectStoreClusters.push_back(std::move(cluster));
       getCurAppLeader(i);
     }
  }

  void TearDown() override {
     SPDLOG_INFO("destroying all server");
     mObjectManagerClient = nullptr;
     mAppClients.clear();
     mNetAdminClients.clear();
     mObjectManagerCluster->killAllServers();
     for (auto i = 0; i < mObjectStoreClusters.size(); ++i) {
       mObjectStoreClusters[i]->killAllServers();
     }
     gringofts::Util::executeCmd("rm -rf ../test/output");
  }

  uint64_t getClusterNum() {
     return mObjectStoreClusters.size();
  }

  void initClients(const MemberInfo &member, uint32_t num) {
     SPDLOG_INFO("init {} clients to server {}", num, member.mAddress);
     updateRoute();
     for (uint32_t i = 0; i < num; ++i) {
       mAppClients[member.mAddress].push_back(std::make_unique<mock::MockAppClient>(
             grpc::CreateChannel(member.mAddress, grpc::InsecureChannelCredentials()),
             mRouteInfo.getRouteVersion()));
     }
  }

  void initNetAdminClients(const MemberInfo &member, uint32_t num) {
    SPDLOG_INFO("init {} netadmin clients to server {}", num, member.mAddress);
    updateRoute();
    for (uint32_t i = 0; i < num; ++i) {
      auto &addr = member2NetadminAddr[member.mAddress];
      mNetAdminClients[addr].push_back(
              std::make_unique<mock::MockNetAdminClient>(
              grpc::CreateChannel(addr, grpc::InsecureChannelCredentials())));
    }
  }

  void adminAddCluster(const std::vector<MemberInfo> &clusterMembers) {
    if (!mObjectManagerClient) {
      SPDLOG_INFO("initializing object manager client");
      mObjectManagerClient = std::make_unique<objectstore::network::ObjectManagerClient>(
           mConfFolderPrefix + mObjectStoreFolderPrefix + "1/" + mServer1Name + ".ini");
    }
    mObjectManagerCluster->getAppLeader();
    proto::AddCluster::Request req;
    for (auto m : clusterMembers) {
      auto s = req.mutable_cluster()->add_servers();
      s->set_hostname(std::to_string(m.mId) + "@" + m.mAddress);
      SPDLOG_INFO("manually adding cluster {}", s->hostname());
    }
    auto promise = std::make_shared<std::promise<proto::AddCluster::Response>>();
    auto future = promise->get_future();
    mObjectManagerClient->addCluster(req, [promise](const proto::AddCluster::Response &resp) {
        promise->set_value(resp);
    });
    auto resp = future.get();
    assert(resp.header().code() == proto::ResponseCode::OK);
  }
  std::vector<MemberInfo> getCurAppMembers(uint64_t objectStoreIndex) {
     assert(objectStoreIndex < mObjectStoreClusters.size());
     return mObjectStoreClusters[objectStoreIndex]->getAllAppMemberInfo();
  }
  MemberInfo getCurAppLeader(uint64_t objectStoreIndex, bool waitUntilInService = true) {
     assert(objectStoreIndex < mObjectStoreClusters.size());
     auto leader = mObjectStoreClusters[objectStoreIndex]->getAppLeader();
     if (waitUntilInService) {
       auto &inst = mObjectStoreClusters[objectStoreIndex]->getClusterInst(leader);
       while (true) {
         auto status = inst.getCurClusterStatus();
         if (status == proto::Cluster::IN_SERVICE) {
           break;
         }
         SPDLOG_INFO("waiting for cluster be in service");
         sleep(1);
       }
     }
     return leader;
  }
  std::vector<MemberInfo> getCurAppFollowers(uint64_t objectStoreIndex) {
     assert(objectStoreIndex < mObjectStoreClusters.size());
     auto leader = getCurAppLeader(objectStoreIndex);
     auto allMembers = mObjectStoreClusters[objectStoreIndex]->getAllAppMemberInfo();
     std::vector<MemberInfo> followers;
     for (auto &m : allMembers) {
       if (m.toString() != leader.toString()) {
         followers.push_back(m);
       }
     }
     return followers;
  }
  MemberInfo getCurRaftLeader(uint64_t objectStoreIndex) {
     assert(objectStoreIndex < mObjectStoreClusters.size());
     return mObjectStoreClusters[objectStoreIndex]->waitAndGetRaftLeader();
  }
  std::vector<MemberInfo> getCurRaftFollowers(uint64_t objectStoreIndex) {
     assert(objectStoreIndex < mObjectStoreClusters.size());
     auto leader = getCurRaftLeader(objectStoreIndex);
     auto allMembers = mObjectStoreClusters[objectStoreIndex]->getAllRaftMemberInfo();
     std::vector<MemberInfo> followers;
     for (auto &m : allMembers) {
       if (m.toString() != leader.toString()) {
         followers.push_back(m);
       }
     }
     return followers;
  }
  void restartAllServers(const std::vector<gringofts::SyncPoint> &newPoints, bool enableSyncPoint) {
     SPDLOG_INFO("restarting raft servers");
     mObjectManagerCluster->killAllServers(false);
     for (auto i = 0; i < mObjectStoreClusters.size(); ++i) {
       if (i == mObjectStoreClusters.size() - 1) {
         /// only tear down syncpoint at last server
         mObjectStoreClusters[i]->killAllServers(true);
       } else {
         mObjectStoreClusters[i]->killAllServers(false);
       }
     }
     mAppClients.clear();
     mNetAdminClients.clear();
     mObjectManagerCluster->setupAllServers({
         mConfFolderPrefix + mObjectManagerFolderPrefix + "/" + mServer1Name + ".ini",
         mConfFolderPrefix + mObjectManagerFolderPrefix + "/" + mServer2Name + ".ini",
         mConfFolderPrefix + mObjectManagerFolderPrefix + "/" + mServer3Name + ".ini"});
     for (auto i = 0; i < mObjectStoreClusters.size(); ++i) {
       if (i == mObjectStoreClusters.size() - 1) {
         mObjectStoreClusters[i]->setupAllServers({
             mConfFolderPrefix + mObjectStoreFolderPrefix + std::to_string(i + 1) + "/" + mServer1Name + ".ini",
             mConfFolderPrefix + mObjectStoreFolderPrefix + std::to_string(i + 1) + "/" + mServer2Name + ".ini",
             mConfFolderPrefix + mObjectStoreFolderPrefix + std::to_string(i + 1) + "/" + mServer3Name + ".ini"},
             newPoints);
       } else {
         mObjectStoreClusters[i]->setupAllServers({
             mConfFolderPrefix + mObjectStoreFolderPrefix + std::to_string(i + 1) + "/" + mServer1Name + ".ini",
             mConfFolderPrefix + mObjectStoreFolderPrefix + std::to_string(i + 1) + "/" + mServer2Name + ".ini",
             mConfFolderPrefix + mObjectStoreFolderPrefix + std::to_string(i + 1) + "/" + mServer3Name + ".ini"});
       }
     }
     if (enableSyncPoint) {
       mObjectManagerCluster->enableAllSyncPoints();
       for (auto i = 0; i < mObjectStoreClusters.size(); ++i) {
         mObjectStoreClusters[i]->enableAllSyncPoints();
       }
     }
  }
  uint64_t getLeaderLastLogIndex(uint64_t objectStoreIndex) {
     assert(objectStoreIndex < mObjectStoreClusters.size());
     auto raftLeader = mObjectStoreClusters[objectStoreIndex]->waitAndGetRaftLeader();
     return mObjectStoreClusters[objectStoreIndex]->getLastLogIndex(raftLeader);
  }
  void verifyRaftLastIndex(uint64_t objectStoreIndex, uint64_t expectedLastIndex, bool verifyCommitted = true) {
     assert(objectStoreIndex < mObjectStoreClusters.size());
     mObjectStoreClusters[objectStoreIndex]->waitLogForAll(expectedLastIndex);
     if (verifyCommitted) {
       mObjectStoreClusters[objectStoreIndex]->waitLogCommitForAll(expectedLastIndex);
     }
     auto allMembers = mObjectStoreClusters[objectStoreIndex]->getAllRaftMemberInfo();
     for (auto &m : allMembers) {
       ASSERT_EQ(mObjectStoreClusters[objectStoreIndex]->getLastLogIndex(m), expectedLastIndex);
     }
  }
  void beginSyncPointForOM(const std::vector<gringofts::SyncPoint> &points) {
     mObjectManagerCluster->enableAllSyncPoints();
     mObjectManagerCluster->resetSyncPoints(points);
  }
  void beginSyncPointForOS(const std::vector<gringofts::SyncPoint> &points) {
     for (auto i = 0; i < mObjectStoreClusters.size(); ++i) {
       mObjectStoreClusters[i]->enableAllSyncPoints();
       mObjectStoreClusters[i]->resetSyncPoints(points);
     }
  }
  void endSyncPointForOM() {
     mObjectManagerCluster->disableAllSyncPoints();
  }
  void endSyncPointForOS() {
     for (auto i = 0; i < mObjectStoreClusters.size(); ++i) {
       mObjectStoreClusters[i]->disableAllSyncPoints();
     }
  }

  void updateRoute() {
     SPDLOG_INFO("updating route in test client");
     auto promise = std::make_shared<std::promise<proto::Router::Response>>();
     auto future = promise->get_future();
     proto::Router::Request request;
     mObjectManagerClient->requestRouteInfo(request, [promise](const proto::Router::Response &resp) {
         promise->set_value(resp);
         });
     auto resp = future.get();
     mRouteInfo.updateRouteInfo(resp.routeinfo());
  }
  uint64_t getClusterIndexByKey(const kvengine::store::KeyType &key) {
     auto shardId = objectstore::ObjectStore::keyToShardId(key);
     uint32_t i = 0;
     for (auto &cluster : mObjectStoreClusters) {
       proto::Cluster::ClusterAddress clusterAddr;
       auto members = cluster->getAllAppMemberInfo();
       for (auto m : members) {
         auto s = clusterAddr.add_servers();
         s->set_hostname(std::to_string(m.mId) + "@" + m.mAddress);
       }
       auto clusterAddrFormat = objectstore::ObjectStore::clusterToAddressFormat(clusterAddr);
       if (mRouteInfo.checkIfInClusterScope(clusterAddrFormat, shardId)) {
         SPDLOG_INFO("found key {} in {} with shard {}", key, clusterAddrFormat, shardId);
         break;
       }
       i++;
     }
     return i;
  }

 protected:
  /// by default, we set cluster num to be 1
  uint64_t mClusterNum = 1;
  /// fixed_member config
  const std::string mConfFolderPrefix = "../test/config/";
  const std::string mObjectManagerFolderPrefix = "object-manager";
  const std::string mObjectStoreFolderPrefix = "object-store";
  const std::string mServer1Name = "node1";
  const std::string mServer2Name = "node2";
  const std::string mServer3Name = "node3";

  std::unique_ptr<mock::MockAppCluster<objectmanager::ObjectManager>> mObjectManagerCluster;
  std::vector<std::unique_ptr<mock::MockAppCluster<objectstore::ObjectStore>>> mObjectStoreClusters;
  std::unique_ptr<objectstore::network::ObjectManagerClient> mObjectManagerClient;
  objectstore::ObjectStore::GlobalInfo::RouteInfoHelper mRouteInfo;

  /// each server can have multiple clients
  std::map<ServerAddress, std::vector<std::unique_ptr<mock::MockAppClient>>> mAppClients;
  std::map<ServerAddress, std::vector<std::unique_ptr<mock::MockNetAdminClient>>> mNetAdminClients;
  std::map<ServerAddress, ServerAddress> member2NetadminAddr;
};

}  /// namespace goblin::mock

#endif  // SERVER_TEST_MOCK_MOCKE2ETEST_H_
