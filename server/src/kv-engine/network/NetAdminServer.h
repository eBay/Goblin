/************************************************************************
Copyright 2019-2020 eBay Inc.
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

#ifndef SERVER_SRC_KV_ENGINE_NETWORK_NETADMINSERVER_H_
#define SERVER_SRC_KV_ENGINE_NETWORK_NETADMINSERVER_H_

#include <INIReader.h>
#include <absl/strings/str_format.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>

#include "../utils/TlsUtil.h"
#include "../utils/ClusterInfo.h"

#include "../../../../protocols/generated/netadmin.grpc.pb.h"
#include "NetAdminServiceProvider.h"

namespace goblin::kvengine::network {

// grpc-related
using ::grpc::Server;
using ::grpc::ServerContext;
using ::grpc::ServerBuilder;
using ::grpc::Status;
using ::grpc::Channel;
using ::grpc::ClientContext;

using gringofts::app::protos::AppNetAdmin;
using gringofts::app::protos::CreateSnapshot_Request;
using gringofts::app::protos::CreateSnapshot_Response;
using gringofts::app::protos::CreateSnapshot_ResponseType;
using gringofts::app::protos::TruncatePrefix_Request;
using gringofts::app::protos::TruncatePrefix_Response;
using gringofts::app::protos::TruncatePrefix_ResponseType;
using gringofts::app::protos::GetMemberOffsets_Request;
using gringofts::app::protos::GetMemberOffsets_Response;
using gringofts::app::protos::GetAppliedIndex;
using gringofts::app::protos::GetAppliedIndex_Request;
using gringofts::app::protos::GetAppliedIndex_Response;
/**
 * A server class which exposes some management functionalities to external clients, e.g., pubuddy.
 */
class NetAdminServer final : public AppNetAdmin::Service {
 public:
  NetAdminServer(const INIReader &reader,
                 std::shared_ptr<NetAdminServiceProvider> netAdminProxy,
                 uint64_t port = kvengine::utils::kDefaultNetAdminPort) :
      mServiceProvider(netAdminProxy),
      mSnapshotTakenCounter(gringofts::getCounter("snapshot_taken_counter", {})),
      mSnapshotFailedCounter(gringofts::getCounter("snapshot_failed_counter", {})),
      mPrefixTruncatedCounter(gringofts::getCounter("prefix_truncated_counter", {})),
      mPrefixTruncateFailedCounter(gringofts::getCounter("prefix_truncate_failed_counter", {})) {
    auto netadminPort = reader.Get("netadmin", "ip.port", "UNKNOWN");
    mIpPort = (netadminPort == "UNKNOWN")? absl::StrFormat("0.0.0.0:%d", port) : netadminPort;

    mTlsConfOpt = kvengine::utils::TlsUtil::parseTlsConf(reader, "tls");
  }

  ~NetAdminServer() = default;
  /// disallow copy/move ctor/assignment
  NetAdminServer(const NetAdminServer &) = delete;
  NetAdminServer &operator=(const NetAdminServer &) = delete;

  /**
   * snapshot service, when invoked, will ask app to take a snapshot.
   */
  Status CreateSnapshot(ServerContext *context,
                        const CreateSnapshot_Request *request,
                        CreateSnapshot_Response *reply) override {
    bool expected = false;
    bool ret = mSnapshotIsRunning.compare_exchange_strong(expected, true);
    if (!ret) {
      reply->set_type(CreateSnapshot_ResponseType::CreateSnapshot_ResponseType_PROCESSING);
      return Status::OK;
    }

    SPDLOG_INFO("Start taking a snapshot");
    const auto[succeed, snapshotPath] = mServiceProvider->takeSnapshotAndPersist();
    if (succeed) {
      SPDLOG_INFO("A new snapshot has been persisted to {}", snapshotPath);
      mSnapshotTakenCounter.increase();
    } else {
      SPDLOG_WARN("Failed to create snapshot");
      mSnapshotFailedCounter.increase();
    }
    reply->set_type(succeed ? CreateSnapshot_ResponseType::CreateSnapshot_ResponseType_SUCCESS :
                    CreateSnapshot_ResponseType::CreateSnapshot_ResponseType_FAILED);
    reply->set_message(snapshotPath);

    mSnapshotIsRunning = false;
    return Status::OK;
  }

