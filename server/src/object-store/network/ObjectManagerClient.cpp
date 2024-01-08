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

#include "ObjectManagerClient.h"

#include <absl/strings/str_split.h>
#include <spdlog/spdlog.h>
#include <grpcpp/server_builder.h>

#include <utility>

#include <infra/common_types.h>

#include "../../kv-engine/utils/TessDNSResolver.h"

namespace goblin::objectstore::network {

ObjectManagerClient::ObjectManagerClient(const INIReader &reader) {
  const auto &objectManagerAddr = reader.Get("objectmanager", "address", "UNKNOWN");
  SPDLOG_INFO("connecting manager: {}", objectManagerAddr);
  mManagerAddresses = absl::StrSplit(objectManagerAddr, ",");
  mTlsConf = kvengine::utils::TlsUtil::parseTlsConf(reader, "tls");
  mDNSResolver = std::make_shared<kvengine::utils::TessDNSResolver>();
  mResolvedManagerAddresses.resize(mManagerAddresses.size());
  mStubs.resize(mManagerAddresses.size());
  for (uint64_t i = 0; i < mManagerAddresses.size(); ++i) {
    refressChannel(i);
  }
  /// start AE_resp/RV_resp receiving thread
  mClientLoop = std::thread(&ObjectManagerClient::clientLoopMain, this);
}

ObjectManagerClient::ObjectManagerClient(const char* config): ObjectManagerClient(INIReader(config)) {
}

ObjectManagerClient::ObjectManagerClient(const std::string& config): ObjectManagerClient(INIReader(config)) {
}

ObjectManagerClient::~ObjectManagerClient() {
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

void ObjectManagerClient::refressChannel(uint64_t addrIndex) {
  assert(addrIndex < mManagerAddresses.size());
  assert(addrIndex < mResolvedManagerAddresses.size());
  assert(addrIndex < mStubs.size());
  auto addr = mManagerAddresses[addrIndex];
  auto &existingResolvedAddr = mResolvedManagerAddresses[addrIndex];
  std::vector<std::string> id2domain = absl::StrSplit(addr, "@");
  SPDLOG_INFO("parsing manager node {}", addr);
  assert(id2domain.size() == 2);
  grpc::ChannelArguments chArgs;
  chArgs.SetMaxReceiveMessageSize(INT_MAX);
  auto newResolvedAddress = mDNSResolver->resolve(id2domain[1]);
  if (newResolvedAddress != existingResolvedAddr) {
    std::unique_lock<std::shared_mutex> lock(mMutex);
    if (newResolvedAddress != existingResolvedAddr) {
      auto channel = grpc::CreateCustomChannel(newResolvedAddress,
          kvengine::utils::TlsUtil::buildChannelCredentials(mTlsConf), chArgs);
      mStubs[addrIndex] = proto::KVManager::NewStub(channel);
      existingResolvedAddr = newResolvedAddress;
    }
  }
}

void ObjectManagerClient::addCluster(
    const proto::AddCluster::Request &request,
    AddClusterCallBack cb) {
  auto *call = new AsyncClientCall<proto::AddCluster::Request, proto::AddCluster::Response>(
      AsyncRequestType::ADD_CLUSTER, request);
  call->mCB = cb;
  std::chrono::time_point deadline = std::chrono::system_clock::now()
      + std::chrono::milliseconds(10000);
  call->mContext.set_deadline(deadline);
  std::shared_lock<std::shared_mutex> lock(mMutex);
  call->mResponseReader = mStubs[mCurStubIndex]->PrepareAsyncAddCluster(&call->mContext, request, &mCompletionQueue);
  call->mResponseReader->StartCall();
  call->mResponseReader->Finish(&call->mResponse,
                                &call->mStatus,
                                reinterpret_cast<void *>(call));
}

void ObjectManagerClient::requestRouteInfo(
    const proto::Router::Request &request,
    RouterCallBack cb) {
  auto *call = new AsyncClientCall<proto::Router::Request, proto::Router::Response>(AsyncRequestType::ROUTER, request);
  call->mCB = cb;
  std::chrono::time_point deadline = std::chrono::system_clock::now()
      + std::chrono::milliseconds(5000);
  call->mContext.set_deadline(deadline);
  std::shared_lock<std::shared_mutex> lock(mMutex);
  call->mResponseReader = mStubs[mCurStubIndex]->PrepareAsyncRouter(&call->mContext, request, &mCompletionQueue);
  call->mResponseReader->StartCall();
  call->mResponseReader->Finish(&call->mResponse,
                                &call->mStatus,
                                reinterpret_cast<void *>(call));
}

void ObjectManagerClient::reportStartup(
    const proto::OnStartup::Request &request,
    ReportStartupCallBack cb) {
  auto *call = new AsyncClientCall<proto::OnStartup::Request, proto::OnStartup::Response>(
      AsyncRequestType::REPORT_STARTUP, request);
  call->mCB = cb;
  std::chrono::time_point deadline = std::chrono::system_clock::now()
      + std::chrono::milliseconds(5000);
  call->mContext.set_deadline(deadline);
  std::shared_lock<std::shared_mutex> lock(mMutex);
  call->mResponseReader = mStubs[mCurStubIndex]->PrepareAsyncOnStartup(&call->mContext, request, &mCompletionQueue);
  call->mResponseReader->StartCall();
  call->mResponseReader->Finish(&call->mResponse,
                                &call->mStatus,
                                reinterpret_cast<void *>(call));
}

void ObjectManagerClient::reportMigrationProgress(
    const proto::OnMigration::Request &request,
    ReportMigrationCallBack cb) {
  auto *call = new AsyncClientCall<proto::OnMigration::Request, proto::OnMigration::Response>(
      AsyncRequestType::REPORT_MIGRATION, request);
  call->mCB = cb;
  std::chrono::time_point deadline = std::chrono::system_clock::now()
      + std::chrono::milliseconds(5000);
  call->mContext.set_deadline(deadline);
  std::shared_lock<std::shared_mutex> lock(mMutex);
  call->mResponseReader = mStubs[mCurStubIndex]->PrepareAsyncOnMigration(&call->mContext, request, &mCompletionQueue);
  call->mResponseReader->StartCall();
  call->mResponseReader->Finish(&call->mResponse,
                                &call->mStatus,
                                reinterpret_cast<void *>(call));
}

void ObjectManagerClient::clientLoopMain() {
  auto peerThreadName = "ObjectManagerClient";
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
      refressChannel(mCurStubIndex);
    }
    if (call->getType() == AsyncRequestType::ADD_CLUSTER) {
      auto addClusterCall = dynamic_cast<AsyncClientCall<proto::AddCluster::Request, proto::AddCluster::Response>*>(
          call);
      if (needRetry(call->mStatus.error_code(), addClusterCall->mResponse.header().code())) {
        SPDLOG_ERROR("got error when add cluster: {}", addClusterCall->mResponse.header().code());
        auto req = addClusterCall->mRequest;
        auto cb = addClusterCall->mCB;
        delete addClusterCall;
        addCluster(req, cb);
      } else {
        addClusterCall->mCB(addClusterCall->mResponse);
        delete addClusterCall;
      }
    } else if (call->getType() == AsyncRequestType::ROUTER) {
      auto routeCall = dynamic_cast<AsyncClientCall<proto::Router::Request, proto::Router::Response>*>(call);
      if (needRetry(call->mStatus.error_code(), routeCall->mResponse.header().code())) {
        SPDLOG_ERROR("got error when route requesting: {}", routeCall->mResponse.header().code());
        auto req = routeCall->mRequest;
        auto cb = routeCall->mCB;
        delete routeCall;
        requestRouteInfo(req, cb);
      } else {
        routeCall->mCB(routeCall->mResponse);
        delete routeCall;
      }
    } else if (call->getType() == AsyncRequestType::REPORT_STARTUP) {
      auto startupCall = dynamic_cast<AsyncClientCall<proto::OnStartup::Request, proto::OnStartup::Response>*>(call);
      if (needRetry(call->mStatus.error_code(), startupCall->mResponse.header().code())) {
        SPDLOG_ERROR("got error when report startup: {}", startupCall->mResponse.header().code());
        auto req = startupCall->mRequest;
        auto cb = startupCall->mCB;
        delete startupCall;
        reportStartup(req, cb);
      } else {
        startupCall->mCB(startupCall->mResponse);
        delete startupCall;
      }
    } else if (call->getType() == AsyncRequestType::REPORT_MIGRATION) {
      auto migrationCall = dynamic_cast<AsyncClientCall<proto::OnMigration::Request, proto::OnMigration::Response>*>(
          call);
      if (needRetry(call->mStatus.error_code(), migrationCall->mResponse.header().code())) {
        SPDLOG_ERROR("got error when report migration: {}", migrationCall->mResponse.header().code());
        auto req = migrationCall->mRequest;
        auto cb = migrationCall->mCB;
        delete migrationCall;
        reportMigrationProgress(req, cb);
      } else {
        migrationCall->mCB(migrationCall->mResponse);
        delete migrationCall;
      }
    } else {
      SPDLOG_ERROR("invalid async call");
    }
  }
}

bool ObjectManagerClient::needRetry(grpc::StatusCode grpcCode, proto::ResponseCode code) {
  if (grpcCode == grpc::StatusCode::UNAVAILABLE || code == proto::ResponseCode::NOT_LEADER) {
    mCurRetry++;
    mCurStubIndex = mCurRetry % mStubs.size();
    return true;
  } else {
    return false;
  }
}

}  /// namespace goblin::objectstore::network
