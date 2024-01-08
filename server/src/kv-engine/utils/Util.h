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

#ifndef SERVER_SRC_KV_ENGINE_UTILS_UTIL_H_
#define SERVER_SRC_KV_ENGINE_UTILS_UTIL_H_

#include <assert.h>
#include <boost/filesystem.hpp>
#include <netdb.h>
#include <string>
#include <unistd.h>

#include <spdlog/spdlog.h>

namespace goblin::kvengine::utils {

/// copy the macro definition from "gtest/gtest_prod.h"
/// since we don't want to include dependency from gtest.
#define FRIEND_TEST(test_case_name, test_name)\
friend class test_case_name##_##test_name##_Test

/**
 * @tparam a template to create singleton easily
 */
template<class T>
class Singleton {
 public:
  using Type = T;

  Singleton(const Singleton &) = delete;
  Singleton &operator=(const Singleton &) = delete;

  /**
   * getInstance of the singleton, if the first time call, arguments will be used to
   * construct the object
   * @tparam ArgT
   * @param args
   * @return reference of the object
   */
  template<class... Arg>
  static Type &getInstance(Arg &&... args) {
    static Type instance(std::forward<Arg>(args)...);
    return instance;
  }
 private:
  Singleton() = default;
};

/**
 * A class to hold all common util functions.
 */
class Util final {
 public:
  /// get official name of host
  static std::string getHostname() {
    constexpr uint64_t kBufferSize = 1024;
    char buffer[kBufferSize];

    auto ret = gethostname(buffer, sizeof buffer);
    assert(ret == 0);

    /// make sure it is null-terminated
    buffer[kBufferSize - 1] = '\0';

    /// TODO: gethostbyname is obsolete, and not thread-safe
    /// need replace it
    struct hostent *h = gethostbyname(buffer);
    assert(h != nullptr);

    SPDLOG_INFO("hostname is: {}", h->h_name);
    return h->h_name;
  }

  /// execute a shell cmd and return the result
  /// ATTENTION: should only call this method within unit test.
  ///            call popen() and fork() in production code is dangerous.
  static std::string executeCmd(const std::string &cmd) noexcept {
    std::string data;

    FILE *stream;
    const int kMaxBufferSize = 256;
    char buffer[kMaxBufferSize];

    stream = popen(cmd.c_str(), "r");
    if (stream) {
      while (!feof(stream)) {
        if (fgets(buffer, kMaxBufferSize, stream) != nullptr)
          data.append(buffer);
      }
      pclose(stream);
    } else {
      SPDLOG_WARN("Unable to execute cmd: {} due to error code: {}", cmd, errno);
    }

    SPDLOG_INFO("Execute command '{}', Output '{}'", cmd, data);
    return data;
  }

  static std::string getCurrentVersion() {
    return "1.0.0";
  }

  static double getCertBeginDate() {
    return getDateTimeStamp("conf/notBefore.date");
  }

  static double getCertEndDate() {
    return getDateTimeStamp("conf/notAfter.date");
  }

 private:
  static double getDateTimeStamp(const std::string &fileName) {
    std::ifstream fd(fileName);
    if (!fd) {
      SPDLOG_INFO("failed to open file {}", fileName);
      return 0;
    }

    auto date = std::string(std::istreambuf_iterator<char>(fd), std::istreambuf_iterator<char>());
    std::string::size_type sz;
    return std::stod(date, &sz);
  }
};

}  /// namespace goblin::kvengine::utils

#endif  // SERVER_SRC_KV_ENGINE_UTILS_UTIL_H_
