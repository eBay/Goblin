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

#include <gtest/gtest-spi.h>
#include <functional>
#include "../../src/kv-engine/execution/Loop.h"

namespace goblin::kvengine::execution {

class FunctorLoop : public ThreadLoop {
 public:
  explicit FunctorLoop(std::function<bool()> functor) : mFunctor(functor) {}
  bool loop() override {
    return mFunctor();
  }

 private:
  std::function<bool()> mFunctor;
};

class LoopTest : public ::testing::Test {
};

TEST_F(LoopTest, StartLoopTest) {
  std::atomic_int cnt = 0;
  FunctorLoop loop{
    [&cnt]() {
      cnt++;
      if (cnt >= 10) {
        return false;
      } else {
        return true;
      }
    }
  };
  loop.start();
  loop.join();
  ASSERT_EQ(cnt, 10);
}

using namespace std::chrono_literals;  // NOLINT(build/namespaces)

TEST_F(LoopTest, StopLoopTest) {
  std::atomic_int cnt = 0;
  FunctorLoop loop{[&cnt]() {
    std::cout << "running" << std::endl;
    std::this_thread::sleep_for(100ms);
    return true;
  }};
  loop.start();
  std::this_thread::sleep_for(1000ms);
  loop.waitForStop();
}

}  // namespace goblin::kvengine::execution
