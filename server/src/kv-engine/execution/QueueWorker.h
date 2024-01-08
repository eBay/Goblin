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

#ifndef SERVER_SRC_KV_ENGINE_EXECUTION_QUEUEWORKER_H_
#define SERVER_SRC_KV_ENGINE_EXECUTION_QUEUEWORKER_H_
#include <infra/common_types.h>
#include <functional>
#include <infra/monitor/MonitorCenter.h>
#include <infra/monitor/MonitorTypes.h>
#include <infra/mpscqueue/MpscDoubleBufferQueue.h>
#include "Loop.h"

namespace goblin::kvengine::execution {

template<typename T>
class Processor {
 public:
  virtual void process(const T &) = 0;
  virtual ~Processor() = default;
};

template<typename T>
class FuncProcessor : public Processor<T> {
 public:
  typedef std::function<void(const T &)> Func;
  explicit FuncProcessor(Func func) : mFunc(func) {}
  virtual ~FuncProcessor() = default;
  void process(const T &input) override {
    mFunc(input);
  }
 private:
  Func mFunc;
};

template<typename T>
class QueueWorker :  public ThreadLoop {
 public:
  typedef gringofts::MpscDoubleBufferQueue<T> Queue;
  typedef std::shared_ptr<Processor<T>> ProcessorPtr;

  virtual ~QueueWorker() = default;
  explicit QueueWorker(ProcessorPtr processor, const std::string &name, int workerId = 0) :
      ThreadLoop(std::string(name + "_work_" + std::to_string(workerId))),
      mName(name),
      mWorkerId(workerId),
      mProcessor(processor),
      mTaskCountGauge(gringofts::getGauge(mName + "_queue_size", {{"worker", std::to_string(mWorkerId)}})) {}
  explicit QueueWorker(typename FuncProcessor<T>::Func func, const std::string &name, int workerId = 0) :
      mName(name),
      mWorkerId(workerId),
      mProcessor(std::make_shared<FuncProcessor<T>>(func)),
      mTaskCountGauge(gringofts::getGauge(mName + "_queue_size", {{"worker", std::to_string(mWorkerId)}})) {}
  bool loop() override {
    mTaskCountGauge.set(mTaskCount.load());
    try {
      auto input = mQueue.dequeue();
      mProcessor->process(input);
      mTaskCount -= 1;
    } catch (const gringofts::QueueStoppedException &ex) {
      return false;
    }
    return true;
  }
  void stop() override {
    mQueue.shutdown();
    ThreadLoop::stop();
  };
  void submit(const T &task) {
    mTaskCount += 1;
    mQueue.enqueue(task);
  }

 private:
  const std::string mName;
  const int32_t mWorkerId;
  Queue mQueue;
  std::atomic<uint32_t> mTaskCount = 0;
  ProcessorPtr mProcessor;
  santiago::MetricsCenter::GaugeType mTaskCountGauge;
};
}  // namespace goblin::kvengine::execution
#endif  // SERVER_SRC_KV_ENGINE_EXECUTION_QUEUEWORKER_H_
