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

#ifndef SERVER_TEST_MODEL_COMMANDTEST_H_
#define SERVER_TEST_MODEL_COMMANDTEST_H_

#include <gtest/gtest-spi.h>
#include <infra/util/Util.h>

#include "../../src/kv-engine/utils/TPRegistryEx.h"
#include "../mock/MockAppClient.h"
#include "../mock/MockAppCluster.h"
#include "../mock/MockE2ETest.h"

namespace goblin::kvengine::model {

using gringofts::raft::MemberInfo;

class CommandTest : public mock::MockE2ETest {
 protected:
  explicit CommandTest(uint64_t clusterNum = 1): mock::MockE2ETest(clusterNum) {}
  void waitPreviousRequestsDone() {
     while (1) {
       if (mReqNum != mRespNum) {
         SPDLOG_INFO("waiting for requests to be handled, requests sent: {}, responses received: {}",
             mReqNum, mRespNum);
         sleep(1);
       } else {
         break;
       }
     }
  }
  void testConnect(const MemberInfo &server, uint32_t clientInstIndex, mock::ConnectResponseCallBack cb) {
     auto it = mAppClients.find(server.mAddress);
     assert(it != mAppClients.end());
     assert(it->second.size() > clientInstIndex);
     mReqNum += 1;
     mAppClients[server.mAddress][clientInstIndex]->connectAsync([this, cb](const proto::Connect::Response &resp) {
         std::lock_guard<std::mutex> lock(mMutex);
         cb(resp);
         /// response num should be added after cb
         /// so that everything happened in cb could be seen
         /// after response num is incremented
         mRespNum +=1;
         });
  }
  void testPut(
      const MemberInfo &server,
      uint32_t clientInstIndex,
      const store::KeyType &key,
      const store::ValueType &value,
      mock::PutResponseCallBack cb) {
     auto it = mAppClients.find(server.mAddress);
     assert(it != mAppClients.end());
     assert(it->second.size() > clientInstIndex);
     mReqNum += 1;
     mAppClients[server.mAddress][clientInstIndex]->putAsync(key, value, [this, cb](const proto::Put::Response &resp) {
         std::lock_guard<std::mutex> lock(mMutex);
         cb(resp);
         /// response num should be added after cb
         /// so that everything happened in cb could be seen
         /// after response num is incremented
         mRespNum +=1;
         });
  }
  void testPutTTL(
      const MemberInfo &server,
      uint32_t clientInstIndex,
      const store::KeyType &key,
      const store::ValueType &value,
      const store::TTLType &ttl,
      mock::PutResponseCallBack cb) {
     auto it = mAppClients.find(server.mAddress);
     assert(it != mAppClients.end());
     assert(it->second.size() > clientInstIndex);
     mReqNum += 1;
     mAppClients[server.mAddress][clientInstIndex]->putTTLAsync(
         key, value, ttl, [this, cb](const proto::Put::Response &resp) {
           std::lock_guard<std::mutex> lock(mMutex);
           cb(resp);
           /// response num should be added after cb
           /// so that everything happened in cb could be seen
           /// after response num is incremented
           mRespNum +=1;
         });
  }
  void testPutUdfMeta(
      const MemberInfo &server,
      uint32_t clientInstIndex,
      const store::KeyType &key,
      const store::ValueType &value,
      const goblin::proto::UserDefinedMeta &udfMeta,
      mock::PutResponseCallBack cb) {
     auto it = mAppClients.find(server.mAddress);
     assert(it != mAppClients.end());
     assert(it->second.size() > clientInstIndex);
     mReqNum += 1;
     mAppClients[server.mAddress][clientInstIndex]->putUdfMetaAsync(key, value, udfMeta,
         [this, cb](const proto::Put::Response &resp) {
         std::lock_guard<std::mutex> lock(mMutex);
         cb(resp);
         /// response num should be added after cb
         /// so that everything happened in cb could be seen
         /// after response num is incremented
         mRespNum +=1;
         });
  }
  void testGet(
      const MemberInfo &server,
      uint32_t clientInstIndex,
      const store::KeyType &key,
      mock::GetResponseCallBack cb) {
     auto it = mAppClients.find(server.mAddress);
     assert(it != mAppClients.end());
     assert(it->second.size() > clientInstIndex);
     mReqNum += 1;
     mAppClients[server.mAddress][clientInstIndex]->getAsync(key, [this, cb](const proto::Get::Response &resp) {
         std::lock_guard<std::mutex> lock(mMutex);
         cb(resp);
         /// response num should be added after cb
         /// so that everything happened in cb could be seen
         /// after response num is incremented
         mRespNum +=1;
         });
  }

