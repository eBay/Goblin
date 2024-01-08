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

class UDFTransCommandTest : public CommandTest {
 protected:
    void buildTransRequest(
        const std::map<store::KeyType, goblin::proto::UserDefinedMeta> &kvAndUdfMetaToCheck,
        const std::map<store::KeyType, uint32_t> &kvAndPosToCheck,
        const std::map<store::KeyType, store::ValueType> &kvToPut,
        const std::vector<store::KeyType> &keyToGet,
        proto::Transaction::Request *request) {
      for (auto &[key, udfMeta] : kvAndUdfMetaToCheck) {
        auto precond = request->add_preconds();
        // Add UserMetaCondition
        precond->mutable_usermetacond()->set_key(key);
        precond->mutable_usermetacond()->set_pos(kvAndPosToCheck.at(key));
        precond->mutable_usermetacond()->set_op(goblin::proto::Precondition::GREATER);
        precond->mutable_usermetacond()->set_metatype(goblin::proto::Precondition_UserMetaCondition::NUM);
        for (auto &val : udfMeta.uintfield()) {
          precond->mutable_usermetacond()->mutable_udfmeta()->add_uintfield(val);
        }
        auto &value = kvToPut.at(key);
        auto entry = request->add_entries();
        entry->mutable_writeentry()->set_key(key);
        entry->mutable_writeentry()->set_value(value);
        for (auto &val : udfMeta.uintfield()) {
          entry->mutable_writeentry()->mutable_udfmeta()->add_uintfield(val);
        }
      }

      for (auto &key : keyToGet) {
        auto entry = request->add_entries();
        entry->mutable_readentry()->set_key(key);
      }
      request->set_ismetareturned(true);
    }
    void doOneTransaction(
        const MemberInfo &leader,
        uint32_t clientInstNum,
        uint32_t dataNumPerClient,
        const std::map<store::KeyType, goblin::proto::UserDefinedMeta> &kvAndUdfMetaToCheck,
        const std::map<store::KeyType, uint32_t> &kvAndPosToCheck,
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
          buildTransRequest(kvAndUdfMetaToCheck, kvAndPosToCheck, kvToPut, keyToGet, &request);
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
                    // ASSERT_TRUE(result.writeresult().version() > store::VersionStore::kInvalidVersion);
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

// TEST_F(UDFTransCommandTest, MultiPutGet) {
//   /// persist a kv before we do this test
//   const std::string prepareKey = "MultiPutGetPrepareKey";
//   const std::string prepareValue = "MultiPutGetPrepareValue";
//   const goblin::proto::UserDefinedMeta udfMeta;
//   udfMeta->add_uintfield(123);
//   uint32_t clientInstNum = 10;
//   uint32_t dataNumPerClient = 20;
//   auto clusterNum = getClusterNum();
//   assert(clusterNum == 1);
//   auto leader = getCurAppLeader(0);
//   initClients(leader, clientInstNum);
//   auto clusterIndex = getClusterIndexByKey(prepareKey);
//   assert(clusterIndex == 0);
//   uint64_t beginLogIndex = getLeaderLastLogIndex(clusterIndex);
//   testPutUdfMeta(leader, 0, prepareKey, prepareValue, udfMeta, [](const proto::Put::Response &resp) {
//       ASSERT_EQ(resp.result().code(), proto::ResponseCode::OK);
//       });
//   waitPreviousRequestsDone();

//   /// do transaction test
//   // persist kv first
//   // set udf int: 123
//   std::map<store::KeyType, store::ValueType> key2value;
//   std::map<store::KeyType, goblin::proto::UserDefinedMeta> &kvAndUdfMetaToCheck;
//   std::map<store::KeyType, std::vector<uint_32>::size_t> &kvAndPosToCheck;
//   goblin::proto::UserDefinedMeta udfMeta1;
//   udfMeta->add_uintfield(123);
//   goblin::proto::UserDefinedMeta udfMeta2;
//   udfMeta->add_uintfield(123);
//   std::map<store::KeyType, goblin::proto::UserDefinedMeta> keyAndUdfMetaToCheckIgnored = {
//     {prepareKey, udfMeta1}
//   };
//   std::map<store::KeyType, goblin::proto::UserDefinedMeta> keyAndUdfMetaToCheck = {
//     {prepareKey, udfMeta2}
//   };
//   // std::map<store::KeyType, goblin::proto::UserDefinedMeta> keyAndUdfMetaToCheckIgnored = {
//   //   {prepareKey, udfMeta1}
//   // };
//   std::map<store::KeyType, store::ValueType> kvPrefixToPut = {
//     {"MultiPutGetTestPutKeyOne", "MultiPutGetTestPutValueOne"},
//     {"MultiPutGetTestPutKeyTwo", "MultiPutGetTestPutValueTwo"}
//   };

//   /// prepare the expected value in key2value
//   key2value[prepareKey] = prepareValue;
//   /// prepare the kv: test precondition ignored
//   doOneTransaction(leader, clientInstNum, dataNumPerClient,
//       keyAndUdfMetaToCheckIgnored, std::vector<uint32_t>{0}, kvPrefixToPut, 1, &key2value, &key2version);
//   /// prepare the kv: test precondition succeed
//   doOneTransaction(leader, clientInstNum, dataNumPerClient,
//       keyAndUdfMetaToCheck, std::vector<uint32_t>{0}, kvPrefixToPut, 1, &key2value, &key2version);

//   /// testing udf precondition
//   const std::string prepareKey_2 = "MultiPutGetPrepareKey";
//   const std::string prepareValue_2 = "MultiPutGetPrepareValue2";
//   goblin::proto::UserDefinedMeta udfMeta1_2;
//   udfMeta->add_uintfield(122);
//   goblin::proto::UserDefinedMeta udfMeta2_2;
//   udfMeta->add_uintfield(124);
//   std::map<store::KeyType, goblin::proto::UserDefinedMeta> keyAndUdfMetaToCheckIgnored_2 = {
//     {prepareKey, udfMeta1_2}
//   };
//   std::map<store::KeyType, goblin::proto::UserDefinedMeta> keyAndUdfMetaToCheck_2 = {
//     {prepareKey, udfMeta2_2}
//   };

//   std::map<store::KeyType, store::ValueType> kvPrefixToPut_2 = {
//     {"MultiPutGetTestPutKeyOne", "MultiPutGetTestPutValueOne_Two"},
//     {"MultiPutGetTestPutKeyTwo", "MultiPutGetTestPutValueTwo_Two"}
//   };

//   /// test precondition ignored
//   doOneTransaction(leader, clientInstNum, dataNumPerClient,
//       keyAndUdfMetaToCheckIgnored_2, std::vector<uint32_t>{0}, kvPrefixToPut_2, 1, &key2value, &key2version);
//   /// test precondition succeed
//   doOneTransaction(leader, clientInstNum, dataNumPerClient,
//       keyAndUdfMetaToCheck_2, std::vector<uint32_t>{0}, kvPrefixToPut, 1, &key2value, &key2version);

//   /// check the persisted kv
//   for (auto &[keyPrefix, valuePrefix] : kvPrefixToPut) {
//     auto key = keyPrefix + std::to_string(0) + "_" + std::to_string(0);
//     auto expectedValue = valuePrefix + std::to_string(0) + "_" + std::to_string(0);
//     /// client 1 to read the data written by client 0
//     testGet(leader, 1, key, [expectedValue](const proto::Get::Response &resp) {
//         ASSERT_EQ(resp.result().code(), proto::ResponseCode::OK);
//         ASSERT_EQ(resp.result().value(), expectedValue);
//         /// the version is allocated concurrently
//         ASSERT_GT(resp.result().version(), 1);
//         });
//   }
//   /// we didn't replicate anything, so index should be same
//   /// we added an extra 1 here because we prepare a kv before trans test
//   verifyRaftLastIndex(clusterIndex, beginLogIndex + 1 + clientInstNum * dataNumPerClient);
//   /// verify all clients should get the latest version
//   /// we added an extra 1 here because we prepare a kv before trans test
//   verifyConnect({leader}, 0, clientInstNum, {1 + clientInstNum * dataNumPerClient});
// }

TEST_F(UDFTransCommandTest, OnlyOneCASSucceed) {
  /// persist a kv before we do this test
  /// persist kv first
  /// provided udf int: 123
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
  goblin::proto::UserDefinedMeta udfMeta;
  udfMeta.add_uintfield(123);
  testPutUdfMeta(leader, 0, prepareKey, prepareValue, udfMeta, [](const proto::Put::Response &resp) {
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
        SPDLOG_INFO("Debug UDFTransCommandTest: key {}, prepareValue {}, expectedValue {}",
            prepareKey, prepareValue, resp.result().value());
        auto &udfMeta = resp.result().udfmeta();
        for (auto &val : udfMeta.uintfield()) {
          SPDLOG_INFO("Debug UDFTransCommandTest: udfUint {}", val);
        }
    });
  }
  waitPreviousRequestsDone();

  /// do CAS
  std::map<store::KeyType, store::ValueType> key2value;
  goblin::proto::UserDefinedMeta udfMeta1;
  udfMeta1.add_uintfield(122);
  goblin::proto::UserDefinedMeta udfMeta2;
  udfMeta2.add_uintfield(124);
  const std::string newValuePrefix = "OnlyOneCASSucceedNewValuePrefix";
  uint32_t succeedCnt_1 = 0;
  uint32_t ignoredCnt_1 = 0;
  uint32_t failedCnt_1 = 0;

  uint32_t succeedCnt_2 = 0;
  uint32_t ignoredCnt_2 = 0;
  uint32_t failedCnt_2 = 0;

  /// provided uint = 122 (<123)
  /// put ops should be ignored
  for (uint32_t i = 0; i < clientInstNum; ++i) {
    auto newValue_ignore = newValuePrefix + std::to_string(i) + "_ignore";
    proto::Transaction::Request request1;
    buildTransRequest({{prepareKey, udfMeta1}}, {{prepareKey, 0}}, {{prepareKey, newValue_ignore}}, {}, &request1);
    testTrans(leader, i, request1, [i, newValue_ignore, &succeedCnt_1, &ignoredCnt_1, &failedCnt_1, &prepareKey]
        (const proto::Transaction::Response &resp) {
        if (resp.header().code() == proto::ResponseCode::OK) {
          ASSERT_EQ(resp.results().size(), 1);
          ASSERT_EQ(resp.ignoredputkeys().size(), 1);
          auto &ignored_key = resp.ignoredputkeys()[0];
          ASSERT_EQ(ignored_key, prepareKey);
          ignoredCnt_1 += 1;
          succeedCnt_1 += 1;
        } else if (resp.header().code() == proto::ResponseCode::PRECOND_NOT_MATCHED) {
          ASSERT_EQ(resp.results().size(), 0);
          failedCnt_1 += 1;
        } else {
          assert(0);
        }
    // testGet(leader, i, prepareKey, [i, prepareKey, prepareValue](const proto::Get::Response &resp) {
    //     ASSERT_EQ(resp.result().code(), proto::ResponseCode::OK);
    //     ASSERT_EQ(resp.result().value(), prepareValue);
    //     ASSERT_EQ(resp.result().version(), 1);
    //     SPDLOG_INFO("Debug UDFTransCommandTest: key {}, prepareValue {}, expectedValue {}",
    //         prepareKey, prepareValue, resp.result().value());
    //     auto &udfMeta = resp.result().udfmeta();
    //     for (auto &val: udfMeta.uintfield()) {
    //       SPDLOG_INFO("Debug UDFTransCommandTest: udfUint {}", val);
    //     }
    //     });
    });
  }
  waitPreviousRequestsDone();
  ASSERT_EQ(succeedCnt_1, clientInstNum);
  ASSERT_EQ(ignoredCnt_1, clientInstNum);

  /// provided uint = 124 (>123)
  /// put ops should succeed
  auto newValueSucceed = newValuePrefix + std::to_string(0) + "_succeed";
  for (uint32_t i = 0; i < clientInstNum; ++i) {
    auto newValue_succeed = newValuePrefix + std::to_string(i) + "_succeed";
    proto::Transaction::Request request2;
    buildTransRequest({{prepareKey, udfMeta2}}, {{prepareKey, 0}}, {{prepareKey, newValue_succeed}}, {}, &request2);
    testTrans(leader, i, request2, [i, newValue_succeed, &succeedCnt_2, &ignoredCnt_2, &failedCnt_2, &prepareKey]
        (const proto::Transaction::Response &resp) {
        auto targetValue = newValue_succeed;
        if (resp.header().code() == proto::ResponseCode::OK) {
          if (i == 0) {
            ASSERT_EQ(resp.results().size(), 1);
            auto &result = resp.results()[0];
            ASSERT_TRUE(result.has_writeresult());
            ASSERT_EQ(result.writeresult().version(), 2);
          } else {
            ASSERT_EQ(resp.results().size(), 1);
            ASSERT_EQ(resp.ignoredputkeys().size(), 1);
            auto &ignored_key = resp.ignoredputkeys()[0];
            ASSERT_EQ(ignored_key, prepareKey);
            ignoredCnt_2 += 1;
          }
          succeedCnt_2 += 1;
        } else if (resp.header().code() == proto::ResponseCode::PRECOND_NOT_MATCHED) {
          ASSERT_EQ(resp.results().size(), 0);
          failedCnt_2 += 1;
        } else {
          assert(0);
        }
    // testGet(leader, i, prepareKey, [i, prepareKey](const proto::Get::Response &resp) {
    //     ASSERT_EQ(resp.result().code(), proto::ResponseCode::OK);
    //     ASSERT_EQ(resp.result().value(), prepareValue);
    //     ASSERT_EQ(resp.result().version(), 1);
    //     SPDLOG_INFO("Debug UDFTransCommandTest: key {}, prepareValue {}, expectedValue {}",
    //         prepareKey, prepareValue, resp.result().value());
    //     auto &udfMeta = resp.result().udfmeta();
    //     for (auto &val: udfMeta.uintfield()) {
    //       SPDLOG_INFO("Debug UDFTransCommandTest: udfUint {}", val);
    //     }
    //     });
    });
  }
  waitPreviousRequestsDone();
  ASSERT_EQ(succeedCnt_2, 10);
  ASSERT_EQ(ignoredCnt_2, clientInstNum-1);

  /// we didn't replicate anything, so index should be same
  /// we added an extra 1 here because we prepare a kv before trans test
  verifyRaftLastIndex(clusterIndex, beginLogIndex + 1 + 1);
  /// verify all clients should get the latest version
  /// we added an extra 1 here because we prepare a kv before trans test
  verifyConnect({leader}, 0, clientInstNum, {1 + 1});
}
}  /// namespace goblin::kvengine::model
