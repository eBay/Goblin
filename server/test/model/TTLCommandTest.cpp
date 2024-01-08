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

#include <memory>

#include <gtest/gtest-spi.h>

#include "CommandTest.h"

namespace goblin::kvengine::model {

class TTLCommandTest : public CommandTest {
 protected:
  TTLCommandTest(): CommandTest(2) {}
  void verifyOnePutTTL(
       const std::vector<MemberInfo> &leaders,
       uint32_t clientInstNum,
       uint32_t dataNumPerClient,
       const store::KeyType &keyPrefix,
       const store::KeyType &valuePrefix,
       const store::TTLType &ttl,
       std::vector<std::map<store::KeyType, store::ValueType>> *key2valueHistory,
       std::vector<std::map<store::KeyType, store::VersionType>> *key2versionHistory) {
     assert(key2valueHistory->size() == leaders.size());
     assert(key2versionHistory->size() == leaders.size());
     for (uint32_t i = 0; i < clientInstNum; ++i) {
       for (uint32_t j = 0; j < dataNumPerClient; ++j) {
         auto key = keyPrefix + std::to_string(i) + "_" + std::to_string(j);
         auto value = valuePrefix + std::to_string(i) + "_" + std::to_string(j);
         auto clusterIndex = getClusterIndexByKey(key);
         assert(clusterIndex < leaders.size());
         (*key2valueHistory)[clusterIndex][key] = value;
         auto leader = leaders[clusterIndex];
         testPutTTL(leader, i, key, value, ttl,
             [key, key2versionHistory, clusterIndex](const proto::Put::Response &resp) {
             SPDLOG_INFO("debug: resp from key {} from cluster {}", key, clusterIndex);
             ASSERT_EQ(resp.result().code(), proto::ResponseCode::OK);
             ASSERT_TRUE(resp.result().version() > store::VersionStore::kInvalidVersion);
             (*key2versionHistory)[clusterIndex][key] = resp.result().version();
             });
       }
     }
     waitPreviousRequestsDone();
     auto totalForKeyAndValue = 0;
     auto totalForKeyAndVersion = 0;
     auto totalForVersion = 0;
     for (auto i = 0; i < leaders.size(); ++i) {
       std::set<store::VersionType> versions;
       for (auto &[key, v] : (*key2versionHistory)[i]) {
         versions.insert(v);
       }
       ASSERT_EQ((*key2valueHistory)[i].size(), (*key2versionHistory)[i].size());
       ASSERT_EQ((*key2valueHistory)[i].size(), versions.size());
       totalForKeyAndValue += (*key2valueHistory)[i].size();
       totalForKeyAndVersion += (*key2versionHistory)[i].size();
       totalForVersion += versions.size();
     }
     ASSERT_EQ(clientInstNum * dataNumPerClient, totalForKeyAndValue);
     ASSERT_EQ(clientInstNum * dataNumPerClient, totalForKeyAndVersion);
     ASSERT_EQ(clientInstNum * dataNumPerClient, totalForVersion);
  }
};

  TEST_F(TTLCommandTest, PutThenPutTTLWithTTLTest) {
    const std::string keyPrefix = "PutThenPutTTLWithTTLTestKey";
    const std::string valuePrefix = "PutThenPutTTLWithTTLTestValue";
    const store::TTLType ttl = 3;  /// 3 seconds
    uint32_t clientInstNum = 5;
    uint32_t dataNumPerClient = 10;
    auto clusterNum = getClusterNum();
    std::vector<MemberInfo> leaders;
    for (auto i = 0; i < clusterNum; ++i) {
      auto leader = getCurAppLeader(i);
      initClients(leader, clientInstNum);
      leaders.push_back(leader);
    }

    std::vector<std::map<store::KeyType, store::ValueType>> key2value(leaders.size());
    std::vector<std::map<store::KeyType, store::VersionType>> key2version(leaders.size());
    /// put data
    verifyOnePut(leaders, clientInstNum, dataNumPerClient, keyPrefix, valuePrefix, &key2value, &key2version);
    /// get data
    verifyOneGet(leaders, clientInstNum, dataNumPerClient, keyPrefix, key2value, key2version);
    verifyOnePutTTL(leaders, clientInstNum, dataNumPerClient, keyPrefix, valuePrefix, ttl, &key2value, &key2version);
    /// get data when data are not expired
    verifyOneGet(leaders, clientInstNum, dataNumPerClient, keyPrefix, key2value, key2version);
    sleep(ttl);
    /// get data when data are expired
    verifyOneGet(leaders, clientInstNum, dataNumPerClient, keyPrefix,
        std::vector<std::map<store::KeyType, store::ValueType>>(leaders.size()),
        std::vector<std::map<store::KeyType, store::VersionType>>(leaders.size()));
  }

