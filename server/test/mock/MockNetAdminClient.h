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

#ifndef SERVER_TEST_MOCK_MOCKNETADMINCLIENT_H_
#define SERVER_TEST_MOCK_MOCKNETADMINCLIENT_H_

#include <memory>
#include <optional>
#include <thread>

#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <spdlog/spdlog.h>

#include <infra/raft/RaftInterface.h>
#include <infra/common_types.h>
#include "../../../../protocols/generated/netadmin.grpc.pb.h"
#include "../../src/kv-engine/utils/TimeUtil.h"

namespace goblin::mock {

using gringofts::app::protos::AppNetAdmin;
using gringofts::raft::MemberInfo;
using gringofts::app::protos::GetMemberOffsets_Request;
using gringofts::app::protos::GetMemberOffsets_Response;

enum class MockNetAdminClientCallType {
  Unknown = 0,
  GetMemberOffsets_Request = 1,
  GetMemberOffsets_Response = 2
};

struct AsyncNetAdminClientCallBase {
  virtual ~AsyncNetAdminClientCallBase() = default;

  virtual std::string toString() const = 0;
  virtual MockNetAdminClientCallType getType() const = 0;
  virtual void runCB() = 0;

  grpc::ClientContext mContext;
  grpc::Status mStatus;
};

template<typename ResponseType>
struct AsyncNetAdminClientCall : public AsyncNetAdminClientCallBase {
  std::string toString() const override { assert(0); }
  MockNetAdminClientCallType getType() const override { assert(0); }
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
std::string AsyncNetAdminClientCall<GetMemberOffsets_Response>::toString() const {
  return "get member offsets response";
}

using GetMemberOffsetsCallBack = std::function<void(const GetMemberOffsets_Response&)>;
using GetMemberOffsetsClientCall = AsyncNetAdminClientCall<GetMemberOffsets_Response>;

class MockNetAdminClient {
 public:
  explicit MockNetAdminClient(const std::shared_ptr<grpc::ChannelInterface> &channel);

  ~MockNetAdminClient();

  void getMemberOffsets(grpc::ClientContext *,
      const GetMemberOffsets_Request &request,
      GetMemberOffsets_Response *reply);
  void getMemberOffsetsAsync(const GetMemberOffsets_Request &request, GetMemberOffsetsCallBack cb);
  void getMemberOffsetsAsync(GetMemberOffsetsCallBack cb);

 private:
  void clientLoopMain();


  std::unique_ptr<AppNetAdmin::Stub> mStub;
  grpc::CompletionQueue mCompletionQueue;
  /// std::unique_ptr<AppNetAdmin::StubInterface> mStub;
  /// flag that notify resp receive thread to quit
  std::atomic<bool> mRunning = true;
  std::thread mClientLoop;
};

}  /// namespace goblin::mock

#endif  // SERVER_TEST_MOCK_MOCKNETADMINCLIENT_H_
