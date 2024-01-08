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

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>

#include "ObjectManager.h"

int main(int argc, char **argv) {
  spdlog::stdout_logger_mt("console");
  spdlog::set_pattern("[%D %H:%M:%S.%F] [%s:%# %!] [%l] [thread %t] %v");
  /// create a rotating file logger with 1GB size max and 100 rotated files
  auto logger = spdlog::rotating_logger_mt("app_logger", argv[1], 1024 * 1024 * 1024 * 1, 100);
  spdlog::set_default_logger(logger);
  spdlog::flush_on(spdlog::level::info);

  SPDLOG_INFO("pid={}", getpid());
  assert(argc == 3);
  goblin::objectmanager::ObjectManager(argv[2]).run();
  return 0;
}
