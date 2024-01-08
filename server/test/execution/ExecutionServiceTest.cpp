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
#include <atomic>
#include <unordered_map>
#include <thread>
#include <boost/thread/latch.hpp>

#include "../../src/kv-engine/execution/ExecutionServiceImpl.h"

namespace goblin::kvengine::execution {

class ExecutionServiceTest : public ::testing::Test {
};

TEST_F(ExecutionServiceTest, enqueueTest) {
  // 5 threads
  std::atomic_int sum = 0;
  int count = 1000;
  boost::latch latch(count);
  ExecutionServiceImpl<int> service{[&sum, &latch](const int &val) {
    sum += val;
    latch.count_down();
  }, "", 5};
  service.start();
  for (int i = 0; i < count; i++) {
    service.submit(i);
  }
  using namespace std::chrono_literals;  // NOLINT(build/namespaces)
  latch.wait();
  EXPECT_EQ(sum, (0 + count - 1) * count / 2);
  service.shutdown();
}

}  // namespace goblin::kvengine::execution
