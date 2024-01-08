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

#include "MigrationManager.h"

#include <future>

#include "../ObjectStore.h"

namespace goblin::objectstore::migration {

MigrationManager::ScanTask::ScanTask(
    MigrationManager &manager,
    kvengine::store::WSName wsName,
    std::vector<uint64_t> shardIds,
    std::vector<kvengine::store::KeyType> specificKeys) :
  mMigrationManager(manager),
  mWSName(wsName),
  mShardIds(shardIds),
  mSpecificKeys(specificKeys) {
  SPDLOG_INFO("create a new scan task {}, ws {}, shards {}, specified {}",
      mTaskID, mWSName, mShardIds.size(), mSpecificKeys.size());
}

void MigrationManager::ScanTask::run() {
  SPDLOG_INFO("executing scan task {}", mTaskID);
  onCommandFinished(nullptr);
}

void MigrationManager::ScanTask::onCommandFinished(std::shared_ptr<model::ScanResponse> resp) {
  if (!resp) {
    /// kick off the first command
    auto newReq = std::make_shared<model::ScanRequest>(mWSName, mShardIds, nullptr, kScanMaxBatch, mSpecificKeys);
    auto newContext = std::make_shared<model::ScanCommandContext>(
        newReq, [this](std::shared_ptr<model::ScanResponse> scanResp) {
        this->onCommandFinished(scanResp);
        });
    mMigrationManager.mKVEngine.exeCustomCommand(std::make_shared<model::ScanCommand>(newContext));
  } else {
    auto exportTask = std::make_shared<ExportTask>(mMigrationManager, mWSName, mShardIds, resp);
    for (auto shardId : mShardIds) {
      mMigrationManager.mMigratingInfo[shardId]->addExportTask(exportTask);
    }
    mMigrationManager.mThreadPoolService.submit(exportTask);
    if (!resp->getIterator()->hasValue()) {
      /// iterate to the end
      for (auto shardId : mShardIds) {
        mMigrationManager.markScanTaskDone(shardId, mTaskID);
      }
    } else {
      /// create a new scan task
      auto newReq = std::make_shared<model::ScanRequest>(
          mWSName, mShardIds, resp->getIterator(), kScanMaxBatch, mSpecificKeys);
      auto newContext = std::make_shared<model::ScanCommandContext>(
          newReq, [this](std::shared_ptr<model::ScanResponse> scanResp) {
          this->onCommandFinished(scanResp);
          });
      mMigrationManager.mKVEngine.exeCustomCommand(std::make_shared<model::ScanCommand>(newContext));
    }
  }
}

MigrationManager::ExportTask::ExportTask(
    MigrationManager &manager,
    kvengine::store::WSName wsName,
    std::vector<uint64_t> shardIds,
    std::shared_ptr<model::ScanResponse> scanResp):
  mMigrationManager(manager),
  mWSName(wsName),
  mShardIds(shardIds),
  mScanResp(scanResp) {
  SPDLOG_INFO("executing export task {}, ws {}", mTaskID, mWSName);
}

void MigrationManager::ExportTask::run() {
  SPDLOG_INFO("executing export task {}", mTaskID);
  exportLocalKeys();
  deleteLocalKeys();
}

void MigrationManager::ExportTask::exportLocalKeys() {
  auto &data = mScanResp->getScannedData();
  proto::MigrateBatch::Request req;
  for (auto &tuple : data) {
    auto &[key, value, ttl, version] = tuple;
    auto entry = req.add_entries();
    entry->set_key(key);
    entry->set_value(value);
    entry->set_enablettl(ttl == kvengine::store::INFINITE_TTL ? false : true);
    entry->set_ttl(ttl);
    entry->set_version(version);
  }
  SPDLOG_INFO("exporting local keys count {}", data.size());
  /// TODO: refactor this retry strategy
  uint32_t retryCount = 0;
  while (true) {
    auto promise = std::make_shared<std::promise<proto::MigrateBatch::Response>>();
    auto future = promise->get_future();
    mMigrationManager.mPeerClient->migrateBatch(req, [promise](const proto::MigrateBatch::Response &resp) {
        promise->set_value(resp);
        });
    auto resp = future.get();
    bool isAllSucceed = true;
    for (auto result : resp.results()) {
      if (result.code() != proto::ResponseCode::OK) {
        isAllSucceed = false;
        break;
      }
    }
    if (isAllSucceed) {
      break;
    }
    retryCount++;
    SPDLOG_INFO("retry migrate batch times {}", retryCount);
  }
}

void MigrationManager::ExportTask::deleteLocalKeys() {
  proto::ExeBatch::Request req;
  auto &data = mScanResp->getScannedData();
  std::vector<kvengine::store::KeyType> originKeys;
  for (auto &tuple : data) {
    auto &[key, value, ttl, version] = tuple;
    auto entry = req.add_entries()->mutable_removeentry();
    entry->set_key(key);
    entry->set_version(version);
    originKeys.push_back(key);
  }
  SPDLOG_INFO("deleting local keys count {}", data.size());
  auto resp = mMigrationManager.mKVEngine.exeBatch(req);
  std::vector<kvengine::store::KeyType> failedKeys;
  auto i = 0;
  assert(resp.results().size() == originKeys.size());
  for (auto result : resp.results()) {
    if (result.removeresult().code() != proto::ResponseCode::OK) {
      failedKeys.push_back(originKeys[i]);
    }
    i++;
  }
  if (!failedKeys.empty()) {
    SPDLOG_INFO("failed to delete some local keys count {}", failedKeys.size());
    auto scanTask = std::make_shared<ScanTask>(mMigrationManager, mWSName, mShardIds, failedKeys);
    for (auto id : mShardIds) {
      mMigrationManager.mMigratingInfo[id]->addScanTask(scanTask);
    }
    mMigrationManager.mThreadPoolService.submit(scanTask);
  }
  for (auto id : mShardIds) {
    mMigrationManager.markExportTaskDone(id, mTaskID);
  }
}

void MigrationManager::MigratingShardInfo::addScanTask(std::shared_ptr<ScanTask> scanTask) {
  std::lock_guard<std::mutex> lock(mMutex);
  mScanTasks[scanTask->mTaskID] = scanTask;
}

void MigrationManager::MigratingShardInfo::addExportTask(std::shared_ptr<ExportTask> exportTask) {
  std::lock_guard<std::mutex> lock(mMutex);
  mExportTasks[exportTask->mTaskID] = exportTask;
}

void MigrationManager::MigratingShardInfo::markScanTaskDone(utils::TaskID taskID) {
  std::lock_guard<std::mutex> lock(mMutex);
  mScanTasks.erase(taskID);
}

void MigrationManager::MigratingShardInfo::markExportTaskDone(utils::TaskID taskID) {
  std::lock_guard<std::mutex> lock(mMutex);
  mExportTasks.erase(taskID);
}

bool MigrationManager::MigratingShardInfo::isDone() const {
  return mScanTasks.empty() && mExportTasks.empty();
}

MigrationManager::MigrationManager(
    const INIReader &reader,
    kvengine::KVEngine &kvEngine,
    MigrationReporter reporter) :
  mReader(reader),
  mThreadPoolService(kMigrationThreadNum),
  mKVEngine(kvEngine),
  mMigrationReporter(reporter) {
    SPDLOG_INFO("Migration Manager started, Concurrency={}.", kMigrationThreadNum);
}

MigrationManager::~MigrationManager() {
}

void MigrationManager::markScanTaskDone(uint64_t shardId, utils::TaskID taskID) {
  SPDLOG_INFO("scan task {} for shard {} is done", taskID, shardId);
  assert(mMigratingInfo.find(shardId) != mMigratingInfo.end());
  mMigratingInfo[shardId]->markScanTaskDone(taskID);
}

void MigrationManager::markExportTaskDone(uint64_t shardId, utils::TaskID taskID) {
  std::lock_guard<std::mutex> lock(mMutex);
  SPDLOG_INFO("export task {} for shard {} is done", taskID, shardId);
  assert(mMigratingInfo.find(shardId) != mMigratingInfo.end());
  mMigratingInfo[shardId]->markExportTaskDone(taskID);
  proto::Migration::MigrationProgress progress;
  progress.set_taskid(mMigrationTaskID);
  auto newEvent = progress.add_events();
  *newEvent = mMigratingInfo[shardId]->getProgressEvent();
  /// check if all done
  for (auto &[shardId, info] : mMigratingInfo) {
    if (!info->isDone()) {
      SPDLOG_INFO("shard {} is still migrating", shardId);
      progress.set_overallstatus(proto::Migration::RUNNING);
      mMigrationReporter(progress);
      return;
    }
  }
  SPDLOG_INFO("all shards are done, migration task {}", mMigrationTaskID);
  progress.set_overallstatus(proto::Migration::SUCCEEDED);
  mMigrationReporter(progress);
}

void MigrationManager::startMigration(const proto::Migration::MigrationTask &migrationTask) {
  mMigrationTaskID = migrationTask.taskid();
  auto peerCluster = migrationTask.targetcluster();
  mPeerClient = std::make_unique<network::ObjectStoreClient>(mReader, peerCluster);
  std::map<kvengine::store::WSName, std::vector<uint64_t>> shardsPerWS;
  for (auto shard : migrationTask.migrateshards()) {
    auto cf = ObjectStore::shardIdToColumnFamily(
        ObjectStore::kWorkingSetNamePrefix, shard.shardinfo().id(), ObjectStore::kWorkingSetFactor);
    mMigratingInfo[shard.shardinfo().id()] = std::make_unique<MigratingShardInfo>(shard.shardinfo().id());
    shardsPerWS[cf].push_back(shard.shardinfo().id());
  }
  for (auto &[wsName, shardIds] : shardsPerWS) {
    auto scanTask = std::make_shared<ScanTask>(*this, wsName, shardIds);
    for (auto id : shardIds) {
      mMigratingInfo[id]->addScanTask(scanTask);
    }
    mThreadPoolService.submit(scanTask);
  }
}

void MigrationManager::endMigration() {
  mMigratingInfo.clear();
}

}  /// namespace goblin::objectstore::migration
