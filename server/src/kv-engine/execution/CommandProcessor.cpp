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

#include "CommandProcessor.h"
#include "../model/KVGetCommand.h"

#include "../store/VersionStore.h"

namespace goblin::kvengine::execution {

proto::ResponseCode toResponseCode(const utils::Status &s) {
  if (s.isOK()) {
    return proto::ResponseCode::OK;
  }
  if (s.isPrecondUnMatched()) {
    return proto::ResponseCode::PRECOND_NOT_MATCHED;
  }
  if (s.isInvalidArg() || s.isNotSupported()) {
    return proto::ResponseCode::BAD_REQUEST;
  }
  if (s.isWrongRoute()) {
    return proto::ResponseCode::WRONG_ROUTE;
  }
  if (s.isMigratedRoute()) {
    return proto::ResponseCode::MIGRATED_ROUTE;
  }
  if (s.isClientRouteOutOfDate()) {
    return proto::ResponseCode::CLIENT_ROUTE_OUT_OF_DATE;
  }
  if (s.isServerRouteOutOfDate()) {
    return proto::ResponseCode::SERVER_ROUTE_OUT_OF_DATE;
  }
  SPDLOG_ERROR("unexpected response status {}", s.getDetail());
  return proto::ResponseCode::GENERAL_ERROR;
}

void CommandProcessor::process(const std::shared_ptr<model::Command> &command) {
  auto context = command->getContext();
  /// metrics, out of queue
  context->setCommandOutQueueTimeInNanos();
  gringofts::raft::RaftRole curRole = gringofts::raft::RaftRole::Follower;
  uint64_t curTerm = 0;
  bool isStaleRead = command->isFollowerReadCmd();
  #ifdef DISABLE_FOLLOWER_READ
    isStaleRead = false;
  #endif
  // SPDLOG_INFO("debug: isStaleRead = {}", isStaleRead);
  do {
    mRaftStorePtr->getRoleAndTerm(&curRole, &curTerm);
    /// SPDLOG_INFO("debug: get role {} and term {}", curRole, curTerm);
    /// check if waiting for leader ready
    /**
    * allowStale=true & read: follower can process the read requests
    * otherwise: only leader can process the requests
    **/
    if (isStaleRead) {
      break;
    }
    if (curRole != gringofts::raft::RaftRole::Leader) {
      context->fillResponseAndReply(
          proto::ResponseCode::NOT_LEADER, "NotLeaderAnyMore", mRaftStorePtr->getLeaderHint());
      return;
    }
    if (mCurTerm == curTerm) {
      break;
    }
    /// only one thread needs to wait for leader ready
    std::unique_lock<std::mutex> lock(mBecomeLeaderMutex);
    if (mCurTerm == curTerm) {
      /// SPDLOG_INFO("debug: no need to wait leader");
      break;
    }
    /// wait until all running commands finish
    auto maxWaitInSec = std::chrono::seconds(3);
    std::unique_lock processLock{mProcessComamndMutex};
    auto cvSucceed = mProcessCommandCv.wait_for(processLock, maxWaitInSec, [this]() { return mRunningCmdCnt == 0;  });
    assert(cvSucceed);

    auto start = gringofts::TimeUtil::currentTimeInNanos();
    const uint64_t ONE_SECOND_IN_NS = 1000 * 1000 * 1000;
    /// SPDLOG_INFO("debug: going to wait leader");
    uint64_t lastLogIndex;
    std::shared_ptr<proto::Bundle> lastBundle;
    std::vector<gringofts::raft::MemberInfo> clusterInfo;
    auto s = mRaftStorePtr->waitTillLeaderIsReadyOrStepDown(
        curTerm, &lastLogIndex, &lastBundle, &clusterInfo);
    if (s.isNotLeader()) {
      /// in this case, current thread will release the lock
      /// but other threads will fail as well since curTerm is no longer a valid term
      mKVStorePtr->clear();
      continue;
    }
    if (mBecomeLeaderCallBack) {
      s = mBecomeLeaderCallBack(clusterInfo);
      if (!s.isOK()) {
        mKVStorePtr->clear();
        context->fillResponseAndReply(
            proto::ResponseCode::GENERAL_ERROR, "fail after become leader", mRaftStorePtr->getLeaderHint());
        return;
      }
    }
    /// by default, we use 0 as max version
    store::VersionType maxVersionInLeader = store::VersionStore::kInvalidVersion;
    if (lastBundle) {
      model::Event::getVersionFromBundle(*lastBundle, &maxVersionInLeader);
    }
    mVersionStorePtr->recoverMaxVersion(maxVersionInLeader);
    mKVStorePtr->clear();
    mKVStorePtr->flush();
    mCurTerm = curTerm;
    mRaftStorePtr->updateLogIndexWhenNewLeader(lastLogIndex);
    auto end = gringofts::TimeUtil::currentTimeInNanos();
    SPDLOG_INFO("it took {}us to become leader", (end - start)/1000);
  } while (mCurTerm != curTerm);

  /*** run a command ***/
  {
    std::unique_lock lock{mProcessComamndMutex};
    mRunningCmdCnt++;
  }
  if (isStaleRead) {
    doFollowerReadExecute(command);
  } else {
    doExecute(command);
  }
  {
    std::unique_lock lock{mProcessComamndMutex};
    mRunningCmdCnt--;
    mProcessCommandCv.notify_one();
  }
}

void CommandProcessor::doFollowerReadExecute(const std::shared_ptr<model::Command> &command) {
  /// SPDLOG_INFO("debug: execute a read command from the follower");
  auto context = command->getContext();

  auto s = command->prepare(mKVStorePtr);
  if (!s.isOK()) {
    context->fillResponseAndReply(toResponseCode(s), s.getDetail(), std::nullopt);
    return;
  }
  auto curTime = gringofts::TimeUtil::currentTimeInNanos();
  /// command lock timestamp
  context->setCommandLockTimeInNanos();
  /// auto prepareLatency = (curTime - context->getCreatedTimeInNanos()) / 1000000.0;
  /// SPDLOG_INFO("debug: prepare: {}", prepareLatency);

  model::EventList events;
  bool hasError = true;
  do {
    /// command pre-execute timestamp
    context->setCommandPreExecutedTimeInNanos();

    s = command->execute(mKVStorePtr, &events);
    if (!s.isOK() && !s.isNotFound()) {
      /// reply
      break;
    }
    /// command executed timestamp
    context->setCommandExecutedTimeInNanos();
    if (events.empty()) {
      /// reply
      break;
    }
    store::VersionType maxReadVersion = store::VersionStore::kInvalidVersion;
    for (auto &e : events) {
      auto *read = dynamic_cast<const model::ReadEvent*>(e.get());
      assert(read != nullptr);
      if (read->isNotFound()) {
        /// if not found, we wait until the latest version is commmitted
        maxReadVersion = std::max(maxReadVersion, mVersionStorePtr->getMaxAllocatedVersion());
      } else {
        maxReadVersion = std::max(maxReadVersion, read->getValueVersion());
      }
    }
    /// metrics, before raft commit
    context->setBeforeRaftCommitTimeInNanos();
    /// if all are read events, no need to allocate a new version or finish
    context->initSuccessResponse(mVersionStorePtr->getCurMaxVersion(), events);
    /// if all are read events, we reply directly, setting expectedIndex to zero
    mRaftStorePtr->replyAsync(0, this->mCurTerm, maxReadVersion, context, true);
    hasError = false;
  } while (0);
  /// we guarantee that finish will not fail
  assert(command->finish(mKVStorePtr, events).isOK());
  if (hasError) {
    /// case1: get some errors when executing, reply directly
    /// case2: no events are generated, reply directly, typically in connect command
    context->initSuccessResponse(mVersionStorePtr->getCurMaxVersion(), {});
    context->fillResponseAndReply(toResponseCode(s), s.getDetail(), std::nullopt);
  }
}

void CommandProcessor::doExecute(const std::shared_ptr<model::Command> &command) {
  auto context = command->getContext();

  auto s = command->prepare(mKVStorePtr);
  if (!s.isOK()) {
    context->fillResponseAndReply(toResponseCode(s), s.getDetail(), std::nullopt);
    return;
  }
  auto curTime = gringofts::TimeUtil::currentTimeInNanos();
  /// command lock timestamp
  context->setCommandLockTimeInNanos();
  /// auto prepareLatency = (curTime - context->getCreatedTimeInNanos()) / 1000000.0;
  /// SPDLOG_INFO("debug: prepare: {}", prepareLatency);

  model::EventList events;
  bool hasError = true;
  do {
    if (!context->skipPreExecuteCB() && mPreExecuteCallBack) {
      s = mPreExecuteCallBack(*context, *mKVStorePtr);
      if (!s.isOK()) {
        /// reply
        break;
      }
    }
    /// command pre-execute timestamp
    context->setCommandPreExecutedTimeInNanos();

    s = command->execute(mKVStorePtr, &events);
    if (!s.isOK() && !s.isNotFound()) {
      /// reply
      break;
    }
    /// command executed timestamp
    context->setCommandExecutedTimeInNanos();
    if (events.empty()) {
      /// reply
      break;
    }
    store::VersionType expectedVersion = store::VersionStore::kInvalidVersion;
    model::EventList nonReadEvents;
    /// maxReadVersion is the version of the snapshot that we generates read events.
    /// this version must be committed before we reply response to client.
    store::VersionType maxReadVersion = store::VersionStore::kInvalidVersion;
    for (auto &e : events) {
      if (e->getType() == model::EventType::READ) {
        auto *read = dynamic_cast<const model::ReadEvent*>(e.get());
        assert(read != nullptr);
        store::VersionType curMaxReadVersion = store::VersionStore::kInvalidVersion;
        if (read->isNotFound() && read->getValueVersion() == store::VersionStore::kInvalidVersion) {
          /// case1:
          ///  If the key is not found, and there is no deleted record, curMaxReadVersion = current committed version,
          ///  which means on current_committed_version, this key is not found.
          curMaxReadVersion = mVersionStorePtr->getCurMaxVersion();
        } else {
          /// case2: If the key is not found, but there is its deleted record, curMaxReadVersion = delete record version
          /// case3: If the key exists, curMaxReadVersion = value version of the key
          curMaxReadVersion = read->getValueVersion();
        }
        maxReadVersion = std::max(maxReadVersion, curMaxReadVersion);
      } else if (e->getType() == model::EventType::IMPORT) {
        auto *import = dynamic_cast<const model::ImportEvent*>(e.get());
        assert(import != nullptr);
        expectedVersion = import->getImportedVersion();
        assert(events.size() == 1);
      } else {
        if (e->getType() == model::EventType::WRITE && e->isNoOP()) {
          continue;
        }
        nonReadEvents.push_back(e);
      }
    }
    /// metrics, before raft commit
    context->setBeforeRaftCommitTimeInNanos();
    if (nonReadEvents.empty()) {
      /// if all are read events, no need to allocate a new version or finish
      context->initSuccessResponse(mVersionStorePtr->getCurMaxVersion(), events);
      /// if all are read events, we reply directly, setting expectedIndex to zero
      mRaftStorePtr->replyAsync(0, this->mCurTerm, maxReadVersion, context);
    } else {
      /// allocate a new version and persist through raft
      /// these two operations should be finished in an atomic way
      mVersionStorePtr->allocateNextVersion(
          [this, context, &events, &nonReadEvents](const store::VersionType &version) {
          for (auto &e : events) {
            e->assignVersion(version);
          }
          context->initSuccessResponse(mVersionStorePtr->getCurMaxVersion(), events);
          mRaftStorePtr->persistAsync(this->mCurTerm, context, version, nonReadEvents);
          }, expectedVersion);
    }
    hasError = false;
  } while (0);
  /// we guarantee that finish will not fail
  assert(command->finish(mKVStorePtr, events).isOK());
  if (hasError) {
    /// case1: get some errors when executing, reply directly
    /// case2: no events are generated, reply directly, typically in connect command
    context->initSuccessResponse(mVersionStorePtr->getCurMaxVersion(), {});
    context->fillResponseAndReply(toResponseCode(s), s.getDetail(), std::nullopt);
  }
}
}  // namespace goblin::kvengine::execution
