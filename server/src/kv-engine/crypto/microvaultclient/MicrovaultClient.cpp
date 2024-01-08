/*
 * Copyright (c) 2021 eBay Software Foundation. All rights reserved.
 */

#include "MicrovaultClient.h"

#include <grpcpp/create_channel.h>
#include <spdlog/spdlog.h>

namespace goblin::kvengine::crypto {

MicrovaultClient::MicrovaultClient(int port)
    : mEndPoint("0.0.0.0:" + std::to_string(port)) {
  refreshChannel();
}

void MicrovaultClient::refreshChannel() {
  grpc::ChannelArguments chArgs;
  chArgs.SetMaxReceiveMessageSize(INT_MAX);
  chArgs.SetInt("grpc.testing.fixed_reconnect_backoff_ms", 100);
  auto channel = grpc::CreateChannel(mEndPoint, grpc::InsecureChannelCredentials());
  mStub = ::microvault::MicroVault::NewStub(channel);
}

grpc::Status MicrovaultClient::getLatestVersion(const std::string &keyPath, int *latestVersion) {
  ::microvault::GetKeyInfoRequest request;
  ::microvault::GetKeyInfoResponse response;
  grpc::Status status;

  request.set_keyref(keyPath);

  for (int i = 0; i < kMaxRetryTimes; ++i) {
    grpc::ClientContext context;
    status = mStub->GetKeyInfo(&context, request, &response);
    if (status.ok()) {
      *latestVersion = response.key().version();
      SPDLOG_INFO("The latest version of key {} is {}", keyPath, *latestVersion);
      return status;
    } else if (status.error_code() == grpc::StatusCode::NOT_FOUND) {
      SPDLOG_ERROR("The key {} is not found", keyPath);
      return status;
    }

    SPDLOG_WARN("Failed to get latest version of key {}, retry times: {}, error_code: {}, error_message: {}",
        keyPath, i, status.error_code(), status.error_message());
    usleep(1000);  // sleep 1ms
    refreshChannel();
  }

  SPDLOG_ERROR("Failed to get latest version of key {} after {} retries. error_code: {}, error_message: {}",
      keyPath, kMaxRetryTimes, status.error_code(), status.error_message());

  return status;
}

grpc::Status MicrovaultClient::getSecretByVersion(const std::string &keyPath, int version, std::string *content) {
  grpc::ClientContext context;
  ::microvault::ExportKeyRequest request;
  ::microvault::ExportKeyResponse response;
  grpc::Status status;

  request.set_keyref(keyPath + "/versions/" + std::to_string(version));

  for (int i = 0; i < kMaxRetryTimes; ++i) {
    status = mStub->ExportKey(&context, request, &response);
    if (status.ok()) {
      *content = response.key().material();
      return status;
    } else if (status.error_code() == grpc::StatusCode::NOT_FOUND) {
      SPDLOG_ERROR("The key {} with version {} is not found", keyPath, version);
      return status;
    }
    usleep(1000);  // sleep 1ms
    refreshChannel();
  }

  SPDLOG_ERROR("Failed to get version {} of key {} after {} retries. error_code: {}, error_message: {}",
      version, keyPath, kMaxRetryTimes, status.error_code(), status.error_message());

  return status;
}

}  // namespace goblin::kvengine::crypto
