/**
 * Copyright (c) 2021 eBay Software Foundation. All rights reserved.
 */

#ifndef SERVER_SRC_OBJECT_STORE_NETWORK_OSNETADMINSERVICEPROVIDER_H_
#define SERVER_SRC_OBJECT_STORE_NETWORK_OSNETADMINSERVICEPROVIDER_H_

#include <memory>

#include "../../kv-engine/network/NetAdminServiceProvider.h"
#include "../../kv-engine/KVEngine.h"

namespace goblin::objectstore::network {

class OSNetAdminServiceProvider final : public kvengine::network::NetAdminServiceProvider {
 public:
  explicit OSNetAdminServiceProvider(std::shared_ptr<kvengine::KVEngine> kve) :
      mKVEngine(std::move(kve)) {}

  ~OSNetAdminServiceProvider() override = default;

  void truncatePrefix(uint64_t offsetKept) override {
    mKVEngine->truncatePrefix(offsetKept);
  }

  std::pair<bool, std::string> takeSnapshotAndPersist() const override {
    return mKVEngine->takeSnapshotAndPersist();
  }

  std::optional<uint64_t> getLatestSnapshotOffset() const override {
    return mKVEngine->getLatestSnapshotOffset();
  }

  kvengine::utils::Status getMemberOffsets(kvengine::OffsetInfo *leader,
       std::vector<kvengine::OffsetInfo> *followers) const override {
    return mKVEngine->getMemberOffsets(leader, followers);
  }

  uint64_t lastApplied() const override {
    return mKVEngine->lastApplied();
  }

  std::optional<uint64_t> getLeaderHint() const override {
    return mKVEngine->getLeaderHint();
  }

  bool isLeader() const override {
    return mKVEngine->checkLeaderShip();
  }

 private:
  std::shared_ptr<kvengine::KVEngine> mKVEngine;
};

}  /// namespace goblin::objectstore::network

#endif  // SERVER_SRC_OBJECT_STORE_NETWORK_OSNETADMINSERVICEPROVIDER_H_
