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


#include "ReplyLoop.h"

#include <infra/common_types.h>
#include <spdlog/spdlog.h>

#include "../utils/TimeUtil.h"

namespace goblin::kvengine::raft {

ReplyLoop::ReplyLoop(
    const std::shared_ptr<store::VersionStore> &versionStore,
    const std::shared_ptr<gringofts::raft::RaftInterface> &raftImpl,
    const uint32_t writeReplyConcurrency,
    const uint32_t readReplyConcurrency,
    const uint32_t reportMetricConcurrency)
    : mVersionStore(versionStore),
    mRaftImpl(raftImpl),
    mSuccessWriteCounter(gringofts::getCounter("success_write_count", {})),
    mSuccessReadCounter(gringofts::getCounter("success_read_count", {})),
    mWriteReplyExecutionService([this](const TaskPtr &task) {replyTask(task);}, "writereply", writeReplyConcurrency),
    mReadReplyExecutionService([this](const TaskPtr &task) {replyTask(task);}, "readreply",
                                readReplyConcurrency),
    mReportMetricExecutionService([](const TaskPtr &task) {task->mContext->reportMetrics();}, "reportmetric",
                                  reportMetricConcurrency) {
  mWriteReplyExecutionService.start();
  mReadReplyExecutionService.start();
  mReportMetricExecutionService.start();

  SPDLOG_INFO("WriteReplyLoop started, Concurrency={}.", writeReplyConcurrency);
  SPDLOG_INFO("ReadReplyLoop started, Concurrency={}.", readReplyConcurrency);
  SPDLOG_INFO("ReportMetricLoop started, Concurrency={}.", reportMetricConcurrency);
}

ReplyLoop::~ReplyLoop() {
  mRunning = false;
  mReportMetricExecutionService.shutdown();
  mReadReplyExecutionService.shutdown();
  mWriteReplyExecutionService.shutdown();
}

void ReplyLoop::pushTask(
    uint64_t index,
    uint64_t term,
    const store::VersionType &version,
    std::shared_ptr<model::CommandContext> context,
    bool isFollowerReadTask) {

  /// TODO: set up a metric command
  SPDLOG_DEBUG("debug: index {}, term {}, reply queue {}", index, term, mTaskCount.load());

  auto taskPtr = std::make_shared<Task>();
  taskPtr->mIndex = index;
  taskPtr->mTerm = term;
  taskPtr->mVersion = version;
  taskPtr->mContext = context;
  taskPtr->followerReadTask = isFollowerReadTask;

  if (taskPtr->mIndex != 0) {
    // for write task
    mWriteReplyExecutionService.submit(taskPtr);
  } else {
    // for read task
    mReadReplyExecutionService.submit(taskPtr);
  }
}

bool ReplyLoop::waitTillCommittedOrQuit(uint64_t index, uint64_t term) {
  /**
   * compare <lastLogIndex, currentTerm> with <index, term>
   *
   *   <  , ==   same leader, keep waiting
   *
   *   >= , ==   same leader, keep waiting
   *
   *   <  , >    as new leader, quit
   *             as new follower, quit
   *
   *   >= , >    as new leader, keep waiting, will help commit entries from old leader
   *             as new follower, keep waiting, new leader might help commit
   */

  while (mRunning) {
    uint64_t term1 = 0;
    uint64_t term2 = 0;
    uint64_t lastLogIndex = 0;

    do {
      term1 = mRaftImpl->getCurrentTerm();
      lastLogIndex = mRaftImpl->getLastLogIndex();
      term2 = mRaftImpl->getCurrentTerm();
    } while (term1 != term2);

    assert(term1 >= term);

    if (lastLogIndex < index && term1 > term) {
      return false;   /// we can quit
    }

    if (mRaftImpl->getCommitIndex() < index) {
      usleep(1000);   /// sleep 1ms, retry
      continue;
    }

    gringofts::raft::LogEntry entry;
    assert(mRaftImpl->getEntry(index, &entry));
    return entry.term() == term;
  }

  SPDLOG_WARN("Quit since reply loop is stopped.");
  return false;
}

void ReplyLoop::replyTask(TaskPtr task) {
  auto ts1InNano = utils::TimeUtil::currentTimeInNanos();
  /// SPDLOG_INFO("debug: reply queue: {}", replyLatency);

  if (task->mIndex != 0) {
    /// write task, we wait for index
    bool isCommitted = waitTillCommittedOrQuit(task->mIndex, task->mTerm);
    if (!isCommitted) {
      task->mCode = proto::ResponseCode::NOT_LEADER;
      task->mMessage = "NotLeaderAnyMore";
    } else {
      mSuccessWriteCounter.increase();
    }
  } else {
    /// read task, we only wait for version
    auto curMaxVersion = mVersionStore->getCurMaxVersion();
    /// SPDLOG_INFO("debug: curMaxVersion = {}", curMaxVersion);
    if (!task->followerReadTask && task->mVersion > curMaxVersion) {
      /// the target version is not committed, reschedule this task
      /// mark this task as uncommitted to avoid duplicated metrics reporting

      /// SPDLOG_INFO("debug: waiting version {} for read, cur max {}", task->mVersion, curMaxVersion);
      pushTask(task->mIndex, task->mTerm, task->mVersion, task->mContext, false);
      return;
    } else {
      mSuccessReadCounter.increase();
    }
  }
  if (task->mContext) {
    task->mContext->setAfterRaftCommitTimeInNanos();
  }

  auto ts2InNano = utils::TimeUtil::currentTimeInNanos();

  if (task->mContext) {
    /// SPDLOG_INFO("debug: whole: {}", task->mOverallLatency / 1000000.0);
    /// response
    task->mContext->fillResponseAndReply(task->mCode, task->mMessage.c_str(), mRaftImpl->getLeaderHint());
  }

  auto ts3InNano = utils::TimeUtil::currentTimeInNanos();
  if (task->mContext) {
    task->mContext->setAfterReplyTimeInNanos();
  }
  if (task->mIndex != 0) {
    SPDLOG_DEBUG("received on <index,term>=<{},{}>, replyCode={}, replyMessage={}, "
                "waitTillCommit cost {}ms, reply cost {}ms.",
                task->mIndex, task->mTerm, task->mCode, task->mMessage,
                (ts2InNano - ts1InNano) / 1000000.0,
                (ts3InNano - ts2InNano) / 1000000.0);
  } else {
    SPDLOG_DEBUG("received on <index,term>=<{},{}>, replyCode={}, replyMessage={}, "
                "waitTillCommit cost {}ms, reply cost {}ms.",
                task->mIndex, task->mTerm, task->mCode, task->mMessage,
                (ts2InNano - ts1InNano) / 1000000.0,
                (ts3InNano - ts2InNano) / 1000000.0);
  }

  /// update the global max version
  mVersionStore->updateCurMaxVersion(task->mVersion);

  /// report metrics in multi-thread
  if (task->mContext) {
    mReportMetricExecutionService.submit(task);
  }
}

}  /// namespace goblin::kvengine::raft

