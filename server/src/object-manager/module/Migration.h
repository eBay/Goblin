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

#ifndef SERVER_SRC_OBJECT_MANAGER_MODULE_MIGRATION_H_
#define SERVER_SRC_OBJECT_MANAGER_MODULE_MIGRATION_H_

#include "Base.h"

namespace goblin::objectmanager::module {
class Migration final : public Base {
 public:
    Migration(MigrationTask, VersionType, const std::shared_ptr<kvengine::KVEngine>&);
    void fillPrecondition(proto::Transaction_Request&);
    const MigrationTask& getMigrationTask() const { return mMigrationTask; }
    void updateInTransaction(MigrationStatus, proto::Transaction_Request&);

    static proto::ResponseCode createInTransaction(const MigrationTask&, proto::Transaction_Request&);
 private:
    VersionType getVersion() const { return mVersion; }
    MigrationStatus getStatus() const { return mMigrationTask.overallstatus(); }
    void setStatus(MigrationStatus newStatus) { mMigrationTask.set_overallstatus(newStatus); }
    MigrationTaskIdType getMigrationTaskId() const { return mMigrationTask.taskid(); }

    MigrationTask mMigrationTask;
    VersionType mVersion;
};
}  ///  end namespace goblin::objectmanager::module

#endif  //  SERVER_SRC_OBJECT_MANAGER_MODULE_MIGRATION_H_
