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

#ifndef SERVER_SRC_KV_ENGINE_NETWORK_NETADMINSERVICEPROVIDER_H_
#define SERVER_SRC_KV_ENGINE_NETWORK_NETADMINSERVICEPROVIDER_H_

#include <optional>
#include <vector>
#include <string>

namespace goblin::kvengine::network {

/**
 * A class that serves as a proxy between NetAdminServer and StateMachine.
 */
class NetAdminServiceProvider {
 public:
  virtual ~NetAdminServiceProvider() = default;
  /**
  * Take the snapshot of the state machine and persist it to local disk
  * @return <true, path of snapshot file> if succeed, false otherwise
  */
  virtual std::pair<bool, std::string> takeSnapshotAndPersist() const = 0;
  /**
   * See #gringofts::SnapshotUtil::findLatestSnapshotOffset
   */
  virtual std::optional<uint64_t> getLatestSnapshotOffset() const = 0;
  /**
   * See #gringofts::ReadonlyCommandEventStore::truncatePrefix
   * @param offsetKept
   */
  virtual void truncatePrefix(uint64_t offsetKept) = 0;
  /**
   * index of highest log entry applied to state machine
   * @return
   */
  virtual uint64_t lastApplied() const = 0;
  /**
   * raft members' offsets
   * @return <leader's offset info, a vector of followers' offset info>
   */
  virtual kvengine::utils::Status getMemberOffsets(kvengine::OffsetInfo *,
      std::vector<kvengine::OffsetInfo> *) const {
    /// not supported by default
    return kvengine::utils::Status::error("unimplemented");
  }
  virtual std::optional<uint64_t> getLeaderHint() const = 0;

  virtual bool isLeader() const = 0;
};

}  /// namespace goblin::kvengine::network

#endif  // SERVER_SRC_KV_ENGINE_NETWORK_NETADMINSERVICEPROVIDER_H_
