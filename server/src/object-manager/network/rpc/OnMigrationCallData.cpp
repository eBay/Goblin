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

#include "OnMigrationCallData.h"

#include "../../../kv-engine/utils/StrUtil.h"
#include "../../module/ClusterSetManager.h"

namespace goblin::objectmanager::network {
  OnMigrationCallData::OnMigrationCallData(
          proto::KVManager::AsyncService *service,
          grpc::ServerCompletionQueue *cq,
          std::shared_ptr<kvengine::KVEngine> kvEngine)
          : BaseCallData(service, cq, kvEngine), mResponder(&mContext) {
    // Invoke the serving logic right away.
    proceed();
  }

  void OnMigrationCallData::proceed() {
    if (mStatus == CREATE) {
      // Make this instance progress to the PROCESS state.
      mStatus = PROCESS;

      // As part of the initial CREATE state, we *request* that the system
      // start processing SayHello requests. In this request, "this" acts as
      // the tag uniquely identifying the request (so that different CallData
      // instances can serve different requests concurrently), in this case
      // the memory address of this CallData instance.
      mService->RequestOnMigration(&mContext, &mRequest, &mResponder,
                                   mCompletionQueue, mCompletionQueue, this);
    } else if (mStatus == PROCESS) {
      // Spawn a new CallData instance to serve new clients while we process
      // the one for this CallData. The instance will deallocate itself as
      // part of its FINISH state.
      new OnMigrationCallData(mService, mCompletionQueue, mKVEngine);
      mStatus = FINISH;

      proto::OnMigration::Response response;
      auto &header = *response.mutable_header();
      buildHeader(header, proto::OK);

      if (isSuccess()) {
        module::ClusterSetManager clusterSetManager{mKVEngine};
        clusterSetManager.init(header);
        SPDLOG_INFO("OnMigrationCallData, clusterSetManager init return code {}", header.code());
        if (header.code() != proto::OK) {
          return mResponder.Finish(response, buildStatus(header.code()), this);
        }

        const auto& progress = mRequest.progress();
        auto[pSourceCluster, pTargetCluster] = clusterSetManager.handleMigrationSuccess(header, progress);
        SPDLOG_INFO("OnMigrationCallData, clusterSetManager.handleMigrationSuccess return code {}", header.code());
        mResponder.Finish(response, buildStatus(header.code()), this);

        if (header.code() == proto::OK) {
          clusterSetManager.endMigration(pSourceCluster, pTargetCluster);
        }
        return;
      }

      if (isFailure()) {
        SPDLOG_WARN("OnMigrationCallData, process failure migration not implemented");
        buildHeader(header, proto::OK, "failure migration not handled");
        return mResponder.Finish(response, buildStatus(header.code()), this);
      }

      SPDLOG_INFO("OnMigrationCallData, migration status is {}, return {}",
                  mRequest.progress().overallstatus(), response.header().code());
      const auto& msg = kvengine::utils::StrUtil::format("migration, overallStatus is %d",
                                                         mRequest.progress().overallstatus());
      buildHeader(header, proto::OK, msg);
      mResponder.Finish(response, buildStatus(header.code()), this);
    } else {
      GPR_ASSERT(mStatus == FINISH);
      // Once in the FINISH state, deallocate ourselves (CallData).
      delete this;
    }
  }

  bool OnMigrationCallData::isSuccess() const {
    return mRequest.progress().overallstatus() == proto::Migration::SUCCEEDED;
  }

  bool OnMigrationCallData::isFailure() const {
    return mRequest.progress().overallstatus() == proto::Migration::FAILED;
  }
}  //  namespace goblin::objectmanager::network
