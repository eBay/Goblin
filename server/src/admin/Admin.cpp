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

#include "Admin.h"

#include <future>
#include <iostream>

#include <infra/raft/storage/SegmentLog.h>

#include "../object-store/ObjectStore.h"
#include "../object-store/network/ObjectManagerClient.h"
#include "../kv-engine/crypto/SecretKeyFactory.h"

namespace goblin::admin {

void Admin::addCluster(const std::string &config, const std::vector<std::string> &serverAddrs) {
  SPDLOG_INFO("initializing object manager client");

  auto omClient = std::make_unique<objectstore::network::ObjectManagerClient>(config);
  proto::AddCluster::Request req;
  for (auto server : serverAddrs) {
    auto s = req.mutable_cluster()->add_servers();
    s->set_hostname(server);
    SPDLOG_INFO("manually adding cluster {}", server);
  }
  auto promise = std::make_shared<std::promise<proto::AddCluster::Response>>();
  auto future = promise->get_future();
  omClient->addCluster(req, [promise](const proto::AddCluster::Response &resp) {
      promise->set_value(resp);
      });
  auto resp = future.get();
  if (resp.header().code() != proto::ResponseCode::OK) {
    SPDLOG_ERROR("Add cluster failed. error code: {}, message: {}", resp.header().code(), resp.header().message());
  }
}

std::shared_ptr<kvengine::store::OldRocksDBKVStore> Admin::openOldStoreDB(const std::string &config) {
  std::vector<kvengine::store::WSName> allWS;
  auto wsNum = 1 << objectstore::ObjectStore::kWorkingSetFactor;
  for (auto i = 0; i < wsNum; ++i) {
    allWS.push_back(objectstore::ObjectStore::kWorkingSetNamePrefix + std::to_string(i));
  }
  auto wsLookupFunc = [](const kvengine::store::KeyType &key) {
    auto shardId = objectstore::ObjectStore::keyToShardId(key);
    auto wsName = objectstore::ObjectStore::shardIdToColumnFamily(
        objectstore::ObjectStore::kWorkingSetNamePrefix, shardId, objectstore::ObjectStore::kWorkingSetFactor);
    return wsName;
  };
  INIReader reader(config);
  const auto &walDir = reader.Get("rocksdb", "wal.dir", "");
  const auto &dbDir = reader.Get("rocksdb", "db.dir", "");
  auto db = std::make_shared<kvengine::store::OldRocksDBKVStore>(walDir, dbDir, allWS, wsLookupFunc);
  return db;
}

std::shared_ptr<kvengine::store::RocksDBKVStore> Admin::openStoreDB(const std::string &config) {
  std::vector<kvengine::store::WSName> allWS;
  auto wsNum = 1 << objectstore::ObjectStore::kWorkingSetFactor;
  for (auto i = 0; i < wsNum; ++i) {
    allWS.push_back(objectstore::ObjectStore::kWorkingSetNamePrefix + std::to_string(i));
  }
  auto wsLookupFunc = [](const kvengine::store::KeyType &key) {
    auto shardId = objectstore::ObjectStore::keyToShardId(key);
    auto wsName = objectstore::ObjectStore::shardIdToColumnFamily(
        objectstore::ObjectStore::kWorkingSetNamePrefix, shardId, objectstore::ObjectStore::kWorkingSetFactor);
    return wsName;
  };
  INIReader reader(config);
  const auto &walDir = reader.Get("rocksdb", "wal.dir", "");
  const auto &dbDir = reader.Get("rocksdb", "db.dir", "");
  auto db = std::make_shared<kvengine::store::RocksDBKVStore>(walDir, dbDir, allWS, wsLookupFunc);
  return db;
}

void Admin::loadMilestoneFromStoreDB(const std::string &config) {
  auto db = openStoreDB(config);
  kvengine::store::MilestoneType milestone;
  auto s = db->loadMilestone(&milestone);
  if (s.isOK()) {
    SPDLOG_INFO("milestone {}", milestone);
  } else {
    SPDLOG_WARN("failed to load milestone {}", s.getDetail());
  }
}
void Admin::saveMilestoneToStoreDB(const std::string &config, uint64_t milestone) {
  auto db = openStoreDB(config);
  auto s = db->commit(milestone);
  if (s.isOK()) {
    SPDLOG_INFO("persisted milestone {}", milestone);
  } else {
    SPDLOG_WARN("failed to load milestone {}", s.getDetail());
  }
}

void Admin::getKeyFromStoreDB(const std::string &config, const std::string &key) {
  auto db = openStoreDB(config);
  kvengine::store::ValueType value;
  kvengine::store::TTLType ttl = kvengine::store::INFINITE_TTL;
  kvengine::store::VersionType version = kvengine::store::VersionStore::kInvalidVersion;
  auto s = db->readKV(key, &value, &ttl, &version);
  if (s.isOK()) {
    SPDLOG_INFO("key {}, value {}, ttl {}, version {}", key, value, ttl, version);
  } else {
    SPDLOG_WARN("failed to read {}", s.getDetail());
  }
}

void Admin::getKeyFromOldStoreDB(const std::string &config, const std::string &key) {
  auto db = openOldStoreDB(config);
  kvengine::store::ValueType value;
  kvengine::store::TTLType ttl = kvengine::store::INFINITE_TTL;
  kvengine::store::VersionType version = kvengine::store::VersionStore::kInvalidVersion;
  auto s = db->readKV(key, &value, &ttl, &version);
  if (s.isOK()) {
    SPDLOG_INFO("key {}, value {}, ttl {}, version {}", key, value, ttl, version);
  } else {
    SPDLOG_WARN("failed to read {}", s.getDetail());
  }
}

void Admin::putKVToStoreDB(
    const std::string &config,
    const std::string &key,
    const std::string &value,
    uint64_t version) {
  auto db = openStoreDB(config);
  auto s = db->writeKV(key, value, version);
  if (s.isOK()) {
    SPDLOG_INFO("key {}, value {}, version {}", key, value, version);
  } else {
    SPDLOG_WARN("failed to write {}", s.getDetail());
  }
}

void Admin::migrationDB(const std::string &oldDBConfig, const std::string &newDBConfig) {
  auto oldDB = openOldStoreDB(oldDBConfig);
  auto newDB = openStoreDB(newDBConfig);

  std::vector<kvengine::store::WSName> allWS;
  auto wsNum = 1 << objectstore::ObjectStore::kWorkingSetFactor;
  for (auto i = 0; i < wsNum; ++i) {
    allWS.push_back(objectstore::ObjectStore::kWorkingSetNamePrefix + std::to_string(i));
  }
  SPDLOG_INFO("take snapshot");
  auto snapshot = oldDB->takeSnapshot();
  auto curTime = kvengine::utils::TimeUtil::currentTimeInNanos();
  for (auto ws : allWS) {
    SPDLOG_INFO("iterating {}", ws);
    auto iterator = snapshot->newIterator(ws);
    iterator->seekToBegin();
    while (iterator->hasValue()) {
      std::tuple<
        kvengine::store::KeyType,
        kvengine::store::ValueType,
        kvengine::store::TTLType,
        kvengine::store::VersionType> tuple;
      auto s = iterator->get(tuple);
      if (!s.isOK()) {
        continue;
      } else {
        auto &[key, value, ttl, version] = tuple;
        std::cout << key << "," << version << std::endl;
        if (ttl == kvengine::store::INFINITE_TTL) {
          newDB->writeKV(key, value, version);
        } else {
          newDB->writeTTLKV(key, value, version, ttl, curTime + ttl);
        }
      }
      iterator->next();
    }
  }
  uint64_t milestone = 0;
  auto s = oldDB->loadMilestone(&milestone);
  assert(s.isOK());
  std::cout << "loaded milestone " << milestone << std::endl;
  s = newDB->commit(milestone);
  assert(s.isOK());
}

void Admin::compareDB(const std::string &oldDBConfig, const std::string &newDBConfig) {
  auto oldDB = openStoreDB(oldDBConfig);
  auto newDB = openStoreDB(newDBConfig);

  std::vector<kvengine::store::WSName> allWS;
  auto wsNum = 1 << objectstore::ObjectStore::kWorkingSetFactor;
  for (auto i = 0; i < wsNum; ++i) {
    allWS.push_back(objectstore::ObjectStore::kWorkingSetNamePrefix + std::to_string(i));
  }
  auto oldSnapshot = oldDB->takeSnapshot();
  auto oldSize = oldSnapshot->size();
  auto newSnapshot = newDB->takeSnapshot();
  auto newSize = newSnapshot->size();
  auto minSize = std::min(oldSize, newSize);
  SPDLOG_INFO("size of two db, old: {}, new: {}", oldSize, newSize);

  for (auto ws : allWS) {
    SPDLOG_INFO("iterating {}", ws);
    auto oldIt = oldSnapshot->newIterator(ws);
    auto newIt = newSnapshot->newIterator(ws);
    oldIt->seekToBegin();
    newIt->seekToBegin();
    while (oldIt->hasValue() && newIt->hasValue()) {
      std::tuple<
        kvengine::store::KeyType,
        kvengine::store::ValueType,
        kvengine::store::TTLType,
        kvengine::store::VersionType> oldTuple;
      std::tuple<
        kvengine::store::KeyType,
        kvengine::store::ValueType,
        kvengine::store::TTLType,
        kvengine::store::VersionType> newTuple;
      auto s = oldIt->get(oldTuple);
      assert(s.isOK());
      s = newIt->get(newTuple);
      assert(s.isOK());
      if (oldTuple != newTuple) {
        auto &[oldKey, oldValue, oldTTL, oldVersion] = oldTuple;
        auto &[newKey, newValue, newTTL, newVersion] = newTuple;
        SPDLOG_INFO("unequal, old: {}, new: {}", oldKey, newKey);
      }
      oldIt->next();
      newIt->next();
    }
  }
}

void Admin::migrateSegments(
    const std::string &oldConfig,
    const std::string &newConfig) {
  INIReader oldReader(oldConfig);
  auto oldCrypto = std::make_shared<gringofts::CryptoUtil>();
  oldCrypto->init(oldReader, kvengine::crypto::SecretKeyFactory());
  auto oldRaftConfig = oldReader.Get("store", "raft.config.path", "");
  INIReader oldRaftReader(oldRaftConfig);
  auto oldDataSizeLimit = oldRaftReader.GetInteger("raft.storage", "segment.data.size.limit", 0);
  auto oldMetaSizeLimit = oldRaftReader.GetInteger("raft.storage", "segment.meta.size.limit", 0);
  auto oldLogDir = oldRaftReader.Get("raft.storage", "storage.dir", "");
  auto oldSegmentLog = std::make_shared<gringofts::storage::SegmentLog>
    (oldLogDir, oldCrypto, oldDataSizeLimit, oldMetaSizeLimit);

  INIReader newReader(newConfig);
  auto newCrypto = std::make_shared<gringofts::CryptoUtil>();
  newCrypto->init(newReader, kvengine::crypto::SecretKeyFactory());
  auto newRaftConfig = newReader.Get("store", "raft.config.path", "");
  INIReader newRaftReader(newRaftConfig);
  auto newDataSizeLimit = newRaftReader.GetInteger("raft.storage", "segment.data.size.limit", 0);
  auto newMetaSizeLimit = newRaftReader.GetInteger("raft.storage", "segment.meta.size.limit", 0);
  auto newLogDir = newRaftReader.Get("raft.storage", "storage.dir", "");
  auto newSegmentLog = std::make_shared<gringofts::storage::SegmentLog>
    (newLogDir, newCrypto, newDataSizeLimit, newMetaSizeLimit);

  auto start = oldSegmentLog->getFirstLogIndex();
  auto end = oldSegmentLog->getLastLogIndex();
  auto latestVersion = newCrypto->getLatestSecKeyVersion();
  SPDLOG_INFO("migrating segment from index {} to {} using key version {}",
      start, end, latestVersion);
  for (auto i = start; i <= end; ++i) {
    gringofts::raft::LogEntry entry;
    oldSegmentLog->getEntry(i, &entry);
    decryptEntry(&entry, *oldCrypto);
    encryptEntry(&entry, *newCrypto);
    newSegmentLog->appendEntry(entry);
  }
}

void Admin::decryptEntry(gringofts::raft::LogEntry *entry, gringofts::CryptoUtil &crypto) {
  if (entry->noop()) {
    return;
  }
  if (crypto.isEnabled()) {
    if (entry->version().secret_key_version() == gringofts::SecretKey::kInvalidSecKeyVersion) {
      assert(crypto.decrypt(entry->mutable_payload(), gringofts::SecretKey::kOldestSecKeyVersion) == 0);
    } else {
      assert(crypto.decrypt(entry->mutable_payload(), entry->version().secret_key_version()) == 0);
    }
  }
}

void Admin::encryptEntry(gringofts::raft::LogEntry *entry, gringofts::CryptoUtil &crypto) {
  if (entry->noop()) {
    return;
  }
    /// in-place encryption
  auto latestVersion = crypto.getLatestSecKeyVersion();
  entry->mutable_version()->set_secret_key_version(latestVersion);
  assert(crypto.encrypt(
          entry->mutable_payload(),
          latestVersion) == 0);
}

}  /// namespace goblin::admin
