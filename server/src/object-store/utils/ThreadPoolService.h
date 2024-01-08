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

#ifndef SERVER_SRC_OBJECT_STORE_UTILS_THREADPOOLSERVICE_H_
#define SERVER_SRC_OBJECT_STORE_UTILS_THREADPOOLSERVICE_H_

#include <atomic>
#include <list>
#include <memory>
#include <shared_mutex>
#include <thread>

#include <spdlog/spdlog.h>

namespace goblin::objectstore::utils {

using TaskID = uint64_t;
constexpr TaskID kInvalidTaskID = 0;

class ThreadPoolService {
 public:
  class Task {
   public:
      Task() {
        mTaskID = ThreadPoolService::allocateNewTaskID();
      }
      virtual ~Task() = default;

      virtual void run() = 0;

      TaskID mTaskID = kInvalidTaskID;
      /// 0:initial, 1:doing, 2:done
      std::atomic<uint64_t> flag = 0;
  };

  explicit ThreadPoolService(uint64_t maxThreadNum);

  /// forbidden copy/move
  ThreadPoolService(const ThreadPoolService&) = delete;
  ThreadPoolService& operator=(const ThreadPoolService&) = delete;

  ~ThreadPoolService();

  using TaskPtr = std::shared_ptr<Task>;
  void submit(TaskPtr task);
  uint64_t getQueueSize();
  static TaskID allocateNewTaskID();

 private:
  /// thread function for poping tasks from queue
  void popThreadMain();

  /// thread function for executing tasks
  void executeThreadMain();

  /// task queue, push_back() and pop_front()
  std::list<TaskPtr> mTaskQueue;
  std::atomic<uint64_t> mPendingReplyQueueSize = 0;
  mutable std::shared_mutex mMutex;

  /**
    * threading model
    */
  std::atomic<bool> mRunning = true;

  /// num of concurrently running threads
  uint64_t mConcurrency;

  std::thread mPopThread;
  std::vector<std::thread> mExecuteThreads;

  static std::atomic<TaskID> mMaxTaskID;
};

}  /// namespace goblin::objectstore::utils

#endif  // SERVER_SRC_OBJECT_STORE_UTILS_THREADPOOLSERVICE_H_
