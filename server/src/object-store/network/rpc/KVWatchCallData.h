/************************************************************************
Copyright 2020-2021 eBay Inc.
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

#ifndef SERVER_SRC_OBJECT_STORE_NETWORK_RPC_KVWATCHCALLDATA_H_
#define SERVER_SRC_OBJECT_STORE_NETWORK_RPC_KVWATCHCALLDATA_H_

#include <chrono>
#include <grpcpp/alarm.h>

#include "../RequestCallData.h"

namespace goblin::objectstore::network {

class KVWatchCallData final : public RequestCallData {
 public:
  KVWatchCallData(proto::KVStore::AsyncService *service,
                  grpc::ServerCompletionQueue *cq,
                  kvengine::KVEngine &kvEngine,  // NOLINT(runtime/references)
                  objectstore::ObjectStore &objectStore);  // NOLINT(runtime/references)

  void proceed() override;
  void failOver() override;

 private:
  bool needNotify(const kvengine::store::KeyType&, kvengine::store::VersionType);
  void processCancelWatchRequest();
  void processNewWatchRequest();
  void responseWatchData();
  void resetHeartbeatTime();
  bool need2SendHeartbeat() const;
  void sendHeartbeat();
  void sendResponse(const proto::Watch_Response&);

 protected:
  bool mUnwatchReceived;
  std::string mWatchId;
  std::map<kvengine::store::KeyType, kvengine::store::VersionType> mKeyVersionMap;

  grpc::Alarm mAlarm;
  proto::Watch::Request mRequest;
  std::shared_ptr<grpc::ServerAsyncReaderWriter<proto::Watch::Response, proto::Watch::Request>> mStreamPointer;

 private:
  std::chrono::system_clock::time_point mPreHeartbeatTime;
  static constexpr uint64_t HEARTBEAT_INTERVAL_IN_SEC = 30;

 private:
  class UnwatchCallData final : public RequestCallData {
   public:
    UnwatchCallData(proto::KVStore::AsyncService *service,
                    grpc::ServerCompletionQueue *cq,
                    kvengine::KVEngine &kvEngine,           // NOLINT(runtime/references)
                    objectstore::ObjectStore &objectStore,  // NOLINT(runtime/references)
                    KVWatchCallData *pCallData);
    void proceed() override;

   private:
    KVWatchCallData *mKVWatchCallData;
  };
};
}  //  namespace goblin::objectstore::network

#endif  //  SERVER_SRC_OBJECT_STORE_NETWORK_RPC_KVWATCHCALLDATA_H_
