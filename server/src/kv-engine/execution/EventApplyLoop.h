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

#ifndef SERVER_SRC_KV_ENGINE_EXECUTION_EVENTAPPLYLOOP_H_
#define SERVER_SRC_KV_ENGINE_EXECUTION_EVENTAPPLYLOOP_H_

#include <infra/util/TestPointProcessor.h>
#include <infra/es/store/SnapshotUtil.h>

#include "../store/InMemoryKVStore.h"
#include "../store/RocksDBKVStore.h"
#include "../model/Event.h"
#include "../raft/RaftEventStore.h"
#include "../network/NetAdminServiceProvider.h"

namespace goblin {
namespace mock {
template <typename ClusterType>
class MockAppCluster;
}
}

namespace goblin::kvengine::execution {

class EventApplyService {
 public:
  virtual ~EventApplyService() = default;
  virtual store::VersionType getLastAppliedVersion() const = 0;
};

class EventApplyLoop : public EventApplyService,
                       public network::NetAdminServiceProvider {
 public:
  EventApplyLoop(
      std::shared_ptr<raft::RaftEventStore> raftEventStore,
      std::shared_ptr<store::KVStore> kvStore) :
      mRaftEventStore(raftEventStore), mKVStore(kvStore),
      mLastMilestoneGauge(gringofts::getGauge("last_applied_index", {})) {
  }
  EventApplyLoop(
      std::shared_ptr<raft::RaftEventStore> raftEventStore,
      std::shared_ptr<store::KVStore> kvStore,
      const std::string &snapshotDir) :
      mRaftEventStore(raftEventStore), mKVStore(kvStore),
      mSnapshotDir(snapshotDir),
      mLastMilestoneGauge(gringofts::getGauge("last_applied_index", {})) {
  }
  ~EventApplyLoop();
  void run();

  void shutdown() { mShouldExit = true; }

  store::VersionType getLastAppliedVersion() const override { return mLastMilestoneKeyVersion; }

  void truncatePrefix(uint64_t offsetKept) override {
    mRaftEventStore->truncatePrefix(offsetKept);
  }

  std::pair<bool, std::string> takeSnapshotAndPersist() const override {
    if (this->mRaftEventStore->isLeader()) {
      /// no snapshot to avoid io contention
      SPDLOG_INFO("avoid taking snapshot when leader");
      return std::make_pair(false, "leader no longer takes snapshot");
    }
    /// create Checkpoint of RocksDB is thread-safe,
    /// we don't need lock mLoopMutex.
    auto checkpointPath = this->mKVStore->createCheckpoint(this->mSnapshotDir);
    return std::make_pair(true, checkpointPath);
  }

  std::optional<uint64_t> getLatestSnapshotOffset() const override {
    /// RocksDBBacked StateMachine use checkpoint instead of snapshot
    return gringofts::SnapshotUtil::findLatestCheckpointOffset(this->mSnapshotDir);
  }

  uint64_t lastApplied() const override {
    return mLastMilestone;
  }

  std::optional<uint64_t> getLeaderHint() const override {
    return mRaftEventStore->getLeaderHint();
  }

  bool isLeader() const override {
    return mRaftEventStore->isLeader();
  }

  static constexpr uint32_t kApplyBatchSize = 100;
  static constexpr uint64_t kSaveMilestoneTimeoutInNano = (uint64_t)50 * 1000 * 1000;  /// 50ms

 protected:
  void recoverSelf();

  std::string mSnapshotDir;

  std::atomic<bool> mShouldExit = false;

  mutable std::mutex mLoopMutex;

  /// should recover when started every time
  std::atomic<bool> mShouldRecover = true;

  uint64_t mLastMilestone = 0;
  gringofts::TimestampInNanos mLastSaveMilestoneTimeInNano = 0;
  std::atomic<store::VersionType> mLastMilestoneKeyVersion = 0;

  std::shared_ptr<store::KVStore> mKVStore;
  std::shared_ptr<raft::RaftEventStore> mRaftEventStore;

 private:
  EventApplyLoop(
      std::shared_ptr<raft::RaftEventStore> raftEventStore,
      std::shared_ptr<store::KVStore> kvStore,
      gringofts::TestPointProcessor *processor);
  EventApplyLoop(
      std::shared_ptr<raft::RaftEventStore> raftEventStore,
      std::shared_ptr<store::KVStore> kvStore,
      const std::string &snapshotDir,
      gringofts::TestPointProcessor *processor);
  gringofts::TestPointProcessor *mTPProcessor = nullptr;
  template <typename ClusterType>
  friend class mock::MockAppCluster;

  santiago::MetricsCenter::GaugeType mLastMilestoneGauge;
};
}  /// namespace goblin::kvengine::execution

#endif  // SERVER_SRC_KV_ENGINE_EXECUTION_EVENTAPPLYLOOP_H_
