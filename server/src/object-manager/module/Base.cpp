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

#include "Base.h"

namespace goblin::objectmanager::module {
  Base::Base(const std::shared_ptr<kvengine::KVEngine>& kvEngine) : mKVEngine(kvEngine) {
  }

  std::string Base::buildClusterKey(ClusterIdType clusterId) {
    return KEY_CLUSTER_INFO_PREFIX + std::to_string(clusterId);
  }

  std::string Base::buildMigrationTaskKey(MigrationTaskIdType taskId) {
    return KEY_MIGRATION_INFO_PREFIX + std::to_string(taskId);
  }


  ResponseHeader& Base::buildHeader(ResponseHeader &header, proto::ResponseCode code, const std::string &msg) {
    header.set_code(code);
    header.set_message(msg);
    return header;
  }

  void Base::fillExistPrecondition(proto::Precondition_ExistCondition *pCond, const std::string &key, bool exists) {
    pCond->set_key(key);
    pCond->set_shouldexist(exists);
  }
}  ///  end namespace goblin::objectmanager::module
