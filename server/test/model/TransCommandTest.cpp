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

class TransCommandTest : public CommandTest {
 protected:
    void buildTransRequest(
        const std::map<store::KeyType, store::VersionType> &keyAndVersionToCheck,
        const std::map<store::KeyType, store::ValueType> &kvToPut,
        const std::vector<store::KeyType> &keyToGet,
        proto::Transaction::Request *request) {
      for (auto &[key, version] : keyAndVersionToCheck) {
        auto precond = request->add_preconds();
        precond->mutable_versioncond()->set_key(key);
        precond->mutable_versioncond()->set_version(version);
        precond->mutable_versioncond()->set_op(proto::Precondition::EQUAL);
      }
      for (auto &[key, value] : kvToPut) {
        auto entry = request->add_entries();
        entry->mutable_writeentry()->set_key(key);
        entry->mutable_writeentry()->set_value(value);
      }
      for (auto &key : keyToGet) {
        auto entry = request->add_entries();
        entry->mutable_readentry()->set_key(key);
      }
    }
    void doOneTransaction(
        const MemberInfo &leader,
        uint32_t clientInstNum,
        uint32_t dataNumPerClient,
        const std::map<store::KeyType, store::VersionType> &keyAndVersionToCheck,
        const std::map<store::KeyType, store::ValueType> &kvPrefixToPut,
        bool expectPrecondMatched,
        std::map<store::KeyType, store::ValueType> *key2valueHistory,
        std::map<store::KeyType, store::VersionType> *key2versionHistory) {
      for (uint32_t i = 0; i < clientInstNum; ++i) {
        for (uint32_t j = 0; j < dataNumPerClient; ++j) {
          std::map<store::KeyType, store::ValueType> kvToPut;
          std::vector<store::KeyType> keyToGet;
          for (auto &[keyPrefix, valuePrefix] : kvPrefixToPut) {
            auto key = keyPrefix + std::to_string(i) + "_" + std::to_string(j);
            auto value = valuePrefix + std::to_string(i) + "_" + std::to_string(j);
            kvToPut[key] = value;
            (*key2valueHistory)[key] = value;
            /// we also try to read the new kv in the same transaction
            keyToGet.push_back(key);
          }
          proto::Transaction::Request request;
          buildTransRequest(keyAndVersionToCheck, kvToPut, keyToGet, &request);
          uint32_t putNum = kvToPut.size();
          uint32_t getNum = keyToGet.size();

          testTrans(leader, i, request,
              [expectPrecondMatched, putNum, getNum, kvToPut,
              keyToGet, key2valueHistory, key2versionHistory](const proto::Transaction::Response &resp) {
              if (expectPrecondMatched) {
                ASSERT_EQ(resp.header().code(), proto::ResponseCode::OK);
                uint32_t i = 0;
                for (auto &result : resp.results()) {
                  if (result.has_writeresult()) {
                    ASSERT_TRUE(i < putNum);
                    ASSERT_EQ(result.writeresult().code(), proto::ResponseCode::OK);
                    ASSERT_TRUE(result.writeresult().version() > store::VersionStore::kInvalidVersion);
                    auto targetKey = keyToGet[i];
                    (*key2versionHistory)[targetKey] = result.writeresult().version();
                  } else if (result.has_readresult()) {
                    ASSERT_TRUE(i < getNum + putNum && i >= putNum);
                    auto targetKey = keyToGet[i - putNum];
                    auto expectedValue = (*key2valueHistory)[targetKey];
                    auto expectedVersion = (*key2versionHistory)[targetKey];
                    ASSERT_EQ(result.readresult().code(), proto::ResponseCode::OK);
                    ASSERT_EQ(result.readresult().value(), expectedValue);
                    ASSERT_EQ(result.readresult().version(), expectedVersion);
                  }
                  i++;
                }
              } else {
                ASSERT_EQ(resp.header().code(), proto::ResponseCode::PRECOND_NOT_MATCHED);
              }
              });
        }
      }
      waitPreviousRequestsDone();
      if (expectPrecondMatched) {
        ASSERT_EQ(clientInstNum * dataNumPerClient * kvPrefixToPut.size(), key2versionHistory->size());
        std::set<store::VersionType> versions;
        for (auto &[key, v] : *key2versionHistory) {
          versions.insert(v);
        }
        ASSERT_EQ(clientInstNum * dataNumPerClient, versions.size());
      } else {
        ASSERT_EQ(0, key2versionHistory->size());
      }
    }
};

