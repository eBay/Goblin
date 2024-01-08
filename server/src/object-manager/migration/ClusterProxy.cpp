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

#include "ClusterProxy.h"

namespace goblin::objectmanager::migration {
  ClusterProxy& ClusterProxy::instance() {
    static ClusterProxy proxy;
    return proxy;
  }

  void ClusterProxy::init(const std::optional<kvengine::utils::TlsConf>& tlsConf) {
    this->mTlsConf = tlsConf;
  }

  void ClusterProxy::startMigration(const proto::StartMigration::Request& request, ClusterIdType clusterId) {
    auto it = mClusterMap.find(clusterId);
    if (it != mClusterMap.end()) {
      SPDLOG_INFO("Cluster {} found, begin to call doMigration", clusterId);
      it->second->doMigration(request);
    } else {
      SPDLOG_WARN("Cluster {} not found in cluster proxy", clusterId);
    }
  }

  void ClusterProxy::endMigration(const proto::EndMigration_Request& request, ClusterIdType clusterId) {
    auto it = mClusterMap.find(clusterId);
    if (it != mClusterMap.end()) {
      SPDLOG_INFO("Cluster {} found, begin to call endMigration", clusterId);
      it->second->endMigration(request);
    } else {
      SPDLOG_WARN("Cluster {} not found in cluster proxy", clusterId);
    }
  }

  void ClusterProxy::registerCluster(ClusterIdType clusterId, const proto::Cluster::ClusterAddress& clusterAddress) {
    if (mClusterMap.find(clusterId) == mClusterMap.end()) {
      mClusterMap[clusterId] = std::make_unique<ClusterClient>(clusterAddress, mTlsConf);
    }
  }
}  //  namespace goblin::objectmanager::migration
