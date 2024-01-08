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

#include "ConnectCallData.h"

#include <utility>

namespace goblin::objectstore::network {

ConnectCallData::ConnectCallData(
    proto::KVStore::AsyncService *service,
    grpc::ServerCompletionQueue *cq,
    kvengine::KVEngine &kvEngine,
    ObjectStore &objectStore):
  RequestCallData(service, cq, kvEngine, objectStore), mResponder(&mContext) {
  // Invoke the serving logic right away.
  proceed();
}

void ConnectCallData::proceed() {
  if (mStatus == CREATE) {
    // Make this instance progress to the PROCESS state.
    mStatus = PROCESS;

    // As part of the initial CREATE state, we *request* that the system
    // start processing SayHello requests. In this request, "this" acts as
    // the tag uniquely identifying the request (so that different CallData
    // instances can serve different requests concurrently), in this case
    // the memory address of this CallData instance.
    mService->RequestConnect(&mContext, &mRequest, &mResponder,
                             mCompletionQueue, mCompletionQueue, this);
  } else if (mStatus == PROCESS) {
    SPDLOG_INFO("debug: receive connect request");
    // Spawn a new CallData instance to serve new clients while we process
    // the one for this CallData. The instance will deallocate itself as
    // part of its FINISH state.
    new ConnectCallData(mService, mCompletionQueue, mKVEngine, mObjectStore);
    mKVEngine.connectAsync(mRequest, [this](const proto::Connect::Response& resp) {
        /// SPDLOG_INFO("debug: cb in connect");
        mStatus = FINISH;
        mResponder.Finish(resp, grpc::Status::OK, this);
        });
  } else {
    GPR_ASSERT(mStatus == FINISH);
    // Once in the FINISH state, deallocate ourselves (CallData).
    delete this;
  }
}

}  /// namespace goblin::objectstore::network


