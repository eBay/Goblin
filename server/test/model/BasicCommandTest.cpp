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

class BasicCommandTest : public CommandTest {
 protected:
  BasicCommandTest(): CommandTest(2) {}
};

TEST_F(BasicCommandTest, PutGetTest) {
  const std::string keyPrefix = "PutGetTestKey";
  const std::string valuePrefix = "PutGetTestValue";
  /// 10 to do put/get and the other 2 to do connect
  uint32_t clientInstNum = 12;
  uint32_t putGetClientInstNum = 10;
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

  verifyConnect(leaders, putGetClientInstNum, clientInstNum,
      std::vector<uint64_t>(leaders.size(), store::VersionStore::kInvalidVersion));
  std::vector<std::map<store::KeyType, store::ValueType>> key2value(leaders.size());
  std::vector<std::map<store::KeyType, store::VersionType>> key2version(leaders.size());
  /// put data
  SPDLOG_INFO("verify one put");
  verifyOnePut(leaders, putGetClientInstNum, dataNumPerClient, keyPrefix, valuePrefix, &key2value, &key2version);
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
  ASSERT_EQ(keySum, putGetClientInstNum * dataNumPerClient);
  /// verify all servers has replicated to the same index
  /// verify all clients should get the latest version
  SPDLOG_INFO("verify connect again");
  verifyConnect(leaders, putGetClientInstNum, clientInstNum, maxVersionPerCluster);
  /// get data
  SPDLOG_INFO("verify one get");
  std::vector<uint64_t> keyCntForGet(leaders.size(), 0);
  verifyOneGet(leaders, putGetClientInstNum, dataNumPerClient, keyPrefix, key2value, key2version);

  /// we didn't replicate anything, so index should be same
  for (auto i = 0; i < leaders.size(); ++i) {
    verifyRaftLastIndex(i, leadersBeginLogIndex[i] + key2value[i].size());
  }
  /// verify all clients should get the latest version
  verifyConnect(leaders, putGetClientInstNum, clientInstNum, maxVersionPerCluster);
}

TEST_F(BasicCommandTest, PutOverwriteTest) {
  const std::string keyPrefix = "PutOverwriteTestKey";
  const std::string valuePrefix = "PutOverwriteTestValue";
  const std::string secondValuePrefix = "PutOverwriteTestSecondValue";
  /// 10 to do put/get and the other 2 to do connect
  uint32_t clientInstNum = 12;
  uint32_t putGetClientInstNum = 10;
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
  verifyConnect(leaders, putGetClientInstNum, clientInstNum,
      std::vector<uint64_t>(leaders.size(), store::VersionStore::kInvalidVersion));
  std::vector<std::map<store::KeyType, store::ValueType>> key2value(leaders.size());
  std::vector<std::map<store::KeyType, store::VersionType>> key2version(leaders.size());
  /// put data
  verifyOnePut(leaders, putGetClientInstNum, dataNumPerClient, keyPrefix, valuePrefix, &key2value, &key2version);
  /// get data
  verifyOneGet(leaders, putGetClientInstNum, dataNumPerClient, keyPrefix, key2value, key2version);
  std::vector<std::map<store::KeyType, store::ValueType>> key2secondValue(leaders.size());
  std::vector<std::map<store::KeyType, store::VersionType>> key2secondVersion(leaders.size());
  /// overwrite data
  verifyOnePut(leaders, putGetClientInstNum, dataNumPerClient, keyPrefix, secondValuePrefix,
      &key2secondValue, &key2secondVersion);
  /// get overwritten data
  verifyOneGet(leaders, putGetClientInstNum, dataNumPerClient, keyPrefix, key2secondValue, key2secondVersion);

  /// verify all servers has replicated to the same index
  std::vector<uint64_t> maxVersionPerCluster;
  auto keySum = 0;
  for (auto i = 0; i < leaders.size(); ++i) {
    keySum += key2value[i].size();
    keySum += key2secondValue[i].size();
    verifyRaftLastIndex(i, leadersBeginLogIndex[i] + key2value[i].size() + key2secondValue[i].size());
    uint64_t maxVersion = 0;
    for (auto &[k, version] : key2version[i]) {
      maxVersion = std::max(maxVersion, version);
    }
    for (auto &[k, version] : key2secondVersion[i]) {
      maxVersion = std::max(maxVersion, version);
    }
    maxVersionPerCluster.push_back(maxVersion);
  }
  ASSERT_EQ(keySum, 2 * putGetClientInstNum * dataNumPerClient);
  /// verify all clients should get the latest version
  verifyConnect(leaders, putGetClientInstNum, clientInstNum, maxVersionPerCluster);
}

TEST_F(BasicCommandTest, PutDeleteTest) {
  const std::string keyPrefix = "PutDeleteTestKey";
  const std::string valuePrefix = "PutDeleteTestValue";
  /// 10 to do put/get and the other 2 to do connect
  uint32_t clientInstNum = 12;
  uint32_t putGetClientInstNum = 10;
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
  verifyConnect(leaders, putGetClientInstNum, clientInstNum,
      std::vector<uint64_t>(leaders.size(), store::VersionStore::kInvalidVersion));
  std::vector<std::map<store::KeyType, store::ValueType>> key2value(leaders.size());
  std::vector<std::map<store::KeyType, store::VersionType>> key2version(leaders.size());
  /// put data
  verifyOnePut(leaders, putGetClientInstNum, dataNumPerClient, keyPrefix, valuePrefix, &key2value, &key2version);
  /// get data
  verifyOneGet(leaders, putGetClientInstNum, dataNumPerClient, keyPrefix, key2value, key2version);
  /// delete data
  verifyOneDelete(leaders, putGetClientInstNum, dataNumPerClient, keyPrefix, key2value, key2version);
  /// get deleted data and should be not existed
  verifyOneGet(leaders, putGetClientInstNum, dataNumPerClient, keyPrefix,
      std::vector<std::map<store::KeyType, store::ValueType>>(leaders.size()),
      std::vector<std::map<store::KeyType, store::VersionType>>(leaders.size()));

  /// verify all servers has replicated to the same index
  std::vector<uint64_t> maxVersionPerCluster;
  auto keySum = 0;
  for (auto i = 0; i < leaders.size(); ++i) {
    /// multiplied by 2 given put and delete
    keySum += key2value[i].size() * 2;
    verifyRaftLastIndex(i, leadersBeginLogIndex[i] + key2value[i].size() * 2);
    uint64_t maxVersion = 0;
    for (auto &[k, version] : key2version[i]) {
      /// multiplied by 2 given put and delete
      maxVersion = std::max(maxVersion, version * 2);
    }
    maxVersionPerCluster.push_back(maxVersion);
  }
  ASSERT_EQ(keySum, 2 * putGetClientInstNum * dataNumPerClient);
  /// verify all clients should get the latest version
  verifyConnect(leaders, putGetClientInstNum, clientInstNum, maxVersionPerCluster);
}
}  /// namespace goblin::kvengine::model
