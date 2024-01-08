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

#include "Migration.h"

namespace goblin::objectmanager::module {
  Migration::Migration(MigrationTask migrationTask, VersionType version,
                       const std::shared_ptr<kvengine::KVEngine> &kvEngine)
          : Base(kvEngine), mMigrationTask(std::move(migrationTask)), mVersion(version) {
  }

  void Migration::fillPrecondition(proto::Transaction_Request& transReq) {
    auto cond = transReq.add_preconds()->mutable_versioncond();
    cond->set_key(buildMigrationTaskKey(getMigrationTaskId()));
    cond->set_version(getVersion());
    cond->set_op(proto::Precondition_CompareOp_EQUAL);
    SPDLOG_INFO("Migration2::fillPrecondition, add migration exist precondition, version is {}", cond->version());
  }

  void Migration::updateInTransaction(MigrationStatus newStatus, proto::Transaction_Request &transReq) {
    if (this->getStatus() == newStatus) {
      assert(0);
    }
    setStatus(newStatus);
    auto pWriteEntry = transReq.add_entries()->mutable_writeentry();
    pWriteEntry->set_key(buildMigrationTaskKey(getMigrationTaskId()));
    pWriteEntry->set_value(getMigrationTask().SerializeAsString());
  }

  proto::ResponseCode Migration::createInTransaction(
          const MigrationTask& migrationTask, proto::Transaction_Request& transReq) {
    assert(migrationTask.taskid());
    auto key = buildMigrationTaskKey(migrationTask.taskid());
    auto existCond = transReq.add_preconds()->mutable_existcond();
    existCond->set_shouldexist(false);
    existCond->set_key(key);

    auto pWriteEntry = transReq.add_entries()->mutable_writeentry();
    pWriteEntry->set_key(key);
    pWriteEntry->set_value(migrationTask.SerializeAsString());

    return proto::OK;
  }
}  //  namespace goblin::objectmanager::module
