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

#ifndef SERVER_SRC_OBJECT_STORE_NETWORK_REQUESTCALLDATA_H_
#define SERVER_SRC_OBJECT_STORE_NETWORK_REQUESTCALLDATA_H_

#include <string>

#include <grpcpp/grpcpp.h>

#include "../../../../protocols/generated/service.grpc.pb.h"
#include "../../kv-engine/KVEngine.h"

namespace goblin {
namespace objectstore {
class ObjectStore;
}
}

namespace goblin::objectstore::network {

class RequestCallData {
 public:
  // Take in the "service" instance (in this case representing an asynchronous
  // server) and the completion queue "cq" used for asynchronous communication
  // with the gRPC runtime.
  RequestCallData(goblin::proto::KVStore::AsyncService *service,
                  grpc::ServerCompletionQueue *cq,
                  kvengine::KVEngine &kvEngine,            // NOLINT(runtime/references)
                  objectstore::ObjectStore &objectStore);  // NOLINT(runtime/references)

  virtual ~RequestCallData() = default;

  virtual void proceed() = 0;
  virtual void failOver() {
    delete this;
  }

 protected:
  // The means of communication with the gRPC runtime for an asynchronous
  // server.
  goblin::proto::KVStore::AsyncService *mService;
  // The producer-consumer queue where for asynchronous server notifications.
  grpc::ServerCompletionQueue *mCompletionQueue;
  // Context for the rpc, allowing to tweak aspects of it such as the use
  // of compression, authentication, as well as to send metadata back to the
  // client.
  grpc::ServerContext mContext;

  // Let's implement a tiny state machine with the following states.
  enum CallStatus { CREATE, PROCESS, FINISH };
  CallStatus mStatus;  // The current serving state.

  kvengine::KVEngine &mKVEngine;
  objectstore::ObjectStore &mObjectStore;
};

}  /// namespace goblin::objectstore::network

#endif  // SERVER_SRC_OBJECT_STORE_NETWORK_REQUESTCALLDATA_H_
