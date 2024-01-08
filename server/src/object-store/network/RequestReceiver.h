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

#ifndef SERVER_SRC_OBJECT_STORE_NETWORK_REQUESTRECEIVER_H_
#define SERVER_SRC_OBJECT_STORE_NETWORK_REQUESTRECEIVER_H_

#include <vector>
#include <string>

#include <INIReader.h>
#include <grpcpp/grpcpp.h>

#include "../../../../protocols/generated/service.grpc.pb.h"
#include "../../kv-engine/KVEngine.h"
#include "../../kv-engine/utils/TlsUtil.h"

namespace goblin {
namespace objectstore {
class ObjectStore;
}
}
namespace goblin::objectstore::network {

class RequestReceiver final {
 public:
  RequestReceiver(
      const INIReader &,
      kvengine::KVEngine &kvEngine,  // NOLINT(runtime/references)
      objectstore::ObjectStore &objectStore);  // NOLINT(runtime/references)
  ~RequestReceiver() = default;

  // disallow copy ctor and copy assignment
  RequestReceiver(const RequestReceiver &) = delete;
  RequestReceiver &operator=(const RequestReceiver &) = delete;

  // disallow move ctor and move assignment
  RequestReceiver(RequestReceiver &&) = delete;
  RequestReceiver &operator=(RequestReceiver &&) = delete;

  void run();

  void shutdown();

  void join();

 private:
  uint64_t mConcurrency;

  void handleRpcs(uint64_t i);

  kvengine::KVEngine &mKVEngine;
  objectstore::ObjectStore &mObjectStore;

  goblin::proto::KVStore::AsyncService mService;
  std::unique_ptr<grpc::Server> mServer;
  std::atomic<bool> mIsShutdown = false;

  std::vector<std::unique_ptr<grpc::ServerCompletionQueue>> mCompletionQueues;
  std::vector<std::thread> mRcvThreads;

  std::string mIpPort;
  std::optional<kvengine::utils::TlsConf> mTlsConfOpt;
};

}  /// namespace goblin::objectstore::network

#endif  // SERVER_SRC_OBJECT_STORE_NETWORK_REQUESTRECEIVER_H_
