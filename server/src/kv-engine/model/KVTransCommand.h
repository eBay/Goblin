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

#ifndef SERVER_SRC_KV_ENGINE_MODEL_KVTRANSCOMMAND_H_
#define SERVER_SRC_KV_ENGINE_MODEL_KVTRANSCOMMAND_H_

#include "Command.h"

namespace goblin::kvengine::model {

using AsyncTransCBFunc = std::function<void(const proto::Transaction::Response&)>;

class KVTransCommandContext : public CommandContext {
 public:
  KVTransCommandContext(const proto::Transaction::Request&, AsyncTransCBFunc);
  void initSuccessResponse(const store::VersionType &curMaxVersion, const model::EventList &events) override;
  void fillResponseAndReply(
      proto::ResponseCode code,
      const std::string &message,
      std::optional<uint64_t> leaderId) override;

  void reportSubMetrics() override;

  const proto::Transaction::Request& getRequest();

  std::set<store::KeyType> getTargetKeys() override;
  proto::RequestHeader getRequestHeader() override;
  bool skipPreExecuteCB() override {
     return false;
  }
  void addIgnoredPutkey(const store::KeyType &key);
  static constexpr char kType[] = "trans";

 private:
  proto::Transaction::Request mRequest;
  proto::Transaction::Response mResponse;
  AsyncTransCBFunc mCB;

  std::unordered_set<store::KeyType> ignoredPutKeys;
};

class KVTransCommand final : public Command {
 public:
  explicit KVTransCommand(std::shared_ptr<KVTransCommandContext> context);

  std::shared_ptr<CommandContext> getContext() override;

  utils::Status prepare(const std::shared_ptr<store::KVStore> &) override;
  utils::Status execute(const std::shared_ptr<store::KVStore> &, EventList *) override;
  utils::Status finish(const std::shared_ptr<store::KVStore> &kvStore, const EventList &events) override;

  bool isFollowerReadCmd() const override;

 private:
  template <typename KVStore, typename... Types>
  utils::Status readKV(KVStore kvStore, bool isReadFromProxy, Types... vars) const {
    if (isReadFromProxy) {
      return kvStore->readKVFromProxy(vars...);
    } else {
      return kvStore->readKV(vars...);
    }
  }

  template <typename KVStore, typename... Types>
  utils::Status readMeta(KVStore kvStore, bool isReadFromProxy, Types... vars) const {
    if (isReadFromProxy) {
      return kvStore->readMetaFromProxy(vars...);
    } else {
      return kvStore->readMeta(vars...);
    }
  }

  utils::Status checkPrecond(std::shared_ptr<store::KVStore> kvStore);
  std::set<store::KeyType> getTargetKeys();

  std::shared_ptr<KVTransCommandContext> mContext;
  std::unordered_set<store::KeyType> ignoredUdfUintKeys;
  std::unordered_map<store::KeyType, goblin::proto::UserDefinedMeta> udfUintKey2Meta;
};

}  // namespace goblin::kvengine::model

#endif  // SERVER_SRC_KV_ENGINE_MODEL_KVTRANSCOMMAND_H_
