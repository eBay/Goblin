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

#ifndef SERVER_TEST_MOCK_MOCKAPPCLUSTER_HPP_
#define SERVER_TEST_MOCK_MOCKAPPCLUSTER_HPP_

#include <grpc++/grpc++.h>
#include <grpc++/security/credentials.h>

#include "../../src/kv-engine/KVEngineImpl.h"
#include "../../src/kv-engine/KVEngine.h"
#include "../../src/object-store/ObjectStore.h"

namespace goblin {
namespace mock {

template <typename ClusterType>
void MockAppCluster<ClusterType>::setupAllServers(const std::vector<std::string> &configPaths) {
  killAllServers();
  std::vector<MemberInfo> raftMembers;
  /// set up all raft servers
  for (auto &config : configPaths) {
    auto raftMember = setupRaftServer(config);
    raftMembers.push_back(raftMember);
  }
  /// then use raft servers set up all app servers
  uint32_t i = 0;
  for (auto &config : configPaths) {
    setupServer(config, raftMembers[i++]);
  }
}

template <typename ClusterType>
void MockAppCluster<ClusterType>::setupAllServers(
    const std::vector<std::string> &configPaths,
    const std::vector<gringofts::SyncPoint> &points) {
  killAllServers();
  gringofts::SyncPointProcessor::getInstance().setup(points);
  std::vector<MemberInfo> raftMembers;
  /// set up all raft servers
  for (auto &config : configPaths) {
    auto raftMember = setupRaftServer(config);
    raftMembers.push_back(raftMember);
  }
  /// then use raft servers set up all app servers
  uint32_t i = 0;
  for (auto &config : configPaths) {
    setupServer(config, raftMembers[i++]);
  }
}

template <typename ClusterType>
void MockAppCluster<ClusterType>::killAllServers(bool tearDownSyncPoint) {
  /// first kill all raft servers
  mRaftInsts.clear();
  mRaftInstClients.clear();

  for (auto &[member, app] : mAppInsts) {
    app->shutdown();
  }
  for (auto &[member, t] : mThreads) {
    t->join();
  }
  mAppInsts.clear();
  mThreads.clear();
  SPDLOG_INFO("killing all servers");
  if (tearDownSyncPoint) {
    gringofts::SyncPointProcessor::getInstance().tearDown();
  }
}

template <typename ClusterType>
MemberInfo MockAppCluster<ClusterType>::setupRaftServer(const std::string &configPath) {
  INIReader reader(configPath);
  const auto raftConfigPath = reader.Get("store", "raft.config.path", "UNKNOWN");
  return ClusterTestUtil::setupServer(raftConfigPath);
}

template <typename ClusterType>
void MockAppCluster<ClusterType>::setupServer(const std::string &configPath, const MemberInfo &raftMember) {
  SPDLOG_INFO("starting server using {}", configPath);
  INIReader reader(configPath);
  assert(mRaftInsts.find(raftMember) != mRaftInsts.end());
  auto raftImpl = mRaftInsts.at(raftMember);

  /// mock other components
  const auto &walDir = reader.Get("rocksdb", "wal.dir", "");
  const auto &dbDir = reader.Get("rocksdb", "db.dir", "");
  auto versionStore = std::make_shared<kvengine::store::VersionStore>();
  std::vector<kvengine::store::WSName> allWS;
  auto wsNum = 1 << mCFFactor;
  for (auto i = 0; i < wsNum; ++i) {
    allWS.push_back(mWSNamePrefix + std::to_string(i));
  }
  auto rocksDBKVStore = std::shared_ptr<kvengine::store::RocksDBKVStore>(new kvengine::store::RocksDBKVStore(walDir,
        dbDir,
        allWS,
        [factor = mCFFactor, wsPrefix = mWSNamePrefix](const kvengine::store::KeyType &key) {
        auto shardId = objectstore::ObjectStore::keyToShardId(key);
        auto wsName = objectstore::ObjectStore::shardIdToColumnFamily(wsPrefix, shardId, factor);
        return wsName;
        },
        &gringofts::SyncPointProcessor::getInstance()));
  auto crypto = std::make_shared<gringofts::CryptoUtil>();
  crypto->init(reader);
  const int replyConcurrency = reader.GetInteger("concurrency", "reply.concurrency", 5);
  auto replyLoop = std::make_shared<kvengine::raft::ReplyLoop>(versionStore, raftImpl, replyConcurrency);
  auto raftEventStore = std::make_shared<kvengine::raft::RaftEventStore>(raftImpl, crypto, replyLoop);
  std::string snapshotDir = reader.Get("snapshot", "dir", "UNKNOWN");
  assert(snapshotDir != "UNKNOWN");
  auto eventApplyLoop = std::shared_ptr<kvengine::execution::EventApplyLoop>(
      new kvengine::execution::EventApplyLoop(
        raftEventStore, rocksDBKVStore, snapshotDir,
        &gringofts::SyncPointProcessor::getInstance()));

  /// init app
  auto engineImpl = std::make_shared<kvengine::KVEngineImpl>();
  auto engine = std::make_shared<kvengine::KVEngine>();
  engine->InitForTest(engineImpl);
  auto appPtr = new ClusterType(configPath.c_str(), engine);
  auto becomeLeaderFunc = [appPtr](
      const std::vector<gringofts::raft::MemberInfo> &clusterInfo) {
    return appPtr->becomeLeaderCallBack(clusterInfo);
  };
  auto preExecuteFunc = [appPtr](kvengine::model::CommandContext &context, kvengine::store::KVStore &kvStore) {
    return appPtr->preExecuteCallBack(context, kvStore);
  };
  engineImpl->InitForTest(
      configPath.c_str(),
      allWS,
      [factor = mCFFactor, wsPrefix = mWSNamePrefix](const kvengine::store::KeyType &key) {
        auto shardId = objectstore::ObjectStore::keyToShardId(key);
        auto wsName = objectstore::ObjectStore::shardIdToColumnFamily(wsPrefix, shardId, factor);
        return wsName;
      },
      becomeLeaderFunc,
      preExecuteFunc,
      raftImpl,
      versionStore,
      rocksDBKVStore,
      replyLoop,
      raftEventStore,
      eventApplyLoop,
      &gringofts::SyncPointProcessor::getInstance());
  const auto receiverAddress = reader.Get("receiver", "ip.port", "UNKNOWN");
  MemberInfo appMember = raftMember;
  appMember.mAddress = receiverAddress;
  assert(mAppInsts.find(appMember) == mAppInsts.end());
  mAppInsts[appMember] = std::unique_ptr<ClusterType>(appPtr);
  mThreads[appMember] = (std::make_unique<std::thread>([appPtr]() {
        appPtr->run();
        }));
}

template <typename ClusterType>
void MockAppCluster<ClusterType>::killServer(const MemberInfo &member) {
  SPDLOG_INFO("killing server {}", member.toString());
  ClusterTestUtil::killServer(member);
  assert(mAppInsts.find(member) != mAppInsts.end());
  mAppInsts.erase(member);
  SPDLOG_INFO("server {} is down", member.toString());
  SPDLOG_INFO("threads are down");
}

template <typename ClusterType>
MemberInfo MockAppCluster<ClusterType>::waitAndGetRaftLeader() {
  return ClusterTestUtil::waitAndGetLeader();
}

template <typename ClusterType>
MemberInfo MockAppCluster<ClusterType>::getAppLeader() {
  auto raftMember = waitAndGetLeader();
  /// MemberInfo use id as key, so we can reuse the raftMember as appMember
  auto it = mAppInsts.find(raftMember);
  assert(it != mAppInsts.end());
  return it->first;
}

template <typename ClusterType>
const ClusterType& MockAppCluster<ClusterType>::getClusterInst(const MemberInfo &member) {
  auto it = mAppInsts.find(member);
  assert(it != mAppInsts.end());
  return *it->second;
}

template <typename ClusterType>
std::vector<MemberInfo> MockAppCluster<ClusterType>::getAllRaftMemberInfo() {
  std::vector<MemberInfo> members;
  for (auto &[member, raftInst] : mRaftInsts) {
    members.push_back(member);
  }
  return members;
}

template <typename ClusterType>
std::vector<MemberInfo> MockAppCluster<ClusterType>::getAllAppMemberInfo() {
  std::vector<MemberInfo> members;
  for (auto &[member, app] : mAppInsts) {
    members.push_back(member);
  }
  return members;
}
}  /// namespace mock
}  /// namespace goblin

#endif  // SERVER_TEST_MOCK_MOCKAPPCLUSTER_HPP_
