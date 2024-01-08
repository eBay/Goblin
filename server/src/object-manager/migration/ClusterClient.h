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

#ifndef SERVER_SRC_OBJECT_MANAGER_MIGRATION_CLUSTERCLIENT_H_
#define SERVER_SRC_OBJECT_MANAGER_MIGRATION_CLUSTERCLIENT_H_

#include <string>
#include <map>
#include <chrono>
#include <thread>

#include <INIReader.h>
#include <infra/util/DNSResolver.h>

#include "../../../../protocols/generated/service.grpc.pb.h"
#include "../../../../protocols/generated/control.pb.h"

#include "../../kv-engine/utils/TlsUtil.h"

namespace goblin::objectmanager::migration {
using NodeIdType = uint32_t;

class ClusterClient {
 public:
  ClusterClient(proto::Cluster::ClusterAddress , const std::optional<kvengine::utils::TlsConf> &);

  proto::StartMigration::Response doMigration(const proto::StartMigration::Request &);
  proto::EndMigration_Response endMigration(const proto::EndMigration_Request &);

  void update(const proto::Cluster::ClusterAddress &);

  uint64_t getNextNodeIndex(uint64_t) const;

 private:
  void init(const std::optional<kvengine::utils::TlsConf> &tlsConf);
  void buildClient(NodeIdType, const std::string&);

  std::tuple<proto::ResponseCode, proto::StartMigration::Response> doMigrationImpl(
      const proto::StartMigration::Request &, uint64_t , int);
  std::tuple<proto::ResponseCode, proto::EndMigration_Response> endMigrationImpl(
      const proto::EndMigration_Request &, uint64_t, int);

  NodeIdType getNodeIdByIndex(uint64_t);
  int getIndexByNodeId(NodeIdType);
  void doSleep(int retry);

  static constexpr int MAX_ROUND = 3;
  static constexpr int SLEEP_MILLI_SEC_PER_ROUND = 100;

 private:
  class NodeClient {
   public:
    NodeClient(const std::shared_ptr<grpc::Channel> &channel, NodeIdType nodeId, std::string hostName);
    void doMigration(const proto::StartMigration::Request &, proto::StartMigration::Response *);
    void endMigration(const proto::EndMigration_Request &, proto::EndMigration_Response *);

    const std::string& getHostname() const { return mHostName; }
    NodeIdType getNodeId() const { return mNodeId; }

   private:
    NodeIdType mNodeId;
    std::string mHostName;
    std::unique_ptr<proto::KVStore::Stub> mStub;
  };

  uint64_t mLeaderIndex;
  std::vector<NodeClient*> mNodeClientVector;
  std::map<NodeIdType, std::unique_ptr<NodeClient>> mClientMap;
  proto::Cluster::ClusterAddress mClusterAddress;
  std::optional<kvengine::utils::TlsConf> mTlsConf;
  std::shared_ptr<gringofts::DNSResolver> mDNSResolver;
};
}  // namespace goblin::objectmanager::migration

#endif  // SERVER_SRC_OBJECT_MANAGER_MIGRATION_CLUSTERCLIENT_H_
