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

#ifndef SERVER_SRC_KV_ENGINE_MODEL_COMMAND_H_
#define SERVER_SRC_KV_ENGINE_MODEL_COMMAND_H_

#ifndef SPDLOG_ACTIVE_LEVEL
#ifdef DEBUG
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#endif
#endif

#include <functional>

#include <infra/monitor/MonitorTypes.h>
#include <infra/util/TimeUtil.h>

#include "../../../protocols/generated/common.pb.h"
#include "../../../protocols/generated/service.pb.h"
#include "../utils/ClusterInfoUtil.h"
#include "../utils/Status.h"
#include "../store/KVStore.h"
#include "../store/VersionStore.h"
#include "Event.h"

namespace goblin::kvengine::model {

/// TODO: refactor this class to be a template class
class CommandContext {
 public:
  CommandContext() : mType("default") {}
  explicit CommandContext(const std::string &commandType) : mType(commandType) {}
  void setCreateTimeInNanos() {
     mMetrics.mCommandCreateTimeInNanos = gringofts::TimeUtil::currentTimeInNanos();
  }

  void setCommandOutQueueTimeInNanos() {
     mMetrics.mCommandOutQueueTimeInNanos = gringofts::TimeUtil::currentTimeInNanos();
  }

  void setCommandLockTimeInNanos() {
     mMetrics.mCommandLockTimeInNanos = gringofts::TimeUtil::currentTimeInNanos();
  }

  void setCommandPreExecutedTimeInNanos() {
     mMetrics.mCommandPreExecutedTimeInNanos = gringofts::TimeUtil::currentTimeInNanos();
  }

  void setCommandExecutedTimeInNanos() {
     mMetrics.mCommandExecutedTimeInNanos = gringofts::TimeUtil::currentTimeInNanos();
  }

  void setBeforeRaftCommitTimeInNanos() {
     mMetrics.mBeforeRaftCommitTimeInNanos = gringofts::TimeUtil::currentTimeInNanos();
  }

  void setAfterRaftCommitTimeInNanos() {
    mMetrics.mAfterRaftCommitTimeInNanos = gringofts::TimeUtil::currentTimeInNanos();
  }

  void setAfterReplyTimeInNanos() {
    mMetrics.mAfterReplyTimeInNanos = gringofts::TimeUtil::currentTimeInNanos();
  }

  void reportMetrics() {
    static const prometheus::Histogram::BucketBoundaries bucketBoundaries = prometheus::Histogram::BucketBoundaries {
        0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9,
        1, 1.048576, 1.398101, 1.747626, 2.097151, 2.446676, 2.796201, 3.145726, 3.495251, 3.844776, 4.194304,
        5.592405, 6.990506, 8.388607, 9.786708,
        11.184809, 12.58291, 13.981011, 15.379112, 16.777216, 22.369621, 27.962026, 33.554431, 39.146836, 44.739241,
        50.331646, 55.924051, 61.516456, 67.108864, 89.478485,
        111.848106, 134.217727, 156.587348, 178.956969, 201.32659, 223.696211, 246.065832, 268.435456, 357.913941,
        447.392426, 536.870911, 626.349396, 715.827881, 805.306366, 894.784851, 984.263336,
        1073.741824, 1431.655765, 1789.569706, 2147.483647, 2505.397588, 2863.311529, 3221.22547, 3579.139411,
        3937.053352, 4294.967296, 5726.623061, 7158.278826, 8589.934591,
        10021.59036, 11453.24612, 12884.90189, 14316.55765, 15748.21342, 17179.86918, 22906.49225, 28633.11531,
        30000.0};

    /// metrics, time cost stay in worker's queue
    auto histogramInQueue = gringofts::getHistogram("request_call_stay_in_queue_latency_in_ms", {{"comtype", mType}},
                                                    bucketBoundaries);
    auto inQueueLatency = (mMetrics.mCommandOutQueueTimeInNanos -
        mMetrics.mCommandCreateTimeInNanos) / 1000000.0;
    histogramInQueue.observe(inQueueLatency);

    /// metrics, time cost till command locked
    auto histogramLocked = gringofts::getHistogram("request_command_locked_latency_in_ms", {{"comtype", mType}},
                                                   bucketBoundaries);
    auto lockedLatency = (mMetrics.mCommandLockTimeInNanos -
        mMetrics.mCommandOutQueueTimeInNanos) / 1000000.0;
    histogramLocked.observe(lockedLatency);

    /// metrics, time cost till command pre-executed
    auto histogramPreExecuted = gringofts::getHistogram("request_command_preexecuted_latency_in_ms",
                                                        {{"comtype", mType}}, bucketBoundaries);
    auto preExecutedLatency = (mMetrics.mCommandPreExecutedTimeInNanos -
        mMetrics.mCommandLockTimeInNanos) / 1000000.0;
    histogramPreExecuted.observe(preExecutedLatency);

    /// metrics, time cost till command executed
    auto histogramExecuted = gringofts::getHistogram("request_command_executed_latency_in_ms", {{"comtype", mType}},
                                                     bucketBoundaries);
    auto executedLatency = (mMetrics.mCommandExecutedTimeInNanos -
        mMetrics.mCommandPreExecutedTimeInNanos) / 1000000.0;
    histogramExecuted.observe(executedLatency);

    /// metrics, time exclude commit and reply
    auto histogramUncommit = gringofts::getHistogram("request_call_without_commit_latency_in_ms",
                                                     {{"comtype", mType}}, bucketBoundaries);
    auto unCommitLatency = (mMetrics.mBeforeRaftCommitTimeInNanos - mMetrics.mCommandCreateTimeInNanos) / 1000000.0;
    histogramUncommit.observe(unCommitLatency);

    /// metrics, time for commit
    auto histogramCommit = gringofts::getHistogram("request_commit_latency_in_ms", {{"comtype", mType}},
                                                   bucketBoundaries);
    auto commitLatency = (mMetrics.mAfterRaftCommitTimeInNanos - mMetrics.mBeforeRaftCommitTimeInNanos) / 1000000.0;
    histogramCommit.observe(commitLatency);

    /// metrics, time for reply
    auto histogramReply = gringofts::getHistogram("request_reply_latency_in_ms", {{"comtype", mType}},
                                                  bucketBoundaries);
    auto replyLatency = (mMetrics.mAfterReplyTimeInNanos - mMetrics.mAfterRaftCommitTimeInNanos) / 1000000.0;
    histogramReply.observe(replyLatency);

    auto histogramOverall = gringofts::getHistogram("request_overall_latency_in_ms", {{"comtype", mType}},
                                                    bucketBoundaries);
    auto overallLatency = (mMetrics.mAfterReplyTimeInNanos - mMetrics.mCommandCreateTimeInNanos) / 1000000.0;
    histogramOverall.observe(overallLatency);

    /// for put/get/delete/cas handler to report their metrics
    reportSubMetrics();
  }

