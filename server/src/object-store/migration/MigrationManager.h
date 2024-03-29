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

#ifndef SERVER_SRC_OBJECT_STORE_MIGRATION_MIGRATIONMANAGER_H_
#define SERVER_SRC_OBJECT_STORE_MIGRATION_MIGRATIONMANAGER_H_

#include <spdlog/spdlog.h>

#include "../../../../protocols/generated/control.pb.h"
#include "../../kv-engine/store/KVStore.h"
#include "../../kv-engine/types.h"
#include "../../kv-engine/utils/Status.h"
#include "../../kv-engine/KVEngine.h"
#include "../model/ScanCommand.h"
#include "../network/ObjectStoreClient.h"
#include "../utils/ThreadPoolService.h"

namespace goblin::objectstore::migration {

class MigrationManager final {
 public:
  using MigrationReporter = std::function<void(const proto::Migration::MigrationProgress&)>;

  /// one scan task per working set
  class ScanTask final : public utils::ThreadPoolService::Task {
   public:
      ScanTask(
          MigrationManager &manager,  // NOLINT(runtime/references)
          kvengine::store::WSName wsName,
          std::vector<uint64_t> shardIds,
          std::vector<kvengine::store::KeyType> specificKeys = {});
      ~ScanTask() = default;
      void run() override;
      void onCommandFinished(std::shared_ptr<model::ScanResponse> resp);
   private:
      MigrationManager &mMigrationManager;
      kvengine::store::WSName mWSName;
      std::vector<uint64_t> mShardIds;
      std::vector<kvengine::store::KeyType> mSpecificKeys;
      static constexpr uint64_t kScanMaxBatch = 100;
  };
  /// one export task per batch in a working set
  /// generated by scan task
  class ExportTask : public utils::ThreadPoolService::Task {
   public:
      ExportTask(
          MigrationManager &manager,  // NOLINT(runtime/references)
          kvengine::store::WSName wsName,
          std::vector<uint64_t> shardIds,
          std::shared_ptr<model::ScanResponse> scanResp);
      ~ExportTask() = default;
      void run() override;
   private:
      void exportLocalKeys();
      void deleteLocalKeys();
      MigrationManager &mMigrationManager;
      kvengine::store::WSName mWSName;
      std::vector<uint64_t> mShardIds;
      std::shared_ptr<model::ScanResponse> mScanResp;
  };
  /// each shard will need to run two tasks: ScanTask and ExportTask
  /// after that, the shard can be marked as migration finished
  class MigratingShardInfo final {
   public:
      explicit MigratingShardInfo(uint64_t shardId): mShardId(shardId) {}
      ~MigratingShardInfo() = default;
      uint64_t getShardId() const { return mShardId; }
      uint64_t getScanTaskNum() const { return mScanTasks.size(); }
      uint64_t getExportTaskNum() const { return mExportTasks.size(); }
      proto::Migration::MigrationEvent getProgressEvent() const {
        proto::Migration::MigrationEvent event;
        event.mutable_migrateshard()->mutable_shardinfo()->set_id(mShardId);
        if (getExportTaskNum() != 0) {
          event.mutable_migrateshard()->set_status(proto::Migration::EXPORTING);
        } else if (getScanTaskNum() != 0) {
          event.mutable_migrateshard()->set_status(proto::Migration::SCANNING);
        } else {
          event.mutable_migrateshard()->set_status(proto::Migration::MIGRATED);
        }
        event.set_message("remaining scan: " + std::to_string(getScanTaskNum()) +
            ", remaining export: " + std::to_string(getExportTaskNum()));
        return event;
      }
      void addScanTask(std::shared_ptr<ScanTask> scanTask);
      void addExportTask(std::shared_ptr<ExportTask> exportTask);
      void markScanTaskDone(utils::TaskID taskID);
      void markExportTaskDone(utils::TaskID taskID);
      bool isDone() const;

   private:
      uint64_t mShardId;
      std::map<utils::TaskID, std::shared_ptr<ScanTask>> mScanTasks;
      std::map<utils::TaskID, std::shared_ptr<ExportTask>> mExportTasks;
      std::mutex mMutex;
  };

  MigrationManager(
       const INIReader &reader,
       kvengine::KVEngine &kvEngine,  // NOLINT(runtime/references)
       MigrationReporter repoter);

  /// forbidden copy/move
  MigrationManager(const MigrationManager&) = delete;
  MigrationManager& operator=(const MigrationManager&) = delete;

  ~MigrationManager();

  void startMigration(const proto::Migration::MigrationTask &migrationTask);
  void endMigration();

 private:
  void markScanTaskDone(uint64_t shardId, utils::TaskID taskID);
  void markExportTaskDone(uint64_t shardId, utils::TaskID taskID);

  std::mutex mMutex;
  INIReader mReader;
  uint64_t mMigrationTaskID = 0;
  std::map<uint64_t, std::unique_ptr<MigratingShardInfo>> mMigratingInfo;
  kvengine::KVEngine &mKVEngine;
  std::unique_ptr<network::ObjectStoreClient> mPeerClient;
  MigrationReporter mMigrationReporter;

  utils::ThreadPoolService mThreadPoolService;
  static constexpr uint32_t kMigrationThreadNum = 10;
};

}  /// namespace goblin::objectstore::migration

#endif  // SERVER_SRC_OBJECT_STORE_MIGRATION_MIGRATIONMANAGER_H_
