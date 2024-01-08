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
#include <stdlib.h>

#include "CommandTest.h"

namespace goblin::kvengine::model {

using gringofts::raft::MemberInfo;

class FollowerReadCommandTest : public CommandTest {
 protected:
  FollowerReadCommandTest(): CommandTest(1) {}
};

/// read from follower is allowed: allowStale=true
/// read from leader
/*
TEST_F(FollowerReadCommandTest, PutLeaderGetTest) {
  const std::string keyPrefix = "PutLeaderGetTestKey";
  const std::string valuePrefix = "PutLeaderGetTestValue";
  /// 10 to do put/get and the other 2 to do connect
  uint32_t clientInstNum = 12;
  uint32_t putLeaderGetClientInstNum = 10;
  uint32_t dataNumPerClient = 20;
  auto clusterNum = getClusterNum();
  std::vector<MemberInfo> leaders;
  std::vector<uint64_t> leadersBeginLogIndex;
  for (auto i = 0; i < clusterNum; ++i) {
    auto leader = getCurAppLeader(i);
    initClients(leader, clientInstNum);
    uint64_t beginLogIndex = getLeaderLastLogIndex(i);
    leaders.push_back(leader);
    leadersBeginLogIndex.push_back(beginLogIndex);
  }

  verifyConnect(leaders, putLeaderGetClientInstNum, clientInstNum,
      std::vector<uint64_t>(leaders.size(), store::VersionStore::kInvalidVersion));
  std::vector<std::map<store::KeyType, store::ValueType>> key2value(leaders.size());
  std::vector<std::map<store::KeyType, store::VersionType>> key2version(leaders.size());
  /// put data
  SPDLOG_INFO("verify one put");
  verifyOnePut(leaders, putLeaderGetClientInstNum, dataNumPerClient, keyPrefix,
      valuePrefix, &key2value, &key2version);
  auto keySum = 0;
  std::vector<uint64_t> maxVersionPerCluster;
  for (auto i = 0; i < leaders.size(); ++i) {
    keySum += key2value[i].size();
    verifyRaftLastIndex(i, leadersBeginLogIndex[i] + key2value[i].size());
    uint64_t maxVersion = 0;
    for (auto &[k, version] : key2version[i]) {
      maxVersion = std::max(maxVersion, version);
    }
    maxVersionPerCluster.push_back(maxVersion);
  }
  ASSERT_EQ(keySum, putLeaderGetClientInstNum * dataNumPerClient);
  /// verify all servers has replicated to the same index
  /// verify all clients should get the latest version
  SPDLOG_INFO("verify connect again");
  verifyConnect(leaders, putLeaderGetClientInstNum, clientInstNum, maxVersionPerCluster);
  /// get data
  SPDLOG_INFO("verify one get");
  std::vector<uint64_t> keyCntForGet(leaders.size(), 0);
  verifyOneGet(leaders, putLeaderGetClientInstNum, dataNumPerClient, keyPrefix, key2value, key2version);

  /// we didn't replicate anything, so index should be same
  for (auto i = 0; i < leaders.size(); ++i) {
    verifyRaftLastIndex(i, leadersBeginLogIndex[i] + key2value[i].size());
  }
  /// verify all clients should get the latest version
  verifyConnect(leaders, putLeaderGetClientInstNum, clientInstNum, maxVersionPerCluster);
}
*/

/// read from follower is allowed: allowStale=true
/// read from follower
/*
TEST_F(FollowerReadCommandTest, PutFollowerGetTest) {
  const std::string keyPrefix = "PutFollowerGetTestKey";
  const std::string valuePrefix = "PutFollowerGetTestValue";
  /// 10 to do put/get and the other 2 to do connect
  uint32_t clientInstNum = 12;
  uint32_t putFollowerGetClientInstNum = 10;
  uint32_t dataNumPerClient = 20;
  auto clusterNum = getClusterNum();
  std::vector<MemberInfo> leaders;
  std::vector<uint64_t> leadersBeginLogIndex;
  std::vector<MemberInfo> followers;

  for (auto i = 0; i < clusterNum; ++i) {
    auto leader = getCurAppLeader(i);
    initClients(leader, clientInstNum);
    uint64_t beginLogIndex = getLeaderLastLogIndex(i);
    leaders.push_back(leader);
    leadersBeginLogIndex.push_back(beginLogIndex);
  }

  verifyConnect(leaders, putFollowerGetClientInstNum, clientInstNum,
      std::vector<uint64_t>(leaders.size(), store::VersionStore::kInvalidVersion));
  std::vector<std::map<store::KeyType, store::ValueType>> key2value(leaders.size());
  std::vector<std::map<store::KeyType, store::VersionType>> key2version(leaders.size());
  /// put data
  SPDLOG_INFO("verify one put");
  verifyOnePut(leaders, putFollowerGetClientInstNum, dataNumPerClient,
      keyPrefix, valuePrefix, &key2value, &key2version);
  auto keySum = 0;
  std::vector<uint64_t> maxVersionPerCluster;
  for (auto i = 0; i < leaders.size(); ++i) {
    keySum += key2value[i].size();
    verifyRaftLastIndex(i, leadersBeginLogIndex[i] + key2value[i].size());
    uint64_t maxVersion = 0;
    for (auto &[k, version] : key2version[i]) {
      maxVersion = std::max(maxVersion, version);
    }
    maxVersionPerCluster.push_back(maxVersion);
  }
  ASSERT_EQ(keySum, putFollowerGetClientInstNum * dataNumPerClient);
  /// verify all servers has replicated to the same index
  /// verify all clients should get the latest version
  SPDLOG_INFO("verify connect again");
  verifyConnect(leaders, putFollowerGetClientInstNum, clientInstNum, maxVersionPerCluster);

  /// get data
  SPDLOG_INFO("verify one get");
  for (auto i = 0; i < clusterNum; ++i) {
    auto members = getCurAppFollowers(i);
    assert(members.size() > 1);
    auto follower = members[rand() % members.size()];
    initClients(follower, clientInstNum);
    followers.push_back(follower);
  }
  std::vector<uint64_t> keyCntForGet(followers.size(), 0);
  verifyOneFollowerGet(followers, putFollowerGetClientInstNum, dataNumPerClient, keyPrefix, key2value, key2version);

  /// we didn't replicate anything, so index should be same
  for (auto i = 0; i < leaders.size(); ++i) {
    verifyRaftLastIndex(i, leadersBeginLogIndex[i] + key2value[i].size());
  }
  /// verify all clients should get the latest version
  verifyConnect(leaders, putFollowerGetClientInstNum, clientInstNum, maxVersionPerCluster);
}
*/

}  /// namespace goblin::kvengine::model