  virtual void initSuccessResponse(
      const store::VersionType &curMaxVersion, const model::EventList &events) {
     /// by default, we don't do anything
  }
  virtual void fillResponseAndReply(
      proto::ResponseCode code, const std::string &message, std::optional<uint64_t> leaderId) {
     /// by default, we don't do anything
  }
  virtual std::set<store::KeyType> getTargetKeys() {
     return {};
  }
  virtual proto::RequestHeader getRequestHeader() {
     return proto::RequestHeader();
  }
  virtual bool skipPreExecuteCB() {
     /// by default, skip preExecuteCallBack
     return true;
  }

 protected:
  virtual void reportSubMetrics() {
     /// by default, we don't do anything
  }

  /// metrics
  struct Metrics {
     /// command create time in nanos
     gringofts::TimestampInNanos mCommandCreateTimeInNanos;

     /// pull-out of queue
     gringofts::TimestampInNanos mCommandOutQueueTimeInNanos;

     /// lock time
     gringofts::TimestampInNanos mCommandLockTimeInNanos;

     /// command pre-executed completed
     gringofts::TimestampInNanos mCommandPreExecutedTimeInNanos;

     /// command executed completed
     gringofts::TimestampInNanos mCommandExecutedTimeInNanos;

     /// before events to be commit to raft
     gringofts::TimestampInNanos mBeforeRaftCommitTimeInNanos;

     /// after events to be commit to raft
     gringofts::TimestampInNanos mAfterRaftCommitTimeInNanos;

     /// after reply
     gringofts::TimestampInNanos mAfterReplyTimeInNanos;
  };

  Metrics mMetrics;
  const std::string mType;
};

class Command {
 public:
  Command() = default;
  virtual ~Command() = default;

  virtual std::shared_ptr<CommandContext> getContext() {
     /// if no customized context, provide a default context that only report metrics
     return std::make_shared<CommandContext>();
  }

  virtual bool isFollowerReadCmd() const {
     /// SPDLOG_INFO("debug: base command, isFollowerReadCmd = {}", false);
     return false;
  }

  /**
    * The CommandProcessor will invoke the following three interfaces in order
    */
  virtual utils::Status prepare(const std::shared_ptr<store::KVStore> &kvStore) = 0;

  virtual utils::Status execute(const std::shared_ptr<store::KVStore> &, EventList *) = 0;

  virtual utils::Status finish(const std::shared_ptr<store::KVStore> &kvStore, const EventList &events) = 0;
};

}  // namespace goblin::kvengine::model

#endif  // SERVER_SRC_KV_ENGINE_MODEL_COMMAND_H_
