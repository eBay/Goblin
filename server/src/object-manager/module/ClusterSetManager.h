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

#ifndef SERVER_SRC_OBJECT_MANAGER_MODULE_CLUSTERSETMANAGER_H_
#define SERVER_SRC_OBJECT_MANAGER_MODULE_CLUSTERSETMANAGER_H_

#include <memory>
#include <vector>
#include <tuple>

#include "Base.h"
#include "Cluster.h"
#include "Migration.h"

namespace goblin::objectmanager::module {

class ClusterSetManager final : public Base {
 public:
    explicit ClusterSetManager(const std::shared_ptr<kvengine::KVEngine>& kvEngine);
    void init(ResponseHeader&);
    void addCluster(ResponseHeader&, const ServerAddressList&);
    std::tuple<ClusterStatus, bool> startCluster(ResponseHeader&, const ServerAddressList&, RouteInfo&, MigrationTask&);
    std::tuple<Cluster*, Cluster*> handleMigrationSuccess(ResponseHeader&, const MigrationProgress&);
    void handleRouteInfoRequest(ResponseHeader&, RouteInfo&);
    void removeCluster(ResponseHeader&, const ServerAddressList&);
    void beginMigration(const RouteInfo&, const MigrationTask&);
    void endMigration(Cluster*, Cluster*);

    static void fillAddressInfo(proto::Cluster_ClusterAddress*, Cluster*);
    static void fillShardInfo(MigrationTask&, const std::vector<ShardIdType>&);
    static void notifyStartMigration(proto::StartMigration_Request&, Cluster&, Cluster&);
    static void notifyEndMigration(proto::EndMigration_Request&, Cluster&);

 private:
    proto::ResponseCode fillRouteInfo(RouteInfo&);
    proto::ResponseCode fillRouteAndMigrationInfo(RouteInfo&, MigrationTask&);
    proto::ResponseCode onboardCluster(Cluster&);
    bool isEmtpy() const { return mVersion == NON_VERSION; }
    void fillPrecondition(proto::Transaction_Request&);
    ClusterIdType assignNewClusterId() const;
    void setVersion(VersionType version) { this->mVersion = version; }
    VersionType getVersion() const { return mVersion; }
    bool hasMigrationTask() const { return !mMigrationVector.empty(); }
    RouteVersionType getRouteVersion() { return mClusterSetGlobalInfo.routeversion(); }
    Cluster* findClusterById(ClusterIdType) const;
    Cluster* findClusterByAddress(const ServerAddressList&) const;
    Migration* findMigrationById(MigrationTaskIdType) const;
    void getRoutingCluster(std::vector<Cluster*>&);
    bool isOnlyOneCluster() const { return mClusterVector.size() == 1; }
    MigrationTaskIdType assignMigrationTaskId();
    void increaseGlobalRouteVersion();
    void updateInTransaction(proto::Transaction_Request&);
    proto::ResponseCode appendNewClusterId(ClusterIdType);
    void removeSuccessMigrationId(MigrationTaskIdType);

    VersionType mVersion;
    ClusterSetGlobalInfo mClusterSetGlobalInfo;
    std::vector<std::unique_ptr<Cluster>> mClusterVector;
    std::vector<std::unique_ptr<Migration>> mMigrationVector;
};
}  ///  end namespace goblin::objectmanager::module

#endif  //  SERVER_SRC_OBJECT_MANAGER_MODULE_CLUSTERSETMANAGER_H_
