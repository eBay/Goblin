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

#ifndef SERVER_SRC_OBJECT_STORE_NETWORK_OBJECTMANAGERCLIENT_H_
#define SERVER_SRC_OBJECT_STORE_NETWORK_OBJECTMANAGERCLIENT_H_

#include <vector>
#include <shared_mutex>
#include <string>

#include <INIReader.h>
#include <infra/util/DNSResolver.h>
#include <grpcpp/grpcpp.h>

#include "../../../../protocols/generated/control.grpc.pb.h"
#include "../../kv-engine/KVEngine.h"
#include "../../kv-engine/utils/TlsUtil.h"
#include "AsyncClientCall.h"

namespace goblin::objectstore::network {

using AddClusterCallBack = std::function<void(const proto::AddCluster::Response &)>;
using RouterCallBack = std::function<void(const proto::Router::Response &)>;
using ReportStartupCallBack = std::function<void(const proto::OnStartup::Response &)>;
using ReportMigrationCallBack = std::function<void(const proto::OnMigration::Response &)>;

class ObjectManagerClient {
 public:
  explicit ObjectManagerClient(const INIReader &reader);
  explicit ObjectManagerClient(const char* config);
  explicit ObjectManagerClient(const std::string &config);
  ~ObjectManagerClient();

  void addCluster(
      const proto::AddCluster::Request &request,
      AddClusterCallBack cb);
  void requestRouteInfo(
      const proto::Router::Request &request,
      RouterCallBack cb);
  void reportStartup(
      const proto::OnStartup::Request &request,
      ReportStartupCallBack cb);
  void reportMigrationProgress(
      const proto::OnMigration::Request &request,
      ReportMigrationCallBack cb);

 private:
  void refressChannel(uint64_t addrIndex);
  /// thread function of mClientLoop.
  void clientLoopMain();
  bool needRetry(grpc::StatusCode grpcCode, proto::ResponseCode code);

  uint32_t mCurRetry = 0;
  uint32_t mCurStubIndex = 0;
  std::vector<std::string> mManagerAddresses;
  std::vector<std::string> mResolvedManagerAddresses;
  std::optional<kvengine::utils::TlsConf> mTlsConf;
  std::shared_ptr<gringofts::DNSResolver> mDNSResolver;
  std::vector<std::unique_ptr<proto::KVManager::Stub>> mStubs;
  std::shared_mutex mMutex;  /// the lock to guarantee thread-safe access of mStub
  grpc::CompletionQueue mCompletionQueue;

  /// flag that notify resp receive thread to quit
  std::atomic<bool> mRunning = true;
  std::thread mClientLoop;
};

}  /// namespace goblin::objectstore::network

#endif  // SERVER_SRC_OBJECT_STORE_NETWORK_OBJECTMANAGERCLIENT_H_
