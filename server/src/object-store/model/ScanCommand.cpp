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


#include <rocksdb/db.h>
#include <spdlog/spdlog.h>

#include "ScanCommand.h"
#include "../../kv-engine/store/SnapshottedKVStore.h"

namespace goblin::objectstore::model {

ScanCommandContext::ScanCommandContext(
    std::shared_ptr<ScanRequest> req,
    AsyncScanCBFunc cb) : mRequest(req), mCB(cb) {
  mResponse = std::make_shared<ScanResponse>();
}

void ScanCommandContext::initSuccessResponse(
    const kvengine::store::VersionType &curMaxVersion,
    const kvengine::model::EventList &events) {
  mResponse->setCode(proto::ResponseCode::OK);
  mResponse->setMessage("succeed");
}

void ScanCommandContext::fillResponseAndReply(
    proto::ResponseCode code,
    const std::string &message,
    std::optional<uint64_t> leaderId) {
  if (code != proto::ResponseCode::OK) {
    mResponse->setCode(code);
  }
  mResponse->setMessage(message);
  mCB(mResponse);
}

void ScanCommandContext::reportSubMetrics() {
  /// metrics counter
  auto scanCounter = gringofts::getCounter("scan_command_counter", {});
  scanCounter.increase();
  /// SPDLOG_INFO("debug: write op");
}

const ScanRequest& ScanCommandContext::getRequest() {
  return *mRequest;
}

ScanResponse& ScanCommandContext::getResponse() {
  return *mResponse;
}

ScanCommand::ScanCommand(std::shared_ptr<ScanCommandContext> context):
  mContext(context) {
}

std::shared_ptr<kvengine::model::CommandContext> ScanCommand::getContext() {
  return mContext;
}

kvengine::utils::Status ScanCommand::prepare(const std::shared_ptr<kvengine::store::KVStore> &kvStore) {
  /// acquire shared lock
  return kvStore->lockWS(mContext->getRequest().getTargetWS(), false);
}

kvengine::utils::Status ScanCommand::execute(
    const std::shared_ptr<kvengine::store::KVStore> &kvStore,
    kvengine::model::EventList *events) {
  auto s = kvengine::utils::Status::ok();
  auto &req = mContext->getRequest();
  auto &resp = mContext->getResponse();
  if (req.getSpecificKeys().empty()) {
    s = iterateAll(kvStore, events, &resp);
  } else {
    s = iterateSpecificKeys(kvStore, events, &resp);
  }

  return s;
}

kvengine::utils::Status ScanCommand::finish(
    const std::shared_ptr<kvengine::store::KVStore> &kvStore,
    const kvengine::model::EventList &events) {
  auto s = kvengine::utils::Status::ok();
  for (auto &e : events) {
    s = e->apply(*kvStore);
    if (!s.isOK()) {
      break;
    }
  }
  assert(kvStore->unlockWS(mContext->getRequest().getTargetWS(), false).isOK());
  return s;
}

kvengine::utils::Status ScanCommand::iterateAll(
    const std::shared_ptr<kvengine::store::KVStore> &kvStore,
    kvengine::model::EventList *events,
    ScanResponse *resp) {
  auto s = kvengine::utils::Status::ok();
  auto iterator = resp->getIterator();
  if (!iterator) {
    SPDLOG_INFO("take snapshot");
    auto snapshot = kvStore->takeSnapshot();
    SPDLOG_INFO("new iterator");
    auto newIterator = snapshot->newIterator(mContext->getRequest().getTargetWS());
    SPDLOG_INFO("set iterator");
    resp->setIterator(newIterator);
    iterator = newIterator;
  }
  kvengine::store::KeyType maxVersionKey;
  kvengine::store::ValueType maxVersionValue;
  kvengine::store::TTLType maxVersionTTL;
  kvengine::store::VersionType maxReadVersion = kvengine::store::VersionStore::kInvalidVersion;
  auto &respData = resp->getScannedData();
  uint64_t count = 0;
  SPDLOG_INFO("scanning all");
  iterator->seekToBegin();
  while (iterator->hasValue() && count < mContext->getRequest().getMaxScanBatch()) {
    std::tuple<
      kvengine::store::KeyType,
      kvengine::store::ValueType,
      kvengine::store::TTLType,
      kvengine::store::VersionType> tuple;
    s = iterator->get(tuple);
    if (!s.isOK()) {
      break;
    }
    auto &[key, value, ttl, version] = tuple;
    respData.push_back(std::move(tuple));
    if (version > maxReadVersion) {
      maxVersionKey = key;
      maxReadVersion = version;
      maxVersionValue = value;
      maxVersionTTL = ttl;
    }
    count++;
    iterator->next();
  }
  assert(!s.isNotFound());
  if (s.isOK() && count > 0) {
    /// this guarantee what we read here will be committed
    events->push_back(
        std::make_shared<kvengine::model::ReadEvent>(
          maxVersionKey, maxVersionValue, maxVersionTTL, maxReadVersion, false));
  }
  SPDLOG_INFO("scan total keys: {}, status: {}", count, s.getDetail());
  return s;
}

kvengine::utils::Status ScanCommand::iterateSpecificKeys(
    const std::shared_ptr<kvengine::store::KVStore> &kvStore,
    kvengine::model::EventList *events,
    ScanResponse *resp) {
  auto s = kvengine::utils::Status::ok();
  kvengine::store::KeyType maxVersionKey;
  kvengine::store::ValueType maxVersionValue;
  kvengine::store::TTLType maxVersionTTL;
  kvengine::store::VersionType maxReadVersion = kvengine::store::VersionStore::kInvalidVersion;
  bool hasSomeNotFound = false;
  auto &respData = resp->getScannedData();
  uint64_t count = 0;
  for (auto key : mContext->getRequest().getSpecificKeys()) {
    std::tuple<
      kvengine::store::KeyType,
      kvengine::store::ValueType,
      kvengine::store::TTLType,
      kvengine::store::VersionType> tuple;
    auto &[tupleKey, value, ttl, version] = tuple;
    tupleKey = key;
    s = kvStore->readKV(key, &value, &ttl, &version);
    if (s.isNotFound()) {
      hasSomeNotFound = true;
      /// ignore deleted key
      /// set the whole status to be ok
      s = kvengine::utils::Status::ok();
    }
    if (!s.isOK()) {
      break;
    }
    respData.push_back(std::move(tuple));
    if (version > maxReadVersion) {
      maxVersionKey = key;
      maxReadVersion = version;
      maxVersionValue = value;
      maxVersionTTL = ttl;
    }
  }
  if (s.isOK() && count > 0) {
    if (hasSomeNotFound) {
      /// if there are some keys not found, the read event will wait until latest index is committed
      events->push_back(
          std::make_shared<kvengine::model::ReadEvent>(
            maxVersionKey, maxVersionValue, maxVersionTTL, maxReadVersion, true));
    } else {
      /// this guarantee what we read here will be committed
      events->push_back(
          std::make_shared<kvengine::model::ReadEvent>(
            maxVersionKey, maxVersionValue, maxVersionTTL, maxReadVersion, false));
    }
  }
  SPDLOG_INFO("scan total keys: {}, status: {}", count, s.getDetail());
  return s;
}
}  /// namespace goblin::objectstore::model
