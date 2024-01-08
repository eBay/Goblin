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

#include "ObjectStoreClient.h"

#include <spdlog/spdlog.h>
#include <grpcpp/server_builder.h>

#include <utility>

#include <infra/common_types.h>

namespace goblin::objectstore::network {

ObjectStoreClient::ObjectStoreClient(const INIReader &reader, const proto::Cluster::ClusterAddress &addrs) {
  auto tlsConfOpt = kvengine::utils::TlsUtil::parseTlsConf(reader, "tls");
  for (auto s : addrs.servers()) {
    auto addr = s.hostname();
    SPDLOG_INFO("creating client channel to cluster {}", addr);
    auto channel = grpc::CreateChannel(addr, kvengine::utils::TlsUtil::buildChannelCredentials(tlsConfOpt));
    mStubs.push_back(proto::KVStore::NewStub(channel));
  }
  /// start AE_resp/RV_resp receiving thread
  mClientLoop = std::thread(&ObjectStoreClient::clientLoopMain, this);
}

ObjectStoreClient::~ObjectStoreClient() {
  mRunning = false;

  /// shut down CQ
  mCompletionQueue.Shutdown();

  /// join event loop.
  if (mClientLoop.joinable()) {
    mClientLoop.join();
  }

  /// drain completion queue.
  void *tag;
  bool ok;
  while (mCompletionQueue.Next(&tag, &ok)) { ; }
}

void ObjectStoreClient::migrateBatch(
    const proto::MigrateBatch::Request &request,
    MigrateBatchCallBack cb) {
  auto *call = new AsyncClientCall<proto::MigrateBatch::Request, proto::MigrateBatch::Response>(
      AsyncRequestType::MIGRATE_BATCH, request);
  call->mCB = cb;
  std::chrono::time_point deadline = std::chrono::system_clock::now()
      + std::chrono::milliseconds(3000);
  call->mContext.set_deadline(deadline);
  call->mResponseReader = mStubs[mCurStubIndex]->PrepareAsyncMigrateBatch(&call->mContext, request, &mCompletionQueue);
  call->mResponseReader->StartCall();
  call->mResponseReader->Finish(&call->mResponse,
                                &call->mStatus,
                                reinterpret_cast<void *>(call));
}

void ObjectStoreClient::clientLoopMain() {
  auto peerThreadName = "ObjectStoreClient";
  pthread_setname_np(pthread_self(), peerThreadName);

  void *tag;  /// The tag is the memory location of the call object
  bool ok = false;

  /// Block until the next result is available in the completion queue.
  while (mCompletionQueue.Next(&tag, &ok)) {
    if (!mRunning) {
      SPDLOG_INFO("Client loop quit.");
      return;
    }

    auto *call = static_cast<AsyncClientCallBase*>(tag);
    GPR_ASSERT(ok);

    if (!call->mStatus.ok()) {
      SPDLOG_WARN("{} failed., gRpc error_code: {}, error_message: {}, error_details: {}",
                  call->toString(),
                  call->mStatus.error_code(),
                  call->mStatus.error_message(),
                  call->mStatus.error_details());
    }
    if (call->getType() == AsyncRequestType::MIGRATE_BATCH) {
      auto migrateBatchCall =
        dynamic_cast<AsyncClientCall<proto::MigrateBatch::Request, proto::MigrateBatch::Response>*>(call);
      if (needRetry(call->mStatus.error_code(), migrateBatchCall->mResponse.header().code())) {
        SPDLOG_ERROR("got error when migrate batch: {}", migrateBatchCall->mResponse.header().code());
        auto req = migrateBatchCall->mRequest;
        auto cb = migrateBatchCall->mCB;
        delete migrateBatchCall;
        migrateBatch(req, cb);
      } else {
        migrateBatchCall->mCB(migrateBatchCall->mResponse);
        delete migrateBatchCall;
      }
    } else {
      SPDLOG_ERROR("invalid async call");
    }
  }
}

bool ObjectStoreClient::needRetry(grpc::StatusCode grpcCode, proto::ResponseCode code) {
  if (grpcCode == grpc::StatusCode::UNAVAILABLE || code == proto::ResponseCode::NOT_LEADER) {
    mCurRetry++;
    mCurStubIndex = mCurRetry % mStubs.size();
    return true;
  } else {
    return false;
  }
}

}  /// namespace goblin::objectstore::network
