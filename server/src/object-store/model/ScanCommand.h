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

#ifndef SERVER_SRC_OBJECT_STORE_MODEL_SCANCOMMAND_H_
#define SERVER_SRC_OBJECT_STORE_MODEL_SCANCOMMAND_H_

#include "../../kv-engine/model/Command.h"
#include "../../kv-engine/model/Event.h"
#include "../../kv-engine/store/KVStore.h"
#include "../../kv-engine/types.h"
#include "../../kv-engine/utils/Status.h"

namespace goblin::objectstore::model {

class ScanResponse;

using AsyncScanCBFunc = std::function<void(std::shared_ptr<ScanResponse> resp)>;

class ScanRequest {
 public:
  ScanRequest(
       kvengine::store::WSName wsName,
       const std::vector<uint64_t> &shardIds,
       std::shared_ptr<kvengine::store::KVIterator> iterator,
       uint64_t maxScanBatch,
       /// if specific keys are empty, we will scan the whole working set
       /// otherwise, only scan the specific keys
       const std::vector<kvengine::store::KeyType> &specificKeys):
     mWSName(wsName),
     mShardIds(shardIds),
     mIterator(iterator),
     mMaxScanBatch(maxScanBatch),
     mSpecificKeys(specificKeys) {}

  const kvengine::store::WSName& getTargetWS() const { return mWSName; }
  const std::vector<kvengine::store::KeyType>& getSpecificKeys() const { return mSpecificKeys; }
  uint64_t getMaxScanBatch() const { return mMaxScanBatch; }

 private:
  kvengine::store::WSName mWSName;
  std::vector<uint64_t> mShardIds;
  std::shared_ptr<kvengine::store::KVIterator> mIterator;
  uint64_t mMaxScanBatch;
  std::vector<kvengine::store::KeyType> mSpecificKeys;
};

class ScanResponse {
 public:
  ScanResponse() = default;
  ~ScanResponse() = default;

  void setIterator(std::shared_ptr<kvengine::store::KVIterator> it) { mIterator = it; }
  std::shared_ptr<kvengine::store::KVIterator> getIterator() { return mIterator; }
  std::vector<std::tuple<
    kvengine::store::KeyType,
    kvengine::store::ValueType,
    kvengine::store::VersionType,
    kvengine::store::TTLType>>& getScannedData() { return mData; }

  void setCode(uint64_t code) { mCode = code; }
  void setMessage(std::string msg) { mMessage = msg; }

 private:
  std::vector<std::tuple<
    kvengine::store::KeyType,
    kvengine::store::ValueType,
    kvengine::store::VersionType,
    kvengine::store::TTLType>> mData;
  std::shared_ptr<kvengine::store::KVIterator> mIterator;
  uint64_t mCode = proto::ResponseCode::UNKNOWN;
  std::string mMessage;
};

class ScanCommandContext final : public kvengine::model::CommandContext {
 public:
  ScanCommandContext(
       std::shared_ptr<ScanRequest> req,
       AsyncScanCBFunc cb);
  ~ScanCommandContext() = default;

  void initSuccessResponse(
      const kvengine::store::VersionType &curMaxVersion,
      const kvengine::model::EventList &events) override;
  void fillResponseAndReply(
      proto::ResponseCode code,
      const std::string &message,
      std::optional<uint64_t> leaderId) override;

  void reportSubMetrics() override;

  const ScanRequest& getRequest();
  ScanResponse& getResponse();

 private:
  std::shared_ptr<ScanRequest> mRequest;
  std::shared_ptr<ScanResponse> mResponse;
  AsyncScanCBFunc mCB;
};

class ScanCommand final : public kvengine::model::Command {
 public:
  explicit ScanCommand(std::shared_ptr<ScanCommandContext> context);
  ~ScanCommand() = default;

  std::shared_ptr<kvengine::model::CommandContext> getContext() override;

  kvengine::utils::Status prepare(const std::shared_ptr<kvengine::store::KVStore> &) override;
  kvengine::utils::Status execute(
      const std::shared_ptr<kvengine::store::KVStore> &,
      kvengine::model::EventList *) override;
  kvengine::utils::Status finish(
      const std::shared_ptr<kvengine::store::KVStore> &kvStore,
      const kvengine::model::EventList &events) override;

 private:
  kvengine::utils::Status iterateAll(
      const std::shared_ptr<kvengine::store::KVStore> &kvStore,
      kvengine::model::EventList *events,
      ScanResponse *resp);
  kvengine::utils::Status iterateSpecificKeys(
      const std::shared_ptr<kvengine::store::KVStore> &kvStore,
      kvengine::model::EventList *events,
      ScanResponse *resp);
  std::shared_ptr<ScanCommandContext> mContext;
};

}  // namespace goblin::objectstore::model

#endif  // SERVER_SRC_OBJECT_STORE_MODEL_SCANCOMMAND_H_