  void testGet(
      const MemberInfo &server,
      uint32_t clientInstIndex,
      const store::KeyType &key,
      bool isMetaReturned,
      mock::GetResponseCallBack cb) {
     auto it = mAppClients.find(server.mAddress);
     assert(it != mAppClients.end());
     assert(it->second.size() > clientInstIndex);
     mReqNum += 1;
     mAppClients[server.mAddress][clientInstIndex]->getAsync(key, isMetaReturned,
         [this, cb](const proto::Get::Response &resp) {
         std::lock_guard<std::mutex> lock(mMutex);
         cb(resp);
         /// response num should be added after cb
         /// so that everything happened in cb could be seen
         /// after response num is incremented
         mRespNum +=1;
         });
  }

  void testFollowerGet(
      const MemberInfo &server,
      uint32_t clientInstIndex,
      const store::KeyType &key,
      mock::GetResponseCallBack cb) {
     auto it = mAppClients.find(server.mAddress);
     assert(it != mAppClients.end());
     assert(it->second.size() > clientInstIndex);
     mReqNum += 1;
     mAppClients[server.mAddress][clientInstIndex]->getFollowerAsync(key, [this, cb](const proto::Get::Response &resp) {
         std::lock_guard<std::mutex> lock(mMutex);
         cb(resp);
         /// response num should be added after cb
         /// so that everything happened in cb could be seen
         /// after response num is incremented
         mRespNum +=1;
         });
  }

  void testDelete(
      const MemberInfo &server,
      uint32_t clientInstIndex,
      const store::KeyType &key,
      mock::DeleteResponseCallBack cb) {
     auto it = mAppClients.find(server.mAddress);
     assert(it != mAppClients.end());
     assert(it->second.size() > clientInstIndex);
     mReqNum += 1;
     mAppClients[server.mAddress][clientInstIndex]->deleteAsync(
         key, [this, cb](const proto::Delete::Response &resp) {
           std::lock_guard<std::mutex> lock(mMutex);
           cb(resp);
           /// response num should be added after cb
           /// so that everything happened in cb could be seen
           /// after response num is incremented
           mRespNum +=1;
         });
  }
  void testTrans(
      const MemberInfo &server,
      uint32_t clientInstIndex,
      const proto::Transaction::Request &req,
      mock::TransResponseCallBack cb) {
     auto it = mAppClients.find(server.mAddress);
     assert(it != mAppClients.end());
     assert(it->second.size() > clientInstIndex);
     mReqNum += 1;
     mAppClients[server.mAddress][clientInstIndex]->transAsync(
         req, [this, cb](const proto::Transaction::Response &resp) {
           std::lock_guard<std::mutex> lock(mMutex);
           cb(resp);
           /// response num should be added after cb
           /// so that everything happened in cb could be seen
           /// after response num is incremented
           mRespNum +=1;
         });
  }

