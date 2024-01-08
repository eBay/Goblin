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

#ifndef SERVER_TEST_MOCK_MOCKAPPCLUSTER_H_
#define SERVER_TEST_MOCK_MOCKAPPCLUSTER_H_

#include <map>

#include <infra/raft/RaftInterface.h>
#include <infra/raft/v2/RaftCore.h>
#include <infra/raft/v2/ClusterTestUtil.h>
#include <test_util/SyncPointProcessor.h>

#include "../../src/kv-engine/utils/ClusterInfoUtil.h"

#include "MockAppClient.h"

namespace goblin {
namespace mock {

using gringofts::raft::MemberInfo;
using gringofts::raft::v2::ClusterTestUtil;

template<typename ClusterType>
class MockAppCluster : public ClusterTestUtil {
 public:
  MockAppCluster() : ClusterTestUtil([](const INIReader &reader) {
    auto pair = kvengine::utils::ClusterInfoUtil::resolveAllClusters(reader);
    return std::make_tuple(pair.first, pair.second);
  }) {}

  /// disallow copy ctor and copy assignment
  MockAppCluster(const MockAppCluster &) = delete;
  MockAppCluster &operator=(const MockAppCluster &) = delete;

  /// disallow move ctor and move assignment
  MockAppCluster(MockAppCluster &&) = delete;
  MockAppCluster &operator=(MockAppCluster &&) = delete;

  void setupAllServers(const std::vector<std::string> &configPaths);
  void setupAllServers(const std::vector<std::string> &configPaths, const std::vector<gringofts::SyncPoint> &points);
  void killAllServers(bool tearDown = true);
  MemberInfo setupRaftServer(const std::string &configPath);
  void setupServer(const std::string &configPath, const MemberInfo &raftMember);
  void killServer(const MemberInfo &member);

  std::vector<MemberInfo> getAllRaftMemberInfo();
  std::vector<MemberInfo> getAllAppMemberInfo();
  MemberInfo waitAndGetRaftLeader();
  MemberInfo getAppLeader();
  const ClusterType &getClusterInst(const MemberInfo &member);

  /// for sync point
  void enableAllSyncPoints() {
    gringofts::SyncPointProcessor::getInstance().enableProcessing();
    /// gringofts syncpoint
    ClusterTestUtil::enableAllSyncPoints();
  }
  void disableAllSyncPoints() {
    gringofts::SyncPointProcessor::getInstance().disableProcessing();
    /// gringofts syncpoint
    ClusterTestUtil::disableAllSyncPoints();
  }
  void resetSyncPoints(const std::vector<gringofts::SyncPoint> &points) {
    gringofts::SyncPointProcessor::getInstance().reset(points);
  }

 private:
  const uint64_t mCFFactor = 4;
  const std::string mWSNamePrefix = "cf_for_app";
  std::map<MemberInfo, std::unique_ptr<std::thread>> mThreads;
  std::map<MemberInfo, std::unique_ptr<ClusterType>> mAppInsts;
};

}  /// namespace mock
}  /// namespace goblin

#include "MockAppCluster.hpp"

#endif  // SERVER_TEST_MOCK_MOCKAPPCLUSTER_H_