TEST_F(TransCommandTest, MultiPutGet) {
  /// persist a kv before we do this test
  const std::string prepareKey = "MultiPutGetPrepareKey";
  const std::string prepareValue = "MultiPutGetPrepareValue";
  uint32_t clientInstNum = 10;
  uint32_t dataNumPerClient = 20;
  auto clusterNum = getClusterNum();
  assert(clusterNum == 1);
  auto leader = getCurAppLeader(0);
  initClients(leader, clientInstNum);
  auto clusterIndex = getClusterIndexByKey(prepareKey);
  assert(clusterIndex == 0);
  uint64_t beginLogIndex = getLeaderLastLogIndex(clusterIndex);
  testPut(leader, 0, prepareKey, prepareValue, [](const proto::Put::Response &resp) {
      ASSERT_EQ(resp.result().code(), proto::ResponseCode::OK);
      ASSERT_EQ(resp.result().version(), 1);
      });
  waitPreviousRequestsDone();

  /// do transaction test
  std::map<store::KeyType, store::ValueType> key2value;
  std::map<store::KeyType, store::VersionType> key2version;
  std::map<store::KeyType, store::VersionType> keyAndVersionToCheckFailed = {
    {prepareKey, 2}
  };
  std::map<store::KeyType, store::VersionType> keyAndVersionToCheck = {
    {prepareKey, 1}
  };
  std::map<store::KeyType, store::ValueType> kvPrefixToPut = {
    {"MultiPutGetTestPutKeyOne", "MultiPutGetTestPutValueOne"},
    {"MultiPutGetTestPutKeyTwo", "MultiPutGetTestPutValueTwo"}
  };
  /// prepare the expected value in key2value
  key2value[prepareKey] = prepareValue;
  /// test precondition failed
  doOneTransaction(leader, clientInstNum, dataNumPerClient,
      keyAndVersionToCheckFailed, kvPrefixToPut, false, &key2value, &key2version);
  /// test precondition succeed
  doOneTransaction(leader, clientInstNum, dataNumPerClient,
      keyAndVersionToCheck, kvPrefixToPut, true, &key2value, &key2version);

  /// check the persisted kv
  for (auto &[keyPrefix, valuePrefix] : kvPrefixToPut) {
    auto key = keyPrefix + std::to_string(0) + "_" + std::to_string(0);
    auto expectedValue = valuePrefix + std::to_string(0) + "_" + std::to_string(0);
    /// client 1 to read the data written by client 0
    testGet(leader, 1, key, [expectedValue](const proto::Get::Response &resp) {
        ASSERT_EQ(resp.result().code(), proto::ResponseCode::OK);
        ASSERT_EQ(resp.result().value(), expectedValue);
        /// the version is allocated concurrently
        ASSERT_GT(resp.result().version(), 1);
        });
  }
  /// we didn't replicate anything, so index should be same
  /// we added an extra 1 here because we prepare a kv before trans test
  verifyRaftLastIndex(clusterIndex, beginLogIndex + 1 + clientInstNum * dataNumPerClient);
  /// verify all clients should get the latest version
  /// we added an extra 1 here because we prepare a kv before trans test
  verifyConnect({leader}, 0, clientInstNum, {1 + clientInstNum * dataNumPerClient});
}

TEST_F(TransCommandTest, OnlyOneCASSucceed) {
  /// persist a kv before we do this test
  const std::string prepareKey = "OnlyOneCASSucceedPrepareKey";
  const std::string prepareValue = "OnlyOneCASSucceedPrepareValue";
  uint32_t clientInstNum = 10;
  auto clusterNum = getClusterNum();
  assert(clusterNum == 1);
  auto leader = getCurAppLeader(0);
  initClients(leader, clientInstNum);
  auto clusterIndex = getClusterIndexByKey(prepareKey);
  assert(clusterIndex == 0);
  uint64_t beginLogIndex = getLeaderLastLogIndex(clusterIndex);
  testPut(leader, 0, prepareKey, prepareValue, [](const proto::Put::Response &resp) {
      ASSERT_EQ(resp.result().code(), proto::ResponseCode::OK);
      ASSERT_EQ(resp.result().version(), 1);
      });
  waitPreviousRequestsDone();

  /// all clients get their data
  for (uint32_t i = 0; i < clientInstNum; ++i) {
    testGet(leader, i, prepareKey, [i, prepareKey, prepareValue](const proto::Get::Response &resp) {
        ASSERT_EQ(resp.result().code(), proto::ResponseCode::OK);
        ASSERT_EQ(resp.result().value(), prepareValue);
        ASSERT_EQ(resp.result().version(), 1);
        });
  }
  waitPreviousRequestsDone();

  /// do CAS
  std::map<store::KeyType, store::VersionType> keyAndVersionToCheck = {
    {prepareKey, 1}
  };
  const std::string newValuePrefix = "OnlyOneCASSucceedNewValuePrefix";
  uint32_t succeedCnt = 0;
  uint32_t failedCnt = 0;
  for (uint32_t i = 0; i < clientInstNum; ++i) {
    auto newValue = newValuePrefix + std::to_string(i);
    proto::Transaction::Request request;
    buildTransRequest(keyAndVersionToCheck, {{prepareKey, newValue}}, {}, &request);
    testTrans(leader, i, request, [i, newValue, &succeedCnt, &failedCnt](const proto::Transaction::Response &resp) {
        auto targetValue = newValue;
        if (resp.header().code() == proto::ResponseCode::OK) {
          ASSERT_EQ(resp.results().size(), 1);
          auto &result = resp.results()[0];
          ASSERT_TRUE(result.has_writeresult());
          ASSERT_EQ(result.writeresult().version(), 2);
          succeedCnt += 1;
        } else if (resp.header().code() == proto::ResponseCode::PRECOND_NOT_MATCHED) {
          ASSERT_EQ(resp.results().size(), 0);
          failedCnt += 1;
        } else {
          assert(0);
        }
    });
  }
  waitPreviousRequestsDone();
  ASSERT_EQ(succeedCnt, 1);
  ASSERT_EQ(failedCnt, clientInstNum - 1);

  /// we didn't replicate anything, so index should be same
  /// we added an extra 1 here because we prepare a kv before trans test
  verifyRaftLastIndex(clusterIndex, beginLogIndex + 1 + 1);
  /// verify all clients should get the latest version
  /// we added an extra 1 here because we prepare a kv before trans test
  verifyConnect({leader}, 0, clientInstNum, {1 + 1});
}
}  /// namespace goblin::kvengine::model