  /**
   * truncate raft log service, when invoked, will close all the raft segment logs before the specified prefix.
   */
  Status TruncatePrefix(ServerContext *context,
                        const TruncatePrefix_Request *request,
                        TruncatePrefix_Response *reply) override {
    bool expected = false;
    bool ret = mTruncatePrefixIsRunning.compare_exchange_strong(expected, true);
    if (!ret) {
      reply->set_type(TruncatePrefix_ResponseType::TruncatePrefix_ResponseType_PROCESSING);
      return Status::OK;
    }

    /// will set success if everything is OK
    reply->set_type(TruncatePrefix_ResponseType::TruncatePrefix_ResponseType_FAILED);
    std::string errorMessage = "Unknown reason";

    do {
      /// 1, get firstIndexKept
      auto firstIndexKept = request->firstindexkept();

      /// 2, call truncatePrefix
      mServiceProvider->truncatePrefix(firstIndexKept);

      reply->set_type(TruncatePrefix_ResponseType::TruncatePrefix_ResponseType_SUCCESS);
      errorMessage = "OK";

      SPDLOG_WARN("Truncate prefix to {}, meanwhile, first index is {}",
                  firstIndexKept, firstIndexKept);
      mPrefixTruncatedCounter.increase();
    } while (0);

    reply->set_message(errorMessage);

    mTruncatePrefixIsRunning = false;
    return Status::OK;
  }

   /**
   *
   */
  Status GetMemberOffsets(ServerContext *context,
                          const GetMemberOffsets_Request *request,
                          GetMemberOffsets_Response *reply) override {
    kvengine::OffsetInfo leader;
    std::vector<kvengine::OffsetInfo> members;
    auto s = mServiceProvider->getMemberOffsets(&leader, &members);
    if (s.isNotLeader()) {
      reply->mutable_header()->set_code(proto::ResponseCode::NOT_LEADER);
      reply->mutable_header()->set_message("Not Leader");
      auto leaderId = mServiceProvider->getLeaderHint();
      reply->mutable_header()->set_reserved(std::to_string(*leaderId));
      SPDLOG_INFO("Not leader, leadhint = {}", std::to_string(*leaderId));
      return Status::OK;
    } else if (!s.isOK()) {
      reply->mutable_header()->set_code(proto::ResponseCode::GENERAL_ERROR);
      reply->mutable_header()->set_message("General Error: Failed to get member offsets");
      return Status::OK;
    }
    reply->mutable_header()->set_code(proto::ResponseCode::OK);
    reply->mutable_leader()->set_server(leader.toString());
    reply->mutable_leader()->set_offset(leader.mOffset);
    SPDLOG_DEBUG("Leader addr = {}, offset = {}", leader.toString(), leader.mOffset);
    for (auto &mem : members) {
      auto follower = reply->add_followers();
      follower->set_server(mem.toString());
      follower->set_offset(mem.mOffset);
      SPDLOG_DEBUG("Follower addr = {}, offset = {}", mem.toString(), mem.mOffset);
    }
    return Status::OK;
  }

  /**
   * get last applied index.
   */
  Status GetAppliedIndex(ServerContext *context,
                        const GetAppliedIndex_Request *request,
                        GetAppliedIndex_Response *reply) override {
    if (mServiceProvider->lastApplied() == 0) {
      reply->mutable_header()->set_code(404);
      reply->mutable_header()->set_message("no log applied");
      return Status::OK;
    }
    reply->mutable_header()->set_code(200);
    reply->set_is_leader(mServiceProvider->isLeader());
    reply->set_applied_index(mServiceProvider->lastApplied());
    return Status::OK;
  }

  /**
   * The main function of the dedicated thread
   */
  void run() {
    if (mIsShutdown) {
      SPDLOG_WARN("NetAdmin server is already down. Will not run again.");
      return;
    }

    std::string server_address(mIpPort);

    ServerBuilder builder;
    /// Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(server_address, kvengine::utils::TlsUtil::buildServerCredentials(mTlsConfOpt));
    /// Register "service" as the instance through which we'll communicate with
    /// clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(this);
    /// Finally assemble the server.
    mServer = builder.BuildAndStart();
    SPDLOG_INFO("Server listening on {}", server_address);
  }

  /**
   * shut down the server
   */
  void shutdown() {
    if (mIsShutdown) {
      SPDLOG_INFO("NetAdmin server is already down");
    } else {
      mIsShutdown = true;
      if (mServer) {
        mServer->Shutdown();
      }
    }
  }

 private:
  std::unique_ptr<Server> mServer;
  std::atomic<bool> mIsShutdown = false;

  std::string mIpPort;
  std::optional<goblin::kvengine::utils::TlsConf> mTlsConfOpt;

  std::shared_ptr<NetAdminServiceProvider> mServiceProvider;
  std::atomic<bool> mSnapshotIsRunning = false;
  std::atomic<bool> mTruncatePrefixIsRunning = false;

  /// metrics start
  mutable santiago::MetricsCenter::CounterType mSnapshotTakenCounter;
  mutable santiago::MetricsCenter::CounterType mSnapshotFailedCounter;
  mutable santiago::MetricsCenter::CounterType mPrefixTruncatedCounter;
  mutable santiago::MetricsCenter::CounterType mPrefixTruncateFailedCounter;
  /// metrics end
};

}  /// namespace goblin::kvengine::network

#endif  // SERVER_SRC_KV_ENGINE_NETWORK_NETADMINSERVER_H_
