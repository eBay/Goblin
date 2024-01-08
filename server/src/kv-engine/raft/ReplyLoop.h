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

#ifndef SERVER_SRC_KV_ENGINE_RAFT_REPLYLOOP_H_
#define SERVER_SRC_KV_ENGINE_RAFT_REPLYLOOP_H_

#include <atomic>
#include <string>
#include <shared_mutex>

#include <infra/raft/RaftInterface.h>

#include "../model/Command.h"
#include "../store/VersionStore.h"
#include "../utils/Status.h"
#include "../execution/ExecutionServiceImpl.h"

namespace goblin::kvengine::raft {

class ReplyLoop {
 public:
  ReplyLoop(
      const std::shared_ptr<store::VersionStore> &versionStore,
      const std::shared_ptr<gringofts::raft::RaftInterface> &raftImpl,
      const uint32_t writeReplyConcurrency = 5,
      const uint32_t readReplyConcurrency = 5,
      const uint32_t reportMetricConcurrency = 10);
  ReplyLoop(const ReplyLoop&) = delete;
  ReplyLoop &operator=(const ReplyLoop &) = delete;

  virtual ~ReplyLoop();

  /// send a task to reply loop
  void pushTask(
      uint64_t index,
      uint64_t term,
      const store::VersionType &version,
      std::shared_ptr<model::CommandContext> context,
      bool isFollowerReadTask = false);

 private:
  struct Task {
    /// <index, term> of log entry of raft
    uint64_t mIndex = 0;
    uint64_t mTerm  = 0;

    uint64_t mVersion = store::VersionStore::kInvalidVersion;

    bool followerReadTask = false;

    /// context to reply
    std::shared_ptr<model::CommandContext> mContext = nullptr;

    /// code and msg to reply if <index, term> is committed
    proto::ResponseCode mCode = proto::ResponseCode::OK;
    std::string mMessage = "Success";
  };

  using TaskPtr = std::shared_ptr<Task>;

  /// reply a task
  void replyTask(TaskPtr task);

  /// wait for <index,term> to be committed or quit
  bool waitTillCommittedOrQuit(uint64_t index, uint64_t term);

  std::shared_ptr<store::VersionStore> mVersionStore;
  std::shared_ptr<const gringofts::raft::RaftInterface> mRaftImpl;

  /**
   * threading model
   */
  std::atomic<bool> mRunning = true;

  santiago::MetricsCenter::CounterType mSuccessWriteCounter;
  santiago::MetricsCenter::CounterType mSuccessReadCounter;

  /// write reply loop, process tasks that contains put, delete
  execution::ExecutionServiceImpl<TaskPtr> mWriteReplyExecutionService;
  /// read reply loop, process tasks that only contains read operations
  execution::ExecutionServiceImpl<TaskPtr> mReadReplyExecutionService;
  /// report metric loop
  execution::ExecutionServiceImpl<TaskPtr> mReportMetricExecutionService;
};

}  /// namespace goblin::kvengine::raft

#endif  // SERVER_SRC_KV_ENGINE_RAFT_REPLYLOOP_H_

