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

#ifndef SERVER_TEST_MOCK_MOCKAPPCLIENT_H_
#define SERVER_TEST_MOCK_MOCKAPPCLIENT_H_

#include <memory>
#include <optional>
#include <thread>

#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <spdlog/spdlog.h>

#include <infra/mpscqueue/MpscDoubleBufferQueue.h>

#include "../../src/kv-engine/utils/TimeUtil.h"
#include "../../../protocols/generated/service.grpc.pb.h"
#include "../../src/kv-engine/store/VersionStore.h"

namespace goblin {
namespace mock {

enum class MockAppClientCallType {
  Unknown = 0,
  ConnectRequest = 1,
  ConnectResponse = 2,
  PutRequest = 3,
  PutResponse = 4,
  GetRequest = 5,
  GetResponse = 6,
  DeleteRequest = 7,
  DeleteResponse = 8,
  TransRequest = 9,
  TransResponse = 10
};

struct AsyncClientCallBase {
  virtual ~AsyncClientCallBase() = default;

  virtual std::string toString() const = 0;
  virtual MockAppClientCallType getType() const = 0;
  virtual void runCB() = 0;

  grpc::ClientContext mContext;
  grpc::Status mStatus;
};

template<typename ResponseType>
struct AsyncClientCall : public AsyncClientCallBase {
  std::string toString() const override { assert(0); }
  MockAppClientCallType getType() const override { assert(0); }
  void runCB() override {
    mCB(mResponse);
  }

  using ResponseCallBack = std::function<void(const ResponseType &)>;
  ResponseType mResponse;
  std::unique_ptr<grpc::ClientAsyncResponseReader<ResponseType>> mResponseReader;
  ResponseCallBack mCB;
};

template<>
inline
std::string AsyncClientCall<proto::Connect::Response>::toString() const {
  return "connect response";
}

template<>
inline
MockAppClientCallType AsyncClientCall<proto::Connect::Response>::getType() const {
  return MockAppClientCallType::ConnectResponse;
}

template<>
inline
std::string AsyncClientCall<proto::Put::Response>::toString() const {
  return "put response";
}

template<>
inline
MockAppClientCallType AsyncClientCall<proto::Put::Response>::getType() const {
  return MockAppClientCallType::PutResponse;
}

template<>
inline
std::string AsyncClientCall<proto::Get::Response>::toString() const {
  return "get response";
}

template<>
inline
MockAppClientCallType AsyncClientCall<proto::Get::Response>::getType() const {
  return MockAppClientCallType::GetResponse;
}

template<>
inline
std::string AsyncClientCall<proto::Delete::Response>::toString() const {
  return "delete response";
}

template<>
inline
MockAppClientCallType AsyncClientCall<proto::Delete::Response>::getType() const {
  return MockAppClientCallType::DeleteResponse;
}

template<>
inline
std::string AsyncClientCall<proto::Transaction::Response>::toString() const {
  return "trans response";
}

template<>
inline
MockAppClientCallType AsyncClientCall<proto::Transaction::Response>::getType() const {
  return MockAppClientCallType::TransResponse;
}

using ConnectClientCall = AsyncClientCall<proto::Connect::Response>;
using PutClientCall = AsyncClientCall<proto::Put::Response>;
using GetClientCall = AsyncClientCall<proto::Get::Response>;
using DeleteClientCall = AsyncClientCall<proto::Delete::Response>;
using TransClientCall = AsyncClientCall<proto::Transaction::Response>;
using ConnectResponseCallBack = std::function<void(const proto::Connect::Response&)>;
using PutResponseCallBack = std::function<void(const proto::Put::Response&)>;
using GetResponseCallBack = std::function<void(const proto::Get::Response&)>;
using DeleteResponseCallBack = std::function<void(const proto::Delete::Response&)>;
using TransResponseCallBack = std::function<void(const proto::Transaction::Response&)>;

class MockAppClient {
 public:
  MockAppClient(const std::shared_ptr<grpc::ChannelInterface> &channel, uint64_t routeVersion);
  ~MockAppClient();

  void connectAsync(ConnectResponseCallBack cb);
  void putAsync(const kvengine::store::KeyType &k, const kvengine::store::ValueType &v, PutResponseCallBack cb);
  void putTTLAsync(const kvengine::store::KeyType &k,
      const kvengine::store::ValueType &v,
      const kvengine::store::TTLType &ttl,
      PutResponseCallBack cb);
  void putUdfMetaAsync(
      const kvengine::store::KeyType &k,
      const kvengine::store::ValueType &v,
      const goblin::proto::UserDefinedMeta &udfMeta,
      PutResponseCallBack cb);
  void putAsync(const proto::Put::Request &request, PutResponseCallBack cb);
  void getAsync(const kvengine::store::KeyType &k, GetResponseCallBack cb);
  void getAsync(const kvengine::store::KeyType &k, bool isMetaReturned, GetResponseCallBack cb);
  void getFollowerAsync(const kvengine::store::KeyType &k, GetResponseCallBack cb);
  void getAsync(const proto::Get::Request &request, GetResponseCallBack cb);
  void deleteAsync(const kvengine::store::KeyType &k, DeleteResponseCallBack cb);
  void deleteAsync(const proto::Delete::Request &request, DeleteResponseCallBack cb);
  void transAsync(const proto::Transaction::Request &request, TransResponseCallBack cb);

 private:
  /// thread function of mClientLoop.
  void clientLoopMain();

  kvengine::store::VersionType mKnownVersion = kvengine::store::VersionStore::kInvalidVersion;
  uint64_t mRouteVersion = 0;

  std::unique_ptr<proto::KVStore::Stub> mStub;
  grpc::CompletionQueue mCompletionQueue;

  /// flag that notify resp receive thread to quit
  std::atomic<bool> mRunning = true;
  std::thread mClientLoop;
};

}  /// namespace mock
}  /// namespace goblin

#endif  // SERVER_TEST_MOCK_MOCKAPPCLIENT_H_
