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

#ifndef SERVER_SRC_OBJECT_MANAGER_MIGRATION_CLUSTERPROXY_H_
#define SERVER_SRC_OBJECT_MANAGER_MIGRATION_CLUSTERPROXY_H_

#include <memory>
#include <map>
#include "ClusterClient.h"
#include "../module/BaseTypes.h"

namespace goblin::objectmanager::migration {
  using ClusterIdType = module::ClusterIdType;

class ClusterProxy {
 public:
  ClusterProxy(const ClusterProxy &) = delete;
  ClusterProxy &operator=(const ClusterProxy &) = delete;

  static ClusterProxy &instance();
    void init(const std::optional<kvengine::utils::TlsConf> &);
    void startMigration(const proto::StartMigration::Request &, ClusterIdType clusterId);
    void endMigration(const proto::EndMigration_Request&, ClusterIdType);
    void registerCluster(ClusterIdType clusterId, const proto::Cluster::ClusterAddress &);

 private:
    ClusterProxy() = default;

    std::optional<kvengine::utils::TlsConf> mTlsConf;
    std::map<ClusterIdType, std::unique_ptr<ClusterClient>> mClusterMap;
};
}  ///  end namespace goblin::objectmanager::migration
#endif  //  SERVER_SRC_OBJECT_MANAGER_MIGRATION_CLUSTERPROXY_H_
