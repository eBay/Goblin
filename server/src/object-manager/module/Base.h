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

#ifndef SERVER_SRC_OBJECT_MANAGER_MODULE_BASE_H_
#define SERVER_SRC_OBJECT_MANAGER_MODULE_BASE_H_

#include "BaseTypes.h"
#include "../../kv-engine/KVEngine.h"

#include "../../../protocols/generated/control.pb.h"

namespace goblin::objectmanager::module {

  using ServerAddress = proto::Cluster_ServerAddress;
  using ClusterAddress = proto::Cluster_ClusterAddress;
  using ClusterSetGlobalInfo = proto::Cluster::ClusterSetGlobalInfo;
  using ClusterInfo = proto::Cluster_ClusterInfo;
  using ClusterStatus = proto::Cluster_ClusterStatus;
  using MigrationTask = proto::Migration_MigrationTask;
  using MigrationProgress = proto::Migration_MigrationProgress;
  using MigrationStatus = proto::Migration_MigrationStatus;
  using RouteInfo = proto::RouteInfo;
  using ResponseHeader = proto::ResponseHeader;
  using ServerAddressList = google::protobuf::RepeatedPtrField<ServerAddress>;

class Base {
 public:
    explicit Base(const std::shared_ptr<kvengine::KVEngine>&);
    static ResponseHeader& buildHeader(ResponseHeader&, proto::ResponseCode, const std::string& = "ok");

 protected:
    static std::string buildClusterKey(ClusterIdType);
    static std::string buildMigrationTaskKey(MigrationTaskIdType);
    static void fillExistPrecondition(proto::Precondition_ExistCondition*, const std::string&, bool);

    const std::shared_ptr<kvengine::KVEngine>& mKVEngine;

    inline static std::string KEY_CLUSTER_INFO_PREFIX = "/kv_manager/cluster_#";
    inline static std::string KEY_MIGRATION_INFO_PREFIX = "/kv_manager/migration_#";
    inline static std::string KEY_CLUSTER_GLOBAL_INFO = "/kv_manager/global_info";

    inline static constexpr VersionType NON_VERSION = 0;
    inline static constexpr ClusterIdType NON_CLUSTER_ID = 0;
};
}  //  namespace goblin::objectmanager::module

#endif  //  SERVER_SRC_OBJECT_MANAGER_MODULE_BASE_H_
