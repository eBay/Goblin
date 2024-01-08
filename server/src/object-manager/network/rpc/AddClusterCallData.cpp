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

#include "AddClusterCallData.h"
#include "../../module/ClusterSetManager.h"

namespace goblin::objectmanager::network {

  AddClusterCallData::AddClusterCallData(
          proto::KVManager::AsyncService *service,
          grpc::ServerCompletionQueue *cq,
          std::shared_ptr<kvengine::KVEngine> kvEngine)
          : BaseCallData(service, cq, kvEngine), mResponder(&mContext) {
    // Invoke the serving logic right away.
    proceed();
  }

  void AddClusterCallData::proceed() {
    if (mStatus == CREATE) {
      // Make this instance progress to the PROCESS state.
      mStatus = PROCESS;

      // As part of the initial CREATE state, we *request* that the system
      // start processing SayHello requests. In this request, "this" acts as
      // the tag uniquely identifying the request (so that different CallData
      // instances can serve different requests concurrently), in this case
      // the memory address of this CallData instance.
      mService->RequestAddCluster(&mContext, &mRequest, &mResponder,
                                   mCompletionQueue, mCompletionQueue, this);
    } else if (mStatus == PROCESS) {
      // Spawn a new CallData instance to serve new clients while we process
      // the one for this CallData. The instance will deallocate itself as
      // part of its FINISH state.
      new AddClusterCallData(mService, mCompletionQueue, mKVEngine);
      mStatus = FINISH;

      proto::AddCluster::Response response;
      auto& header = *response.mutable_header();
      buildHeader(header, proto::OK);

      if (!mRequest.has_cluster() || mRequest.cluster().servers_size() < 1) {
        buildHeader(header, proto::BAD_REQUEST, "Insufficient node found");
        return mResponder.Finish(response, okStatus("Insufficient node found"), this);
      }
      SPDLOG_INFO("AddClusterCallData, begin to process request");

      module::ClusterSetManager clusterSetManager{mKVEngine};
      clusterSetManager.init(header);
      SPDLOG_INFO("AddClusterCallData, clusterSetManager init return code {}", header.code());
      if (header.code() != proto::OK) {
        return mResponder.Finish(response, buildStatus(header.code()), this);
      }

      clusterSetManager.addCluster(header, mRequest.cluster().servers());
      SPDLOG_INFO("AddClusterCallData, clusterSetManager.addCluster return code {}", header.code());
      mResponder.Finish(response, buildStatus(header.code()), this);
      SPDLOG_INFO("AddClusterCallData, process request done");
    } else {
      GPR_ASSERT(mStatus == FINISH);
      // Once in the FINISH state, deallocate ourselves (CallData).
      delete this;
    }
  }
}  // namespace goblin::objectmanager::network
