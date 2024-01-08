/************************************************************************
Copyright 2020-2021 eBay Inc.
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

#ifndef SERVER_SRC_KV_ENGINE_UTILS_TIMEUTIL_H_
#define SERVER_SRC_KV_ENGINE_UTILS_TIMEUTIL_H_

#include <stdint.h>

namespace goblin::kvengine::utils {

using TimestampInNanos = uint64_t;
using TimestampInMills = uint64_t;
using ElapsedTimeInNanos = uint64_t;
/// second unit, keep seconds since epoch(1 Jan 1970)
using TimeType = uint32_t;

class TimeUtil final {
 public:
  /**
   * Return the current time since the Unix epoch in seconds.
   */
  static TimeType secondsSinceEpoch() {
    return ::time(0);
  }

  /**
   * Return the time since the Unix epoch in nanoseconds.
   */
  static TimestampInNanos currentTimeInNanos() {
    struct timespec now;
    int r = clock_gettime(CLOCK_REALTIME, &now);
    assert(r == 0);
    return uint64_t(now.tv_sec) * 1000 * 1000 * 1000 + uint64_t(now.tv_nsec);
  }

  /**
   * Convert timestamp from nanosecond to millisecond
   */
  static uint64_t nanosToMillis(uint64_t nanos) {
    return nanos / 1000000;
  }

  /**
   * Calculate elapse time in millisecond
   */
  static uint64_t elapseTimeInMillis(TimestampInNanos from, TimestampInNanos to) {
    return from < to ? nanosToMillis(to - from) : 0;
  }

  static std::string_view currentUTCTime() {
    time_t now;
    thread_local static char buf[32] = {0};
    struct tm utc_tm;
    time(&now);
    gmtime_r(&now, &utc_tm);
    snprintf(buf, sizeof(buf), "%.4d-%.2d-%.2dT%.2d:%.2d:%.2d.000Z", utc_tm.tm_year + 1900,
             utc_tm.tm_mon + 1, utc_tm.tm_mday, utc_tm.tm_hour, utc_tm.tm_min,
             utc_tm.tm_sec);
    return std::string_view(buf);
  }
};

}  /// namespace goblin::kvengine::utils

#endif  // SERVER_SRC_KV_ENGINE_UTILS_TIMEUTIL_H_
