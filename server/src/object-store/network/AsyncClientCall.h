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

#ifndef SERVER_SRC_OBJECT_STORE_NETWORK_ASYNCCLIENTCALL_H_
#define SERVER_SRC_OBJECT_STORE_NETWORK_ASYNCCLIENTCALL_H_

#include <vector>
#include <string>

#include <INIReader.h>
#include <grpcpp/grpcpp.h>

#include "../../../../protocols/generated/control.grpc.pb.h"
#include "../../kv-engine/KVEngine.h"
#include "../../kv-engine/utils/TlsUtil.h"

namespace goblin::objectstore::network {

enum AsyncRequestType {
  ROUTER = 1,
  REPORT_STARTUP = 2,
  REPORT_MIGRATION = 3,
  MIGRATE_BATCH = 4,
  ADD_CLUSTER = 5
};

struct AsyncClientCallBase {
  virtual ~AsyncClientCallBase() = default;

  virtual std::string toString() const = 0;
  virtual AsyncRequestType getType() const = 0;

  grpc::ClientContext mContext;
  grpc::Status mStatus;
};

template<typename RequestType, typename ResponseType>
struct AsyncClientCall : public AsyncClientCallBase {
  AsyncClientCall(AsyncRequestType type, const RequestType &req): mType(type), mRequest(req) {}
  std::string toString() const override {
    return "async client call type" + std::to_string(mType) +
      ", response code " + std::to_string(mResponse.header().code());
  }
  AsyncRequestType getType() const override {
    return mType;
  }

  AsyncRequestType mType;
  RequestType mRequest;
  ResponseType mResponse;
  std::function<void(const ResponseType &)> mCB;
  std::unique_ptr<grpc::ClientAsyncResponseReader<ResponseType>> mResponseReader;
};
}  /// namespace goblin::objectstore::network

#endif  // SERVER_SRC_OBJECT_STORE_NETWORK_ASYNCCLIENTCALL_H_
