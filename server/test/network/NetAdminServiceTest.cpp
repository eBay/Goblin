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

#ifndef SERVER_TEST_NETWORK_NETADMINSERVICETEST_H_
#define SERVER_TEST_NETWORK_NETADMINSERVICETEST_H_

#include <gtest/gtest.h>
#include <infra/util/Util.h>
#include "../../src/kv-engine/utils/TPRegistryEx.h"
#include "../../src/object-manager/ObjectManager.h"
#include "../../src/object-store/ObjectStore.h"
#include "../mock/MockNetAdminClient.h"
#include "../mock/MockE2ETest.h"

namespace goblin::kvengine::network {

using gringofts::app::protos::AppNetAdmin;
using gringofts::raft::MemberInfo;

class NetAdminServiceTest: public mock::MockE2ETest {
 public:
  explicit NetAdminServiceTest(uint64_t clusterNum = 1) : mock::MockE2ETest(clusterNum) {}

  void testGetMemberOffsetsAsync(const MemberInfo &server,
      int32_t clientInstIndex,
      const mock::GetMemberOffsetsCallBack &cb) {
    auto &addr = member2NetadminAddr[server.mAddress];
    auto it = mNetAdminClients.find(addr);
    assert(it != mNetAdminClients.end());
    assert(it->second.size() > clientInstIndex);
    mReqNum += 1;
    mNetAdminClients[addr][clientInstIndex]->getMemberOffsetsAsync(
              [this, cb](const GetMemberOffsets_Response &resp) {
      std::lock_guard<std::mutex> lock(mMutex);
      cb(resp);
      /// response num should be added after cb
      /// so that everything happened in cb could be seen
      /// after response num is incremented
      mRespNum +=1;
    });
  }

  void verifyGetMemberOffsets(
      const std::vector<MemberInfo> &leaders,
      const uint32_t &startClientIndex,
      const uint32_t &endClientIndex) {
    for (uint32_t i = startClientIndex; i < endClientIndex; ++i) {
      for (auto clusterIndex = 0; clusterIndex < leaders.size(); ++clusterIndex) {
        auto &leader = leaders[clusterIndex];
        testGetMemberOffsetsAsync(leader, i, [](const GetMemberOffsets_Response &resp) {
            ASSERT_EQ(resp.header().code(), proto::ResponseCode::OK);
            auto leader = resp.leader();
            SPDLOG_INFO("leader addr = {}, offset = {}", leader.server(), leader.offset());
            for (auto follower : resp.followers()) {
              SPDLOG_INFO("follower addr = {}, offset = {}",
                  follower.server(), follower.offset());
            }
        });
      }
    }
  }
  void verifyGetMemberOffsetsInFollower(
    const std::vector<MemberInfo> &followers,
    const uint32_t &startClientIndex,
    const uint32_t &endClientIndex) {
    for (uint32_t i = startClientIndex; i < endClientIndex; ++i) {
      for (auto clusterIndex = 0; clusterIndex < followers.size(); ++clusterIndex) {
        auto &follower = followers[clusterIndex];
        testGetMemberOffsetsAsync(follower, i, [](const GetMemberOffsets_Response &resp) {
            ASSERT_EQ(resp.header().code(), proto::ResponseCode::NOT_LEADER);
            SPDLOG_INFO("Error: Get member offsets from followers, current leader is {}",
                resp.header().reserved());
        });
      }
    }
  }

 protected:
  std::mutex mMutex;
  /// calculate the number of reqs and resp
  std::atomic<uint32_t> mReqNum = 0;
  std::atomic<uint32_t> mRespNum = 0;
};

TEST_F(NetAdminServiceTest, GetMemberOffsetsTest) {
  uint32_t clientInstNum = 12;
  uint32_t memberOffsetsInstNum = 10;
  auto clusterNum = getClusterNum();
  std::vector<mock::MemberInfo> leaders;
  std::vector<uint64_t> leadersBeginLogIndex;
  for (auto i = 0; i < clusterNum; ++i) {
    auto leader = getCurAppLeader(i);
    initNetAdminClients(leader, clientInstNum);
    uint64_t beginLogIndex = getLeaderLastLogIndex(i);
    leaders.push_back(leader);
    leadersBeginLogIndex.push_back(beginLogIndex);
  }
  verifyGetMemberOffsets(leaders, memberOffsetsInstNum, clientInstNum);
}

TEST_F(NetAdminServiceTest, GetMemberOffsetsInFollowerTest) {
  uint32_t clientInstNum = 2;
  uint32_t memberOffsetsInstNum = 1;
  auto clusterNum = getClusterNum();
  std::vector<MemberInfo> followers;
  std::vector<uint64_t> leadersBeginLogIndex;
  for (auto i = 0; i < clusterNum; ++i) {
    auto members = getCurAppFollowers(i);
    assert(members.size() > 1);
    auto follower = members[rand() % members.size()];
    initNetAdminClients(follower, clientInstNum);
    followers.push_back(follower);
  }
  verifyGetMemberOffsetsInFollower(followers, memberOffsetsInstNum, clientInstNum);
}

}  /// namespace goblin::kvengine::network

#endif  // SERVER_TEST_NETWORK_NETADMINSERVICETEST_H_
