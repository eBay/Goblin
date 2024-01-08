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
#include <rocksdb/db.h>

#include "../../src/kv-engine/execution/EventApplyLoop.h"
#include "CommandTest.h"

namespace goblin::kvengine::model {

using gringofts::raft::MemberInfo;

class ErrorCaseCommandTest : public CommandTest {
 protected:
  gringofts::SyncPointCallBack createMockFailReadCB() {
     return [=](void *statusArg, void *outValueArg) {
       auto status = static_cast<rocksdb::Status*>(statusArg);
       *status = rocksdb::Status::Corruption();
     };
  }
  gringofts::SyncPointCallBack createMockFailApplyCB(
      uint64_t targetIndex,
      std::atomic<bool> &hitFlag) {  // NOLINT(runtime/references)
     hitFlag = false;
     return [=, &hitFlag](void *statusArg, void *indexArg) {
       auto status = static_cast<utils::Status*>(statusArg);
       auto index = static_cast<uint64_t*>(indexArg);
       if (*index == targetIndex) {
         *status = utils::Status::error();
         hitFlag = true;
       }
     };
  }
  gringofts::SyncPointCallBack createVerifyRecoverCB(
      uint64_t expectedStartIndex,
      std::atomic<bool> &hitFlag) {  // NOLINT(runtime/references)
     hitFlag = false;
     return [=, &hitFlag](void *statusArg, void *indexArg) {
       auto index = static_cast<uint64_t*>(indexArg);
       if (!hitFlag) {
         /// the first index to apply must be within the range of kCommitBatchSize
         ASSERT_TRUE(expectedStartIndex - *index <= execution::EventApplyLoop::kApplyBatchSize);
         hitFlag = true;
       }
     };
  }
};

TEST_F(ErrorCaseCommandTest, GetKeyNotExist) {
  const std::string key = "GetKeyNotExistTestKey";
  auto clusterNum = getClusterNum();
  assert(clusterNum == 1);
  auto leader = getCurAppLeader(0);
  initClients(leader, 1);
  auto clusterIndex = getClusterIndexByKey(key);
  assert(clusterIndex == 0);
  testGet(leader, 0, key, [](const proto::Get::Response &resp) {
      ASSERT_EQ(resp.header().code(), proto::ResponseCode::OK);
      ASSERT_EQ(resp.result().code(), proto::ResponseCode::KEY_NOT_EXIST);
      });
  waitPreviousRequestsDone();
}

TEST_F(ErrorCaseCommandTest, ReadKVStoreFailed) {
  /// setup all syncpoints
  gringofts::SyncPointCallBack mockFailReadCB = createMockFailReadCB();
  std::vector<gringofts::SyncPoint> ps {
    {utils::TPRegistryEx::RocksDBKVStore_readKV_mockGetResult, mockFailReadCB, {}, gringofts::SyncPointType::Ignore}
  };
  beginSyncPointForOS(ps);

  const std::string key = "readKVStoreFailedTestKey";
  auto clusterNum = getClusterNum();
  assert(clusterNum == 1);
  auto leader = getCurAppLeader(0);
  initClients(leader, 1);
  auto clusterIndex = getClusterIndexByKey(key);
  assert(clusterIndex == 0);
  testGet(leader, 0, key, [](const proto::Get::Response &resp) {
      ASSERT_EQ(resp.header().code(), proto::ResponseCode::GENERAL_ERROR);
      ASSERT_EQ(resp.result().code(), proto::ResponseCode::GENERAL_ERROR);
      });
  waitPreviousRequestsDone();

  endSyncPointForOS();
}

TEST_F(ErrorCaseCommandTest, RecoverFromCrash) {
  uint64_t crashLogIndex = 56;
  /// setup syncpoints to make apply fail
  std::atomic<bool> hitMockFlag = false;
  gringofts::SyncPointCallBack mockFailApplyCB = createMockFailApplyCB(crashLogIndex, hitMockFlag);
  std::vector<gringofts::SyncPoint> ps {
    {utils::TPRegistryEx::EventApplyLoop_run_interceptApplyResult,
      mockFailApplyCB, {}, gringofts::SyncPointType::Ignore}
  };
  beginSyncPointForOS(ps);

  const std::string keyPrefix = "RecoverFromCrashTestKey";
  const std::string valuePrefix = "RecoverFromCrashTestValue";
  uint32_t clientInstNum = 10;
  uint32_t dataNumPerClient = 20;
  auto clusterNum = getClusterNum();
  std::vector<MemberInfo> leaders;
  for (auto i = 0; i < clusterNum; ++i) {
    auto leader = getCurAppLeader(i);
    initClients(leader, clientInstNum);
    leaders.push_back(leader);
  }
  /// before put, check the version
  verifyConnect(leaders, 0, clientInstNum, std::vector<uint64_t>(leaders.size(), store::VersionStore::kInvalidVersion));

  std::vector<std::map<store::KeyType, store::ValueType>> key2value(leaders.size());
  std::vector<std::map<store::KeyType, store::VersionType>> key2version(leaders.size());
  /// put data
  verifyOnePut(leaders, clientInstNum, dataNumPerClient, keyPrefix, valuePrefix, &key2value, &key2version);
  /// get data
  verifyOneGet(leaders, clientInstNum, dataNumPerClient, keyPrefix, key2value, key2version);

  while (!hitMockFlag) {
    sleep(1);
    SPDLOG_INFO("waiting for mock syncpoint to be hit");
  }

  SPDLOG_INFO("after put before recover");
  std::vector<uint64_t> maxVersionPerCluster;
  for (auto i = 0; i < leaders.size(); ++i) {
    uint64_t maxVersion = 0;
    for (auto &[k, version] : key2version[i]) {
      maxVersion = std::max(maxVersion, version);
    }
    maxVersionPerCluster.push_back(maxVersion);
  }
  /// after put but before recover, check the version
  verifyConnect(leaders, 0, clientInstNum, maxVersionPerCluster);

  /// setup syncpoints to check recover index
  std::atomic<bool> hitVerifyFlag = false;
  gringofts::SyncPointCallBack verifyCB = createVerifyRecoverCB(crashLogIndex, hitVerifyFlag);
  std::vector<gringofts::SyncPoint> ps2 {
    {utils::TPRegistryEx::EventApplyLoop_run_interceptApplyResult, verifyCB, {}, gringofts::SyncPointType::Ignore}
  };
  /// kill all servers and bring them up
  /// this will also clean all syncpoints
  restartAllServers(ps2, true);

  std::vector<MemberInfo> newLeaders;
  for (auto i = 0; i < clusterNum; ++i) {
    auto leader = getCurAppLeader(i);
    initClients(leader, clientInstNum);
    newLeaders.push_back(leader);
  }
  SPDLOG_INFO("after recover");
  /// the version should be recovered as well
  verifyConnect(newLeaders, 0, clientInstNum, maxVersionPerCluster);
  /// get data after recovery
  verifyOneGet(newLeaders, clientInstNum, dataNumPerClient, keyPrefix, key2value, key2version);

  ASSERT_TRUE(hitVerifyFlag);
  endSyncPointForOS();
}

}  /// namespace goblin::kvengine::model
