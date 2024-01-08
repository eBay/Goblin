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

#include <gtest/gtest-spi.h>
#include <infra/util/Util.h>

#include "CommandTest.h"

namespace goblin::kvengine::model {

using gringofts::raft::MemberInfo;

class LeadershipCommandTest : public CommandTest {
 protected:
  gringofts::SyncPointCallBack createCallBackToReceiveMessages(const std::set<MemberInfo> &whitelistServers) {
     return [=](void *arg1, void *arg2){
       auto info = static_cast<MemberInfo*>(arg1);
       auto event = static_cast<gringofts::raft::v2::RaftEventBase*>(arg2);
       if (whitelistServers.find(*info) == whitelistServers.end()) {
         SPDLOG_INFO("server {} is not allowed to receive messages", info->mId);
         event->mType = gringofts::raft::v2::RaftEventBase::Type::Unknown;
       }
     };
  }
};

TEST_F(LeadershipCommandTest, ConnectFollower) {
  for (uint32_t clusterIndex = 0; clusterIndex < getClusterNum(); ++clusterIndex) {
    uint32_t clientInstNum = 1;
    auto leader = getCurAppLeader(clusterIndex);
    initClients(leader, clientInstNum);
    auto followers = getCurAppFollowers(clusterIndex);
    for (auto &follower : followers) {
      initClients(follower, clientInstNum);
    }
    /// send to follower, all should fail
    for (uint32_t i = 0; i < clientInstNum; ++i) {
      for (auto &follower : followers) {
        testConnect(follower, i, [leader](const proto::Connect::Response &resp) {
            ASSERT_EQ(resp.header().code(), proto::ResponseCode::NOT_LEADER);
            ASSERT_EQ(resp.header().leaderhint(), std::to_string(leader.mId));
            });
      }
    }
    waitPreviousRequestsDone();
  }
}

TEST_F(LeadershipCommandTest, LeadershipChange) {
  const std::string keyPrefix = "LeadershipChangeTestKey";
  const std::string valuePrefix = "LeadershipChangeTestValue";
  uint32_t clientInstNum = 3;
  uint32_t dataNumPerClient = 5;
  auto clusterNum = getClusterNum();
  for (auto i = 0; i < clusterNum; ++i) {
    auto allAppMembers = getCurAppMembers(i);
    for (auto &m : allAppMembers) {
      initClients(m, clientInstNum);
    }
  }
  std::vector<std::map<store::KeyType, store::ValueType>> key2valueInLastRound(clusterNum);
  std::vector<std::map<store::KeyType, store::VersionType>> key2versionInLastRound(clusterNum);
  store::VersionType maxVersionInLastRound = 0;
  uint32_t leaderChangeRounds = 5;
  for (auto i = 0; i < leaderChangeRounds; ++i) {
    /// init current leader for all clusters
    std::vector<MemberInfo> leaders;
    std::vector<uint64_t> leadersBeginLogIndex;
    for (auto i = 0; i < clusterNum; ++i) {
      auto leader = getCurAppLeader(i);
      initClients(leader, clientInstNum);
      uint64_t beginLogIndex = getLeaderLastLogIndex(i);
      leaders.push_back(leader);
      leadersBeginLogIndex.push_back(beginLogIndex);
    }
    SPDLOG_INFO("running with leadership change at round {}", i);
    if (i > 0) {
      /// the version should be recovered
      verifyConnect(leaders, 0, clientInstNum, {maxVersionInLastRound});
      /// check data in last round
      verifyOneGet(leaders, clientInstNum, dataNumPerClient,
          keyPrefix + std::to_string(i - 1) + "_", key2valueInLastRound, key2versionInLastRound);
      /// these data shouldn't be found since it didn't committed in last round
      verifyOneGet(leaders, clientInstNum, dataNumPerClient,
          "LeaderChangedFailedKey1", key2valueInLastRound, key2versionInLastRound);
      verifyOneGet(leaders, clientInstNum, dataNumPerClient,
          "LeaderChangedFailedKey2", key2valueInLastRound, key2versionInLastRound);
    }
    std::vector<std::map<store::KeyType, store::ValueType>> key2value(clusterNum);
    std::vector<std::map<store::KeyType, store::VersionType>> key2version(clusterNum);
    /// put data in new round
    verifyOnePut(leaders, clientInstNum, dataNumPerClient,
        keyPrefix + std::to_string(i) + "_", valuePrefix + std::to_string(i) + "_", &key2value, &key2version);
    /// get data in new round
    verifyOneGet(leaders, clientInstNum, dataNumPerClient,
        keyPrefix + std::to_string(i) + "_", key2value, key2version);

    /// verify until all servers has replicated to the same index
    /// this also make sure all servers has the ability to become a leader
    for (auto i = 0; i < clusterNum; ++i) {
      verifyRaftLastIndex(i, leadersBeginLogIndex[i] + key2value[i].size(), false);
    }
    /// setup syncpoints to re-elect a leader and check data from new leader
    auto reElectClusterIndex = getClusterIndexByKey("LeaderChangedFailedKey1");
    auto leader = getCurAppLeader(reElectClusterIndex);
    auto followers = getCurRaftFollowers(reElectClusterIndex);
    gringofts::SyncPointCallBack banLeaderMsgCB = createCallBackToReceiveMessages(
        std::set<MemberInfo>(followers.begin(), followers.end()));
    std::vector<gringofts::SyncPoint> ps {
      {gringofts::TPRegistry::RaftCore_receiveMessage_interceptIncoming, banLeaderMsgCB,
        {}, gringofts::SyncPointType::Ignore}
    };
    beginSyncPointForOS(ps);
    /// put data in new round but failed to commit
    testPut(leader, 0, "LeaderChangedFailedKey1", "LeaderChangedFailedValue1", [](const proto::Put::Response &resp) {
        /// ignore since it should be timeout
        });
    testPut(leader, 1, "LeaderChangedFailedKey1", "LeaderChangedFailedValue2", [](const proto::Put::Response &resp) {
        /// ignore since it should be timeout
        });
    /// the version should be correct
    store::VersionType expectedMaxVersion = maxVersionInLastRound + clientInstNum * dataNumPerClient;
    verifyConnect({leader}, 0, clientInstNum, {expectedMaxVersion});

    while (true) {
      auto newLeader = getCurAppLeader(reElectClusterIndex);
      if (newLeader.toString() != leader.toString()) {
        break;
      }
      SPDLOG_INFO("waiting for a new leader to be elected");
      sleep(1);
    }
    endSyncPointForOS();
    key2valueInLastRound = key2value;
    key2versionInLastRound = key2version;
    maxVersionInLastRound = expectedMaxVersion;
  }
}
}  /// namespace goblin::kvengine::model
