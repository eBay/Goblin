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

#include "KVWatchCallData.h"
#include "../../watch/WatchCenter.h"

namespace goblin::objectstore::network {
  KVWatchCallData::KVWatchCallData(proto::KVStore::AsyncService *service,
                                   grpc::ServerCompletionQueue *cq,
                                   kvengine::KVEngine &kvEngine,
                                   objectstore::ObjectStore &objectStore) :
          RequestCallData(service, cq, kvEngine, objectStore), mUnwatchReceived(false),
          mStreamPointer(
              std::make_shared<grpc::ServerAsyncReaderWriter<proto::Watch::Response, proto::Watch::Request>>(&mContext)
              ) {
    proceed();
  }

  void KVWatchCallData::proceed() {
    if (mStatus == CREATE) {
      // Make this instance progress to the PROCESS state.
      mStatus = PROCESS;

      // As part of the initial CREATE state, we *request* that the system
      // start processing SayHello requests. In this request, "this" acts as
      // the tag uniquely identifying the request (so that different CallData
      // instances can serve different requests concurrently), in this case
      // the memory address of this CallData instance.
      mService->RequestWatch(&mContext, mStreamPointer.get(), mCompletionQueue, mCompletionQueue, this);
    } else if (mStatus == PROCESS) {
      if (mWatchId.empty()) {
        processNewWatchRequest();
      } else {
        responseWatchData();
      }
    } else {
      GPR_ASSERT(mStatus == FINISH);
      // Once in the FINISH state, deallocate ourselves (CallData).
      delete this;
    }
  }

  bool KVWatchCallData::needNotify(const kvengine::store::KeyType &key, kvengine::store::VersionType version) {
    return version > mKeyVersionMap[key];
  }

  void KVWatchCallData::processCancelWatchRequest() {
    watch::WatchCenter::instance().unRegisterWatch(mWatchId);
  }

  void KVWatchCallData::processNewWatchRequest() {
    // init state
    if (!mRequest.has_cancelreq() && !mRequest.has_createreq()) {
      SPDLOG_INFO("KVWatchCallData, debug: watch streaming connected");
      mStreamPointer->Read(&mRequest, this);
      return;
    }
    new KVWatchCallData(mService, mCompletionQueue, mKVEngine, mObjectStore);

    proto::Watch_Response resp;
    auto header = resp.mutable_header();
    header->set_code(proto::OK);
    SPDLOG_INFO("debug: receive watch callData request");
    if (!mRequest.has_createreq() || mRequest.createreq().entries_size() < 1) {
      SPDLOG_WARN("debug: receive watch callData request check failed");
      mStatus = FINISH;
      header->set_code(proto::BAD_REQUEST);
      mStreamPointer->WriteAndFinish(resp, grpc::WriteOptions(), grpc::Status::OK, this);
      return;
    }

    /// get watch id and return back
    std::vector<kvengine::store::KeyType> keyVector;
    bool needRetrieveOnConnected = mRequest.createreq().retrieveonconnected();
    for (auto &e : mRequest.createreq().entries()) {
      keyVector.push_back(e.key());
      mKeyVersionMap[e.key()] = e.version();
    }

    mWatchId = watch::WatchCenter::instance().registerWatch(keyVector);
    resp.set_watchid(mWatchId);

    if (needRetrieveOnConnected) {
      proto::ExeBatch::Request batchReq;
      *batchReq.mutable_header() = mRequest.header();
      SPDLOG_INFO("receiver header {}", batchReq.header().routeversion());
      for (auto &e : mRequest.createreq().entries()) {
        batchReq.add_entries()->mutable_readentry()->set_key(e.key());
      }

      const auto &batchResp = mKVEngine.getAppliedBatch(batchReq);
      if (batchResp.header().code() == proto::ResponseCode::OK) {
        for (int i = 0, size = batchResp.results_size(); i < size; ++i) {
          auto &result = batchResp.results(i).readresult();
          if (result.code() == proto::ResponseCode::KEY_NOT_EXIST || result.code() == proto::ResponseCode::OK) {
            auto pChange = resp.add_changes();
            pChange->set_key(batchReq.entries(i).readentry().key());
            pChange->set_value(result.value());
            pChange->set_version(result.version());
            auto eventType = result.code() == proto::ResponseCode::KEY_NOT_EXIST ?
                             proto::Watch::INIT_NOTEXIST : proto::Watch::INIT_EXIST;
            pChange->set_eventtype(eventType);
          } else {
            SPDLOG_WARN("failed to get initial value, err {}", result.code());
            header->set_code(result.code());
          }
        }
        sendResponse(resp);
      } else {
        *header = batchResp.header();
        sendResponse(resp);
      }
    } else {
      SPDLOG_INFO("no need to retrieve watch key data, just return watch id");
      sendResponse(resp);
    }
  }