  TEST_F(TTLCommandTest, PutTTLWithTTLThenPutTTLWithoutTTLTest) {
    const std::string keyPrefix = "PutTTLWithTTLThenPutTTLWithoutTTLTestKey";
    const std::string valuePrefix = "PutTTLWithTTLThenPutTTLWithoutTTLTestValue";
    store::TTLType ttl = 5;  /// 5 seconds
    uint32_t clientInstNum = 5;
    uint32_t dataNumPerClient = 10;
    auto clusterNum = getClusterNum();
    std::vector<MemberInfo> leaders;
    for (auto i = 0; i < clusterNum; ++i) {
      auto leader = getCurAppLeader(i);
      initClients(leader, clientInstNum);
      leaders.push_back(leader);
    }

    std::vector<std::map<store::KeyType, store::ValueType>> key2value(leaders.size());
    std::vector<std::map<store::KeyType, store::VersionType>> key2version(leaders.size());
    /// put ttl with ttl
    SPDLOG_INFO("put with ttl");
    verifyOnePutTTL(leaders, clientInstNum, dataNumPerClient, keyPrefix, valuePrefix, ttl, &key2value, &key2version);
    /// get data when data are not expired
    verifyOneGet(leaders, clientInstNum, dataNumPerClient, keyPrefix, key2value, key2version);
    /// put ttl again without ttl, which will reuse previous ttl
    SPDLOG_INFO("put without ttl");
    verifyOnePutTTL(leaders, clientInstNum, dataNumPerClient, keyPrefix,
        valuePrefix, store::INFINITE_TTL, &key2value, &key2version);
    /// the data should not expire
    verifyOneGet(leaders, clientInstNum, dataNumPerClient, keyPrefix, key2value, key2version);
    sleep(5);
    SPDLOG_INFO("check again after sleep");
    /// the data should expire
    verifyOneGet(leaders, clientInstNum, dataNumPerClient, keyPrefix,
        std::vector<std::map<store::KeyType, store::ValueType>>(leaders.size()),
        std::vector<std::map<store::KeyType, store::VersionType>>(leaders.size()));
  }

  TEST_F(TTLCommandTest, PutTTLWithTTLThenPutTTLWithAnotherTTLTest) {
    const std::string keyPrefix = "PutTTLWithTTLThenPutTTLWithAnotherTTLTestKey";
    const std::string valuePrefix = "PutTTLWithTTLThenPutTTLWithAnotherTTLTestValue";
    store::TTLType ttl = 3;  /// 3 seconds
    store::TTLType anotherTTL  = 5;  /// 5 seconds
    uint32_t clientInstNum = 5;
    uint32_t dataNumPerClient = 10;
    auto clusterNum = getClusterNum();
    std::vector<MemberInfo> leaders;
    for (auto i = 0; i < clusterNum; ++i) {
      auto leader = getCurAppLeader(i);
      initClients(leader, clientInstNum);
      leaders.push_back(leader);
    }

    std::vector<std::map<store::KeyType, store::ValueType>> key2value(leaders.size());
    std::vector<std::map<store::KeyType, store::VersionType>> key2version(leaders.size());
    /// put ttl with ttl
    verifyOnePutTTL(leaders, clientInstNum, dataNumPerClient, keyPrefix, valuePrefix, ttl, &key2value, &key2version);
    /// get data when data are not expired
    verifyOneGet(leaders, clientInstNum, dataNumPerClient, keyPrefix, key2value, key2version);
    sleep(1);
    /// put ttl again without ttl, which will reuse previous ttl
    verifyOnePutTTL(leaders, clientInstNum, dataNumPerClient, keyPrefix,
        valuePrefix, anotherTTL, &key2value, &key2version);
    sleep(2);
    /// the data should not expire
    verifyOneGet(leaders, clientInstNum, dataNumPerClient, keyPrefix, key2value, key2version);
    sleep(2);
    /// the data should expire
    verifyOneGet(leaders, clientInstNum, dataNumPerClient, keyPrefix,
        std::vector<std::map<store::KeyType, store::ValueType>>(leaders.size()),
        std::vector<std::map<store::KeyType, store::VersionType>>(leaders.size()));
  }

  TEST_F(TTLCommandTest, PutTTLWithTTLButExpireThenPutTTLWithoutTTLTest) {
    const std::string keyPrefix = "PutTTLWithTTLButExpireThenPutTTLWithoutTTLTestKey";
    const std::string valuePrefix = "PutTTLWithTTLButExpireThenPutTTLWithoutTTLTestValue";
    store::TTLType ttl = 3;  /// 3 seconds
    uint32_t clientInstNum = 5;
    uint32_t dataNumPerClient = 10;
    auto clusterNum = getClusterNum();
    std::vector<MemberInfo> leaders;
    for (auto i = 0; i < clusterNum; ++i) {
      auto leader = getCurAppLeader(i);
      initClients(leader, clientInstNum);
      leaders.push_back(leader);
    }

    std::vector<std::map<store::KeyType, store::ValueType>> key2value(leaders.size());
    std::vector<std::map<store::KeyType, store::VersionType>> key2version(leaders.size());
    /// put ttl with ttl
    verifyOnePutTTL(leaders, clientInstNum, dataNumPerClient, keyPrefix, valuePrefix, ttl, &key2value, &key2version);
    /// get data when data are not expired
    verifyOneGet(leaders, clientInstNum, dataNumPerClient, keyPrefix, key2value, key2version);
    sleep(ttl);
    /// put ttl again without ttl, but previous one is expired
    verifyOnePutTTL(leaders, clientInstNum, dataNumPerClient, keyPrefix,
        valuePrefix, store::INFINITE_TTL, &key2value, &key2version);
    sleep(5);
    /// the data should not expire
    verifyOneGet(leaders, clientInstNum, dataNumPerClient, keyPrefix, key2value, key2version);
  }
}  // namespace goblin::kvengine::model
