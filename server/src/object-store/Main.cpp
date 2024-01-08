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

#ifndef SPDLOG_ACTIVE_LEVEL
#ifdef DEBUG
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#endif
#endif

#include "ObjectStore.h"

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>

int main(int argc, char **argv) {
  /// global settings
  constexpr const char * pattern = "[%D %H:%M:%S.%F] [%s:%# %!] [%l] [thread %t] %v";
  spdlog::level::level_enum logLevel, flushLevel;
#ifdef DEBUG
  logLevel = spdlog::level::debug;
  flushLevel = spdlog::level::debug;
#else
  logLevel = spdlog::level::info;
  flushLevel = spdlog::level::info;
#endif
  /// create a rotating file logger with 1GB size max and 100 rotated files
  auto file_sink =
      std::make_shared<spdlog::sinks::rotating_file_sink_mt>(argv[1], 1024 * 1024 * 1024 * 1, 100);
  file_sink->set_pattern(pattern);
  file_sink->set_level(logLevel);
  std::list<spdlog::sink_ptr> sink_list = {file_sink};
  /// create other file logger with 1GB size max and 5 rotated files in second log path
  const char * secondLogPath = std::getenv("SECONDARY_LOG_PATH");
  if (secondLogPath != nullptr) {
    auto second_sink =
        std::make_shared<spdlog::sinks::rotating_file_sink_mt>(secondLogPath, 1024 * 1024 * 1024 * 1, 5);
    second_sink->set_pattern(pattern);
    second_sink->set_level(logLevel);
    sink_list.push_back(second_sink);
  }
  auto logger = std::make_shared<spdlog::logger>("goblin_logger", sink_list.begin(), sink_list.end());
  logger->flush_on(flushLevel);
  logger->set_level(logLevel);
  spdlog::set_default_logger(logger);

  SPDLOG_INFO("pid={}", getpid());
  assert(argc == 3);
  goblin::objectstore::ObjectStore(argv[2]).run();
}
