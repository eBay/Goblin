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

#ifndef SERVER_SRC_KV_ENGINE_UTILS_APPUTIL_H_
#define SERVER_SRC_KV_ENGINE_UTILS_APPUTIL_H_

#include <assert.h>
#include <boost/filesystem.hpp>
#include <netdb.h>
#include <string>
#include <unistd.h>

#include <spdlog/spdlog.h>

#include "StrUtil.h"
#include "ClusterInfoUtil.h"
#include "FileUtil.h"

namespace goblin::kvengine::utils {

class AppInfo final {
 public:
  ~AppInfo() = default;

  explicit AppInfo(const INIReader &reader);

  /// disallow copy ctor and copy assignment
  AppInfo(const AppInfo &) = delete;
  AppInfo &operator=(const AppInfo &) = delete;

  inline std::string appVersion() const {
    return mAppVersion;
  }

  gringofts::ClusterInfo::Node getMyNode() {
    return getClusterInfo().getAllNodeInfo()[getMyNodeId()];
  }

  gringofts::ClusterInfo getClusterInfo() {
    return mMyClusterInfo;
  }
  inline gringofts::NodeId getMyNodeId() {
    return mMyNodeId;
  }
  inline std::string getMyNodeAddress() {
    return absl::StrCat(getMyNode().mHostName, ":", getMyNode().mPortForRaft);
  }

 private:
  /**
   * Cluster Info
   */
  gringofts::ClusterInfo mMyClusterInfo;
  gringofts::NodeId mMyNodeId;
  std::string mAppVersion;
};

class AppUtil final {
 public:
  /**
   * get current version according to the current work directory
   * @return release version
   */
  static std::string getCurrentVersion() {
    std::string unknownVersion("UNKNOWN");
    std::string version;
    try {
      version = FileUtil::getFileContent("conf/app.version");
    } catch (...) {
      SPDLOG_INFO("failed to open file conf/app.version");
      return unknownVersion;
    }
    return version;
  }

  static std::string getReleaseVersion(const std::string &cwd) {
    const char *suffix = "unx";
    const char *unknownVersion = "UNKNOWN";
    auto tokens = StrUtil::tokenize(cwd, '/');
    if (tokens.size() < 2) {
      return unknownVersion;
    }
    auto &version = tokens[tokens.size() - 2];
    return StrUtil::endsWith(version, suffix) ? version : unknownVersion;
  }
};

}  /// namespace goblin::kvengine::utils

#endif  // SERVER_SRC_KV_ENGINE_UTILS_APPUTIL_H_
