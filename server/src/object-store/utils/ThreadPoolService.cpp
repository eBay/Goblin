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

#include "ThreadPoolService.h"

#include <infra/common_types.h>

namespace goblin::objectstore::utils {

std::atomic<TaskID> ThreadPoolService::mMaxTaskID = kInvalidTaskID;

ThreadPoolService::ThreadPoolService(uint64_t maxThreadNum)
    : mConcurrency(maxThreadNum) {
  mPopThread = std::thread(&ThreadPoolService::popThreadMain, this);

  for (uint64_t i = 0; i < mConcurrency; ++i) {
    mExecuteThreads.emplace_back(&ThreadPoolService::executeThreadMain, this);
  }

  SPDLOG_INFO("ReplyLoop started, Concurrency={}.", mConcurrency);
}

ThreadPoolService::~ThreadPoolService() {
  mRunning = false;

  if (mPopThread.joinable()) {
    mPopThread.join();
  }

  for (auto &t : mExecuteThreads) {
    if (t.joinable()) {
      t.join();
    }
  }
}

void ThreadPoolService::submit(TaskPtr task) {
  /// write lock
  std::unique_lock<std::shared_mutex> lock(mMutex);
  mPendingReplyQueueSize += 1;
  mTaskQueue.push_back(task);
}

uint64_t ThreadPoolService::getQueueSize() {
  return mPendingReplyQueueSize;
}

TaskID ThreadPoolService::allocateNewTaskID() {
  return mMaxTaskID++;
}

void ThreadPoolService::popThreadMain() {
  pthread_setname_np(pthread_self(), "ThreadPoolService_pop");

  while (mRunning) {
    bool busy = false;

    {
      /// read lock
      std::shared_lock<std::shared_mutex> lock(mMutex);
      if (!mTaskQueue.empty() && mTaskQueue.front()->flag == 2) {
        busy = true;
      }
    }

    if (busy) {
      /// write lock
      std::unique_lock<std::shared_mutex> lock(mMutex);
      mTaskQueue.pop_front();
      mPendingReplyQueueSize -= 1;
    } else {
      usleep(1000);   /// nothing to do, sleep 1ms
    }
  }
}

void ThreadPoolService::executeThreadMain() {
  pthread_setname_np(pthread_self(), "ThreadPoolService_reply");

  while (mRunning) {
    TaskPtr taskPtr;

    {
      /// read lock
      std::shared_lock<std::shared_mutex> lock(mMutex);

      for (auto &currPtr : mTaskQueue) {
        uint64_t expected = 0;
        if (currPtr->flag.compare_exchange_strong(expected, 1)) {
          taskPtr = currPtr;
          break;
        }
      }
    }

    if (taskPtr) {
      taskPtr->run();
      /// release task
      taskPtr->flag = 2;
    } else {
      usleep(1000);   /// nothing to do, sleep 1ms
    }
  }
}

}  /// namespace goblin::objectstore::utils
