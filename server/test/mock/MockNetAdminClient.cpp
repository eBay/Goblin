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

#include "MockNetAdminClient.h"

namespace goblin::mock {
MockNetAdminClient::MockNetAdminClient(const std::shared_ptr<grpc::ChannelInterface> &channel)
    : mStub(AppNetAdmin::NewStub(channel)) {
  /// start AE_resp/RV_resp receiving thread
  mClientLoop = std::thread(&MockNetAdminClient::clientLoopMain, this);
}

MockNetAdminClient::~MockNetAdminClient() {
  mRunning = false;

  /// shut down CQ
  mCompletionQueue.Shutdown();

  /// join event loop
  if (mClientLoop.joinable()) {
    mClientLoop.join();
  }

  /// drain completion queue.
  void *tag;
  bool ok;
  while (mCompletionQueue.Next(&tag, &ok)) { ; }
}

void MockNetAdminClient::getMemberOffsets(
    grpc::ClientContext *mContext,
    const GetMemberOffsets_Request &request,
    GetMemberOffsets_Response *reply) {

  auto status = mStub->GetMemberOffsets(mContext, request, reply);
  if (!status.ok()) {
    SPDLOG_WARN("getMemberOffsets failed., gRpc error_code: {}, error_message: {}, error_details: {}",
        status.error_code(),
        status.error_message(),
        status.error_details());
  }
}

void MockNetAdminClient::getMemberOffsetsAsync(GetMemberOffsetsCallBack cb) {
  GetMemberOffsets_Request req;
  return getMemberOffsetsAsync(req, cb);
}

void MockNetAdminClient::getMemberOffsetsAsync(
    const GetMemberOffsets_Request &request,
    GetMemberOffsetsCallBack cb) {
  auto *call = new GetMemberOffsetsClientCall;
  std::chrono::time_point deadline = std::chrono::system_clock::now()
      + std::chrono::milliseconds(10000);
  call->mContext.set_deadline(deadline);
  call->mCB = cb;
  call->mResponseReader = mStub->PrepareAsyncGetMemberOffsets(&call->mContext, request, &mCompletionQueue);
  call->mResponseReader->StartCall();
  call->mResponseReader->Finish(&call->mResponse,
                                &call->mStatus,
                                reinterpret_cast<void *>(call));
}

void MockNetAdminClient::clientLoopMain() {
  auto peerThreadName = std::string("MockNetAdminClient");
  pthread_setname_np(pthread_self(), peerThreadName.c_str());

  void *tag;  /// The tag is the memory location of the call object
  bool ok = false;

  /// Block until the next result is available in the completion queue.
  while (mCompletionQueue.Next(&tag, &ok)) {
    if (!mRunning) {
      SPDLOG_INFO("Client loop quit.");
      return;
    }

    auto *call = static_cast<AsyncNetAdminClientCallBase *>(tag);
    GPR_ASSERT(ok);
    if (!call->mStatus.ok()) {
      SPDLOG_WARN("{} failed., gRpc error_code: {}, error_message: {}, error_details: {}",
                   call->toString(),
                   call->mStatus.error_code(),
                   call->mStatus.error_message(),
                   call->mStatus.error_details());
    }
    call->runCB();
  }
}

}  /// namespace goblin::mock
