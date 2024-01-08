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

#include <infra/common_types.h>
#include "MockAppClient.h"

namespace goblin {
namespace mock {

MockAppClient::MockAppClient(const std::shared_ptr<grpc::ChannelInterface> &channel, uint64_t routeVersion)
    : mRouteVersion(routeVersion), mStub(proto::KVStore::NewStub(channel)) {
  /// start AE_resp/RV_resp receiving thread
  mClientLoop = std::thread(&MockAppClient::clientLoopMain, this);
}

MockAppClient::~MockAppClient() {
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

void MockAppClient::connectAsync(ConnectResponseCallBack cb) {
  auto *call = new ConnectClientCall;

  std::chrono::time_point deadline = std::chrono::system_clock::now()
      + std::chrono::milliseconds(10000);
  call->mContext.set_deadline(deadline);

  proto::Connect::Request request;
  request.mutable_header()->set_routeversion(mRouteVersion);
  call->mCB = [this, cb](const proto::Connect::Response &resp) {
    mKnownVersion = resp.header().latestversion();
    cb(resp);
  };
  call->mResponseReader = mStub->PrepareAsyncConnect(&call->mContext, request, &mCompletionQueue);
  call->mResponseReader->StartCall();
  call->mResponseReader->Finish(&call->mResponse,
                                &call->mStatus,
                                reinterpret_cast<void *>(call));
}

void MockAppClient::putAsync(
    const kvengine::store::KeyType &k,
    const kvengine::store::ValueType &v,
    PutResponseCallBack cb) {
  proto::Put::Request req;
  req.mutable_header()->set_knownversion(mKnownVersion);
  req.mutable_header()->set_routeversion(mRouteVersion);
  req.mutable_entry()->set_key(k);
  req.mutable_entry()->set_value(v);
  return putAsync(req, cb);
}

void MockAppClient::putTTLAsync(
    const kvengine::store::KeyType &k,
    const kvengine::store::ValueType &v,
    const kvengine::store::TTLType &ttl,
    PutResponseCallBack cb) {
  proto::Put::Request req;
  req.mutable_header()->set_knownversion(mKnownVersion);
  req.mutable_header()->set_routeversion(mRouteVersion);
  req.mutable_entry()->set_enablettl(true);
  req.mutable_entry()->set_ttl(ttl);
  req.mutable_entry()->set_key(k);
  req.mutable_entry()->set_value(v);
  return putAsync(req, cb);
}

void MockAppClient::putUdfMetaAsync(
    const kvengine::store::KeyType &k,
    const kvengine::store::ValueType &v,
    const goblin::proto::UserDefinedMeta &udfMeta,
    PutResponseCallBack cb) {
  proto::Put::Request req;
  req.mutable_header()->set_knownversion(mKnownVersion);
  req.mutable_header()->set_routeversion(mRouteVersion);
  req.mutable_entry()->set_key(k);
  req.mutable_entry()->set_value(v);
  auto updateTime = goblin::kvengine::utils::TimeUtil::secondsSinceEpoch();
  req.mutable_entry()->set_updatetime(updateTime);
  for (auto &val : udfMeta.uintfield()) {
    req.mutable_entry()->mutable_udfmeta()->add_uintfield(val);
  }
  for (auto &str : udfMeta.strfield()) {
    req.mutable_entry()->mutable_udfmeta()->add_strfield(str);
  }
  return putAsync(req, cb);
}

void MockAppClient::putAsync(const proto::Put::Request &request, PutResponseCallBack cb) {
  auto *call = new PutClientCall;

  std::chrono::time_point deadline = std::chrono::system_clock::now()
      + std::chrono::milliseconds(10000);
  call->mContext.set_deadline(deadline);

  call->mCB = cb;
  call->mResponseReader = mStub->PrepareAsyncPut(&call->mContext, request, &mCompletionQueue);
  call->mResponseReader->StartCall();
  call->mResponseReader->Finish(&call->mResponse,
                                &call->mStatus,
                                reinterpret_cast<void *>(call));
}

void MockAppClient::getAsync(const kvengine::store::KeyType &k, GetResponseCallBack cb) {
  proto::Get::Request req;
  req.mutable_header()->set_knownversion(mKnownVersion);
  req.mutable_header()->set_routeversion(mRouteVersion);
  req.mutable_entry()->set_key(k);
  return getAsync(req, cb);
}

void MockAppClient::getAsync(const kvengine::store::KeyType &k, bool isMetaReturned, GetResponseCallBack cb) {
  if (!isMetaReturned) {
    getAsync(k, cb);
  } else {
    proto::Get::Request req;
    req.mutable_header()->set_knownversion(mKnownVersion);
    req.mutable_header()->set_routeversion(mRouteVersion);
    req.mutable_entry()->set_key(k);
    req.set_ismetareturned(true);
    return getAsync(req, cb);
  }
}

void MockAppClient::getFollowerAsync(const kvengine::store::KeyType &k, GetResponseCallBack cb) {
  proto::Get::Request req;
  req.mutable_header()->set_knownversion(mKnownVersion);
  req.mutable_header()->set_routeversion(mRouteVersion);
  req.mutable_entry()->set_key(k);
  req.set_allowstale(true);
  return getAsync(req, cb);
}

void MockAppClient::getAsync(const proto::Get::Request &request, GetResponseCallBack cb) {
  auto *call = new GetClientCall;

  std::chrono::time_point deadline = std::chrono::system_clock::now()
      + std::chrono::milliseconds(10000);
  call->mContext.set_deadline(deadline);

  call->mCB = cb;
  call->mResponseReader = mStub->PrepareAsyncGet(&call->mContext, request, &mCompletionQueue);
  call->mResponseReader->StartCall();
  call->mResponseReader->Finish(&call->mResponse,
                                &call->mStatus,
                                reinterpret_cast<void *>(call));
}

void MockAppClient::deleteAsync(const kvengine::store::KeyType &k, DeleteResponseCallBack cb) {
  proto::Delete::Request req;
  req.mutable_header()->set_knownversion(mKnownVersion);
  req.mutable_header()->set_routeversion(mRouteVersion);
  req.set_returnvalue(true);
  req.mutable_entry()->set_key(k);
  return deleteAsync(req, cb);
}

void MockAppClient::deleteAsync(const proto::Delete::Request &request, DeleteResponseCallBack cb) {
  auto *call = new DeleteClientCall;

  std::chrono::time_point deadline = std::chrono::system_clock::now()
      + std::chrono::milliseconds(10000);
  call->mContext.set_deadline(deadline);

  call->mCB = cb;
  call->mResponseReader = mStub->PrepareAsyncDelete(&call->mContext, request, &mCompletionQueue);
  call->mResponseReader->StartCall();
  call->mResponseReader->Finish(&call->mResponse,
                                &call->mStatus,
                                reinterpret_cast<void *>(call));
}

void MockAppClient::transAsync(const proto::Transaction::Request &request, TransResponseCallBack cb) {
  auto *call = new TransClientCall;
  auto req = request;
  req.mutable_header()->set_knownversion(mKnownVersion);
  req.mutable_header()->set_routeversion(mRouteVersion);

  std::chrono::time_point deadline = std::chrono::system_clock::now()
      + std::chrono::milliseconds(10000);
  call->mContext.set_deadline(deadline);

  call->mCB = cb;
  call->mResponseReader = mStub->PrepareAsyncTransaction(&call->mContext, req, &mCompletionQueue);
  call->mResponseReader->StartCall();
  call->mResponseReader->Finish(&call->mResponse,
                                &call->mStatus,
                                reinterpret_cast<void *>(call));
}

void MockAppClient::clientLoopMain() {
  auto peerThreadName = std::string("MockAppClient");
  pthread_setname_np(pthread_self(), peerThreadName.c_str());

  void *tag;  /// The tag is the memory location of the call object
  bool ok = false;

  /// Block until the next result is available in the completion queue.
  while (mCompletionQueue.Next(&tag, &ok)) {
    if (!mRunning) {
      SPDLOG_INFO("Client loop quit.");
      return;
    }

    auto *call = static_cast<AsyncClientCallBase *>(tag);
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

}  /// namespace mock
}  /// namespace goblin
