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

#ifndef SERVER_SRC_OBJECT_MANAGER_NETWORK_RPC_REMOVECLUSTERCALLDATA_H_
#define SERVER_SRC_OBJECT_MANAGER_NETWORK_RPC_REMOVECLUSTERCALLDATA_H_

#include "BaseCallData.h"

namespace goblin::objectmanager::network {

class RemoveClusterCallData final : public BaseCallData {
 public:
    RemoveClusterCallData(proto::KVManager::AsyncService *service,
                          grpc::ServerCompletionQueue *cq,
                          std::shared_ptr<kvengine::KVEngine> kvEngine);  // NOLINT(runtime/references)
    void proceed() override;

 protected:
    proto::RemoveCluster::Request mRequest;
    grpc::ServerAsyncResponseWriter <proto::RemoveCluster::Response> mResponder;
};
}  //  namespace goblin::objectmanager::network
#endif  //  SERVER_SRC_OBJECT_MANAGER_NETWORK_RPC_REMOVECLUSTERCALLDATA_H_
