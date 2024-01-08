/************************************************************************
Copyright 2019-2020 eBay Inc.
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

#ifndef SERVER_SRC_KV_ENGINE_UTILS_TESSDNSRESOLVER_H_
#define SERVER_SRC_KV_ENGINE_UTILS_TESSDNSRESOLVER_H_

#include <absl/strings/match.h>
#include <absl/strings/str_split.h>
#include <arpa/inet.h>
#include <infra/util/DNSResolver.h>
#include <netinet/in.h>
#include <netdb.h>
#include <spdlog/spdlog.h>

namespace goblin::kvengine::utils {

class TessDNSResolver final : public gringofts::DNSResolver {
 public:
  TessDNSResolver() = default;
  ~TessDNSResolver() override = default;

  std::string resolve(const std::string &hostname) override {
    std::vector<std::string> subStrs = absl::StrSplit(hostname, ':');
    const std::string ipStr(getIpByHostname(subStrs.at(0)));
    return ipStr + ":" + subStrs[1];
  }

  static std::string getIpByHostname(const std::string &hostname) {
    std::lock_guard<std::mutex> lock(mLock);
    const auto *hostInfo = gethostbyname(hostname.c_str());
    if (hostInfo == nullptr) {
      SPDLOG_INFO("failed to parse domain name {} after retry", hostname);
      return hostname;
    }
    assert(hostInfo != nullptr);
    const auto *ipStr = inet_ntoa(*((struct in_addr *)hostInfo->h_addr));
    SPDLOG_INFO("Successfully parse domain name from {} to {}", hostInfo->h_name, ipStr);
    return ipStr;
  }

  static std::mutex mLock;
};

}  /// namespace goblin::kvengine::utils

#endif  // SERVER_SRC_KV_ENGINE_UTILS_TESSDNSRESOLVER_H_
