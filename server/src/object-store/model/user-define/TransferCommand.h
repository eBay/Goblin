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

#ifndef SERVER_SRC_OBJECT_STORE_MODEL_USER_DEFINE_TRANSFERCOMMAND_H_
#define SERVER_SRC_OBJECT_STORE_MODEL_USER_DEFINE_TRANSFERCOMMAND_H_

#include "../../../kv-engine/model/KVGenerateCommand.h"
#include "../../../kv-engine/utils/Status.h"

namespace goblin::objectstore::model {

class TransferCommand final : public kvengine::model::KVGenerateCommand {
 public:
  explicit TransferCommand(std::shared_ptr<kvengine::model::KVGenerateCommandContext> context) :
    KVGenerateCommand(context) {}
  kvengine::utils::Status generate(
      const std::map<kvengine::store::KeyType,
                    std::pair<kvengine::store::ValueType,
                    kvengine::store::VersionType>> &kvsToRead,
      const kvengine::model::InputInfoType &inputInfo,
      std::map<kvengine::store::KeyType, kvengine::store::ValueType> *kvsToWrite,
      kvengine::model::OutputInfoType *outputInfo) override;
};

}  // namespace goblin::objectstore::model

#endif  // SERVER_SRC_OBJECT_STORE_MODEL_USER_DEFINE_TRANSFERCOMMAND_H_
