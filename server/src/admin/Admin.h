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

#ifndef SERVER_SRC_ADMIN_ADMIN_H_
#define SERVER_SRC_ADMIN_ADMIN_H_

#include <memory>
#include <string>
#include <vector>

#include <infra/util/CryptoUtil.h>

#include "../kv-engine/store/RocksDBKVStore.h"
#include "OldRocksDBKVStore.h"

namespace goblin::admin {

class Admin final {
 public:
  // disallow copy ctor and copy assignment
  Admin(const Admin &) = delete;
  Admin &operator=(const Admin &) = delete;

  // disallow move ctor and move assignment
  Admin(Admin &&) = delete;
  Admin &operator=(Admin &&) = delete;
  static void addCluster(const std::string &config, const std::vector<std::string> &serverAddrs);

  static void loadMilestoneFromStoreDB(const std::string &config);
  static void saveMilestoneToStoreDB(const std::string &config, uint64_t milestone);

  static void getKeyFromOldStoreDB(const std::string &config, const std::string &key);
  static void getKeyFromStoreDB(const std::string &config, const std::string &key);
  static void putKVToStoreDB(
       const std::string &config,
       const std::string &key,
       const std::string &value,
       uint64_t version);

  static void migrationDB(const std::string &oldDBConfig, const std::string &newDBConfig);
  static void compareDB(const std::string &oldDBConfig, const std::string &newDBConfig);

  static void migrateSegments(
      const std::string &oldConfig,
      const std::string &newConfig);

 private:
  Admin() = default;
  ~Admin() = default;

  static std::shared_ptr<kvengine::store::RocksDBKVStore> openStoreDB(const std::string &config);
  static std::shared_ptr<kvengine::store::OldRocksDBKVStore> openOldStoreDB(const std::string &config);
  static void encryptEntry(gringofts::raft::LogEntry *entry,
      gringofts::CryptoUtil &crypto);  // NOLINT(runtime/references)
  static void decryptEntry(gringofts::raft::LogEntry *entry,
      gringofts::CryptoUtil &crypto);  // NOLINT(runtime/references)
};
}  /// namespace goblin::admin

#endif  // SERVER_SRC_ADMIN_ADMIN_H_
