/*
 * Copyright (c) 2021 eBay Software Foundation. All rights reserved.
 */

#ifndef SERVER_SRC_KV_ENGINE_CRYPTO_MICROVAULTCLIENT_MICROVAULTCLIENT_H_
#define SERVER_SRC_KV_ENGINE_CRYPTO_MICROVAULTCLIENT_MICROVAULTCLIENT_H_

#include <string>

#include "generated/grpc/microvault.grpc.pb.h"

namespace goblin::kvengine::crypto {

/**
 * This class implements microvault grpc client
 * microvault guide: https://github.corp.ebay.com/security-platform/microvault
 */
class MicrovaultClient {
 public:
  MicrovaultClient() = delete;
  explicit MicrovaultClient(int port);

  /// get the latest version of the key from microvault
  grpc::Status getLatestVersion(const std::string &keyPath, int *version);
  /// get the secret content of the given key with given version from microvault
  grpc::Status getSecretByVersion(const std::string &keyPath, int version, std::string *keyContent);

 private:
  /// recreate the channel
  void refreshChannel();

  const std::string mEndPoint;
  static constexpr int kMaxRetryTimes = 3;
  std::unique_ptr<::microvault::MicroVault::Stub> mStub;
};

}  // namespace goblin::kvengine::crypto

#endif  // SERVER_SRC_KV_ENGINE_CRYPTO_MICROVAULTCLIENT_MICROVAULTCLIENT_H_
