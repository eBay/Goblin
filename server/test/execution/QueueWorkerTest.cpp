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

#include <future>
#include <boost/thread/latch.hpp>
#include <gtest/gtest-spi.h>
#include "../../src/kv-engine/execution/QueueWorker.h"

namespace goblin::kvengine::execution {

class QueueWorkerTest : public ::testing::Test {
};

TEST_F(QueueWorkerTest, processTest) {
  int cnt = 0;
  boost::latch latch(2);
  QueueWorker<int> worker([&cnt, &latch](const int &input) {
      cnt += input;
      std::cout << cnt << std::endl;
      latch.count_down();
      }, "");
  worker.start();
  worker.submit(1);
  worker.submit(2);
  latch.wait();
  worker.waitForStop();
  EXPECT_EQ(cnt, 3);
}
}  // namespace goblin::kvengine::execution
