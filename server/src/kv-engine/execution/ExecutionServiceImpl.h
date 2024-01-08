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

#ifndef SERVER_SRC_KV_ENGINE_EXECUTION_EXECUTIONSERVICEIMPL_H_
#define SERVER_SRC_KV_ENGINE_EXECUTION_EXECUTIONSERVICEIMPL_H_
#include <vector>
#include <memory>
#include <mutex>

#include "ExecutionService.h"
#include "QueueWorker.h"

namespace goblin::kvengine::execution {

template<typename T>
class ExecutionServiceImpl : public ExecutionService<T> {
 public:
  using GetQueueIdFunc = std::function<size_t(const T &)>;
  typedef QueueWorker<T> Worker;
  typedef std::unique_ptr<Worker> WorkerPtr;
  ExecutionServiceImpl(typename Worker::ProcessorPtr processor, GetQueueIdFunc getQueueIdFunc, const std::string &name,
                       int size):
    mProcessor(processor), mPointer(0), mGetQueueIdFunc(getQueueIdFunc) {
      mWorkers.reserve(size);
      for (int i = 0; i < size; ++i) {
        mWorkers.push_back(std::make_unique<Worker>(mProcessor, name, i + 1));
      }
  }
  ExecutionServiceImpl(typename Worker::ProcessorPtr processor, const std::string &name, int size):
    ExecutionServiceImpl(processor, [this](const T &) {
      uint64_t index = mPointer.fetch_add(1);
      return index % mWorkers.size();}, name, size) {
  }
  ExecutionServiceImpl(typename FuncProcessor<T>::Func func, GetQueueIdFunc getQueueIdFunc, const std::string &name,
                       int size):
    ExecutionServiceImpl(std::make_shared<FuncProcessor<T>>(func), getQueueIdFunc, name, size) {
  }
  ExecutionServiceImpl(typename FuncProcessor<T>::Func func, const std::string &name, int size):
    ExecutionServiceImpl(std::make_shared<FuncProcessor<T>>(func), name, size) {
  }

  void start() override {
    for (auto &worker : mWorkers) {
      worker->start();
    }
  }
  void shutdown() override {
    SPDLOG_INFO("shutdown worker");
    for (auto &worker : mWorkers) {
      worker->stop();
    }
    for (auto &worker : mWorkers) {
      worker->join();
    }
  }

  void submit(const T &input) override {
    size_t workerIndex = mGetQueueIdFunc(input);
    Worker *worker = mWorkers[workerIndex].get();
    if (worker) {
      worker->submit(input);
    }
  }

 private:
  typename Worker::ProcessorPtr mProcessor;
  std::vector<WorkerPtr> mWorkers;
  std::atomic<uint64_t> mPointer;

  /// should be thread safe
  GetQueueIdFunc mGetQueueIdFunc;
};
}  // namespace goblin::kvengine::execution
#endif  // SERVER_SRC_KV_ENGINE_EXECUTION_EXECUTIONSERVICEIMPL_H_
