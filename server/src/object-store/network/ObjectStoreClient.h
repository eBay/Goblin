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

#ifndef SERVER_SRC_OBJECT_STORE_NETWORK_OBJECTSTORECLIENT_H_
#define SERVER_SRC_OBJECT_STORE_NETWORK_OBJECTSTORECLIENT_H_

#include <vector>
#include <string>

#include <INIReader.h>
#include <grpcpp/grpcpp.h>

#include "../../../../protocols/generated/service.grpc.pb.h"
#include "../../kv-engine/KVEngine.h"
#include "../../kv-engine/utils/TlsUtil.h"
#include "AsyncClientCall.h"

namespace goblin::objectstore::network {

using MigrateBatchCallBack = std::function<void(const proto::MigrateBatch::Response &)>;

class ObjectStoreClient {
 public:
  ObjectStoreClient(const INIReader &reader, const proto::Cluster::ClusterAddress &addrs);
  ~ObjectStoreClient();

  void migrateBatch(
      const proto::MigrateBatch::Request &request,
      MigrateBatchCallBack cb);

 private:
  /// thread function of mClientLoop.
  void clientLoopMain();
  bool needRetry(grpc::StatusCode grpcCode, proto::ResponseCode code);

  static constexpr uint32_t kMaxRetryTimes = 10;
  uint32_t mCurRetry = 0;
  uint32_t mCurStubIndex = 0;
  std::vector<std::unique_ptr<proto::KVStore::Stub>> mStubs;
  grpc::CompletionQueue mCompletionQueue;

  /// flag that notify resp receive thread to quit
  std::atomic<bool> mRunning = true;
  std::thread mClientLoop;
};

}  /// namespace goblin::objectstore::network

#endif  // SERVER_SRC_OBJECT_STORE_NETWORK_OBJECTSTORECLIENT_H_