  void KVWatchCallData::responseWatchData() {
    if (!mUnwatchReceived) {
      auto pCancelData = new UnwatchCallData(mService, mCompletionQueue, mKVEngine, mObjectStore, this);
      mStreamPointer->Read(&mRequest, pCancelData);
      mUnwatchReceived = true;
      SPDLOG_INFO("watch, register to receive unwatch request");
    }

    std::vector<std::unique_ptr<watch::WatchEntry>> valueChangeVec;
    bool result = watch::WatchCenter::instance().purgeWatchValue(mWatchId, &valueChangeVec);
    if (!result) {
      SPDLOG_INFO("watch id not found, current status is {}", mStatus);
      mStatus = FINISH;
      mStreamPointer->Finish(grpc::Status::OK, this);
      return;
    }

    if (valueChangeVec.empty()) {
      if (this->need2SendHeartbeat()) {
        this->sendHeartbeat();
      } else {
        mStatus = PROCESS;
        mAlarm.Set(mCompletionQueue, gpr_now(gpr_clock_type::GPR_CLOCK_REALTIME), this);
      }
      return;
    }

    proto::Watch_Response resp;
    resp.set_watchid(mWatchId);
    auto header = resp.mutable_header();
    header->set_code(proto::OK);

    for (auto &data : valueChangeVec) {
      if (needNotify(data->getKey(), data->getRecordVersion())) {
        auto pChange = resp.add_changes();
        pChange->set_key(data->getKey());
        if (data->isDeleted()) {
          SPDLOG_INFO("notify deleted key {}, version {}, record version {}",
            data->getKey(), data->getVersion(), data->getRecordVersion());
          pChange->set_version(data->getVersion());
          pChange->set_eventtype(proto::Watch_EventType_DELETED);
        } else {
          SPDLOG_INFO("notify overritten key {}, version {}", data->getKey(), data->getVersion());
          pChange->set_eventtype(proto::Watch_EventType_OVERWRITTEN);
          pChange->set_value(data->getValue());
          pChange->set_version(data->getVersion());
        }
      } else {
        SPDLOG_WARN("no need to notify, key is {}, version is {}, record version {}, isDelete {}",
            data->getKey(), data->getVersion(), data->getRecordVersion(), data->isDeleted());
      }
    }
    sendResponse(resp);
  }

  void KVWatchCallData::failOver() {
    SPDLOG_WARN("KVWatchCallData::failOver, status is {}", mStatus);
    if (!mWatchId.empty()) {
      watch::WatchCenter::instance().unRegisterWatch(mWatchId);
    }
    delete this;
  }

  void KVWatchCallData::resetHeartbeatTime() {
    mPreHeartbeatTime = std::chrono::system_clock::now();
  }

  bool KVWatchCallData::need2SendHeartbeat() const {
    std::chrono::duration<double> elapsed_seconds = std::chrono::system_clock::now() - mPreHeartbeatTime;
    auto result = elapsed_seconds.count() > KVWatchCallData::HEARTBEAT_INTERVAL_IN_SEC;
    if (result) {
      SPDLOG_INFO("KVWatchCallData - need2SendHeartbeat {} - duration: {} - interval: {}",
          std::chrono::system_clock::to_time_t(mPreHeartbeatTime), elapsed_seconds.count(),
          KVWatchCallData::HEARTBEAT_INTERVAL_IN_SEC);
    }
    return result;
  }

  void KVWatchCallData::sendHeartbeat() {
    proto::Watch_Response resp;
    resp.set_watchid(mWatchId);
    resp.mutable_header()->set_code(proto::OK);

    auto pChange = resp.add_changes();
    pChange->set_eventtype(proto::Watch_EventType_HEARTBEAT);
    // SPDLOG_INFO("KVWatchCallData::sendHeartbeat invoked.");

    sendResponse(resp);
  }

  void KVWatchCallData::sendResponse(const proto::Watch_Response &resp) {
    mStreamPointer->Write(resp, this);
    this->resetHeartbeatTime();
  }

  KVWatchCallData::UnwatchCallData::UnwatchCallData(
          proto::KVStore::AsyncService *service, grpc::ServerCompletionQueue *cq,
          kvengine::KVEngine &kvEngine, objectstore::ObjectStore &objectStore,
          KVWatchCallData *pCallData)
          : RequestCallData(service, cq, kvEngine, objectStore),
          mKVWatchCallData(pCallData) {
  }

  void KVWatchCallData::UnwatchCallData::proceed() {
    SPDLOG_INFO("UnwatchCallData::begin to un-register watch");
    mKVWatchCallData->processCancelWatchRequest();
    delete this;
  }

}  ///  namespace goblin::objectstore::network