  /****** below functions will run operations and verify result ******/
  void verifyConnect(
       const std::vector<MemberInfo> &leaders,
       uint32_t startClientIndex,
       uint32_t endClientIndex,
       const std::vector<store::VersionType> &expectedVersions) {
     for (uint32_t i = startClientIndex; i < endClientIndex; ++i) {
       for (auto clusterIndex = 0; clusterIndex < leaders.size(); ++clusterIndex) {
         auto &leader = leaders[clusterIndex];
         auto &expectedVersion = expectedVersions[clusterIndex];
         testConnect(leader, i, [expectedVersion](const proto::Connect::Response &resp) {
             ASSERT_EQ(resp.header().code(), proto::ResponseCode::OK);
             ASSERT_EQ(resp.header().latestversion(), expectedVersion);
             });
       }
     }
     waitPreviousRequestsDone();
  }
  void verifyOnePut(
       const std::vector<MemberInfo> &leaders,
       uint32_t clientInstNum,
       uint32_t dataNumPerClient,
       const store::KeyType &keyPrefix,
       const store::KeyType &valuePrefix,
       /// each element correspond to a cluster index
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
         SPDLOG_INFO("debug: key {}, sending to {}", key, leader.mAddress);
         testPut(leader, i, key, value, [key, key2versionHistory, clusterIndex](const proto::Put::Response &resp) {
             SPDLOG_INFO("debug: resp from key {}", key);
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
  void verifyOneGet(
       const std::vector<MemberInfo> &leaders,
       uint32_t clientInstNum,
       uint32_t dataNumPerClient,
       const store::KeyType &keyPrefix,
       const std::vector<std::map<store::KeyType, store::ValueType>> &expectedKey2value,
       const std::vector<std::map<store::KeyType, store::VersionType>> &expectedKey2version) {
     assert(expectedKey2value.size() == leaders.size());
     assert(expectedKey2version.size() == leaders.size());
     for (uint32_t i = 0; i < clientInstNum; ++i) {
       for (uint32_t j = 0; j < dataNumPerClient; ++j) {
         auto key = keyPrefix + std::to_string(i) + "_" + std::to_string(j);
         auto clusterIndex = getClusterIndexByKey(key);
         assert(clusterIndex < leaders.size());
         auto leader = leaders[clusterIndex];
         testGet(leader, i, key,
             [key, &expectedKey2value, &expectedKey2version, clusterIndex](const proto::Get::Response &resp) {
               SPDLOG_INFO("debug: checking key {} for cluster {}", key, clusterIndex);
               if (expectedKey2value[clusterIndex].find(key) == expectedKey2value[clusterIndex].end()) {
                 ASSERT_EQ(resp.result().code(), proto::ResponseCode::KEY_NOT_EXIST);
               } else {
                 ASSERT_EQ(resp.result().code(), proto::ResponseCode::OK);
                 ASSERT_EQ(resp.result().value(), expectedKey2value[clusterIndex].at(key));
                 ASSERT_EQ(resp.result().version(), expectedKey2version[clusterIndex].at(key));
               }
             });
       }
     }
     waitPreviousRequestsDone();
  }
  void verifyOneFollowerGet(
       const std::vector<MemberInfo> &followers,
       uint32_t clientInstNum,
       uint32_t dataNumPerClient,
       const store::KeyType &keyPrefix,
       const std::vector<std::map<store::KeyType, store::ValueType>> &expectedKey2value,
       const std::vector<std::map<store::KeyType, store::VersionType>> &expectedKey2version) {
     assert(expectedKey2value.size() == followers.size());
     assert(expectedKey2version.size() == followers.size());
     for (uint32_t i = 0; i < clientInstNum; ++i) {
       for (uint32_t j = 0; j < dataNumPerClient; ++j) {
         auto key = keyPrefix + std::to_string(i) + "_" + std::to_string(j);
         auto clusterIndex = getClusterIndexByKey(key);
         assert(clusterIndex < followers.size());
         auto follower = followers[clusterIndex];
         testFollowerGet(follower, i, key,
             [key, &expectedKey2value, &expectedKey2version, clusterIndex](const proto::Get::Response &resp) {
               SPDLOG_INFO("debug: checking key {} for cluster {}", key, clusterIndex);
               if (expectedKey2value[clusterIndex].find(key) == expectedKey2value[clusterIndex].end()) {
                 ASSERT_EQ(resp.result().code(), proto::ResponseCode::KEY_NOT_EXIST);
               } else {
                 ASSERT_EQ(resp.result().code(), proto::ResponseCode::OK);
                 ASSERT_EQ(resp.result().value(), expectedKey2value[clusterIndex].at(key));
                 ASSERT_EQ(resp.result().version(), expectedKey2version[clusterIndex].at(key));
               }
             });
       }
     }
     waitPreviousRequestsDone();
  }
  void verifyOneDelete(
       const std::vector<MemberInfo> &leaders,
       uint32_t clientInstNum,
       uint32_t dataNumPerClient,
       const store::KeyType &keyPrefix,
       const std::vector<std::map<store::KeyType, store::ValueType>> &expectedKey2value,
       const std::vector<std::map<store::KeyType, store::VersionType>> &expectedKey2version) {
     assert(expectedKey2value.size() == leaders.size());
     assert(expectedKey2version.size() == leaders.size());
     for (uint32_t i = 0; i < clientInstNum; ++i) {
       for (uint32_t j = 0; j < dataNumPerClient; ++j) {
         auto key = keyPrefix + std::to_string(i) + "_" + std::to_string(j);
         auto clusterIndex = getClusterIndexByKey(key);
         assert(clusterIndex < leaders.size());
         auto leader = leaders[clusterIndex];
         testDelete(leader, i, key,
             [key, &expectedKey2value, &expectedKey2version, clusterIndex](const proto::Delete::Response &resp) {
               ASSERT_EQ(resp.result().code(), proto::ResponseCode::OK);
               ASSERT_EQ(resp.result().value(), expectedKey2value[clusterIndex].at(key));
               ASSERT_EQ(resp.result().version(), expectedKey2version[clusterIndex].at(key));
             });
       }
     }
     waitPreviousRequestsDone();
  }

 protected:
  std::mutex mMutex;
  /// calculate the number of reqs and resp
  std::atomic<uint32_t> mReqNum = 0;
  std::atomic<uint32_t> mRespNum = 0;
};

}  /// namespace goblin::kvengine::model

#endif  // SERVER_TEST_MODEL_COMMANDTEST_H_
