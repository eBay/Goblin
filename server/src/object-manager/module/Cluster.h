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

#ifndef SERVER_SRC_OBJECT_MANAGER_MODULE_CLUSTER_H_
#define SERVER_SRC_OBJECT_MANAGER_MODULE_CLUSTER_H_

#include "Base.h"

namespace goblin::objectmanager::module {

class Cluster final : public Base {
 public:
    Cluster(ClusterInfo, ClusterIdType, VersionType, const std::shared_ptr<kvengine::KVEngine> &);

    void fillPrecondition(proto::Transaction_Request &, ClusterIdType);
    void updateInTransaction(ClusterStatus, proto::Transaction_Request&);
    void removeInTransaction(proto::Transaction_Request&);
    void updateClusterInTransaction(const ServerAddressList&, proto::Transaction_Request&);

    ClusterStatus getStatus() const { return mClusterInfo.status(); }
    ClusterIdType getClusterId() const { return mClusterId; }
    const ClusterAddress& getClusterAddress() const { return mClusterInfo.addr(); }
    const ServerAddressList& getServerAddresses() const { return mClusterInfo.addr().servers(); }

    static std::tuple<proto::ResponseCode, int, ClusterIdType> isAddressInSameCluster(
            const ServerAddressList &, const std::shared_ptr<kvengine::KVEngine> &kvEngine);
    static void createInTransaction(ClusterIdType, const ServerAddressList&, proto::Transaction_Request&);


 private:
    VersionType getVersion() const { return mVersion; }
    void setStatus(ClusterStatus newStatus) { mClusterInfo.set_status(newStatus); }

    ClusterInfo mClusterInfo;
    ClusterIdType mClusterId;
    VersionType mVersion;

    inline static ClusterIdType NON_CLUSTER_ID = 0;
};
}  /// end namespace goblin::objectmanager::module


#endif  //  SERVER_SRC_OBJECT_MANAGER_MODULE_CLUSTER_H_
