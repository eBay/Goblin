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

#ifndef SERVER_SRC_OBJECT_STORE_NETWORK_RPC_KVTRANSCALLDATA_H_
#define SERVER_SRC_OBJECT_STORE_NETWORK_RPC_KVTRANSCALLDATA_H_

#include "../RequestCallData.h"

namespace goblin::objectstore::network {

class KVTransCallData final : public RequestCallData {
 public:
    KVTransCallData(proto::KVStore::AsyncService *service,
                  grpc::ServerCompletionQueue *cq,
                  kvengine::KVEngine &kvEngine,  // NOLINT(runtime/references)
                  objectstore::ObjectStore &objectStore);  // NOLINT(runtime/references)

    void proceed() override;

 protected:
    proto::Transaction::Request mRequest;
    proto::Transaction::Response mResponse;
    grpc::ServerAsyncResponseWriter<proto::Transaction::Response> mResponder;
};

}  // namespace goblin::objectstore::network

#endif  // SERVER_SRC_OBJECT_STORE_NETWORK_RPC_KVTRANSCALLDATA_H_
