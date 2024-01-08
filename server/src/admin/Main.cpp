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

#include <assert.h>
#include <iostream>
#include <string>
#include <vector>

#include "Admin.h"

void printUsage() {
  std::cout << ""                                                                     << std::endl;
  std::cout << "Admin sub-command ..."                                                << std::endl;
  std::cout << ""                                                                     << std::endl;
  std::cout << "Admin addCluster clusterConfigPath server1 server2, server3"          << std::endl;
  std::cout << "    send addCluster request to object manager."                       << std::endl;
  std::cout << ""                                                                     << std::endl;
  std::cout << "Admin loadMilestoneFromStore clusterConfigPath"                       << std::endl;
  std::cout << "    directly load milestone from object store."                       << std::endl;
  std::cout << ""                                                                     << std::endl;
  std::cout << "Admin saveMilestoneToStore clusterConfigPath milestone"               << std::endl;
  std::cout << "    directly save milestone to object store."                         << std::endl;
  std::cout << ""                                                                     << std::endl;
  std::cout << "Admin getKeyFromOldStore clusterConfigPath key"                       << std::endl;
  std::cout << "    directly read key from object store."                             << std::endl;
  std::cout << ""                                                                     << std::endl;
  std::cout << "Admin getKeyFromStore clusterConfigPath key"                          << std::endl;
  std::cout << "    directly read key from object store."                             << std::endl;
  std::cout << ""                                                                     << std::endl;
  std::cout << "Admin putKVToStore clusterConfigPath key value version"               << std::endl;
  std::cout << "    directly put kv to object store."                                 << std::endl;
  std::cout << ""                                                                     << std::endl;
  std::cout << "Admin migrationDB clusterConfigPath newClusterConfigPath"             << std::endl;
  std::cout << "    read old db and write to new db"                                  << std::endl;
  std::cout << ""                                                                     << std::endl;
  std::cout << "Admin compareDB clusterConfigPath newClusterConfigPath"               << std::endl;
  std::cout << "    compare old db and write to new db"                               << std::endl;
  std::cout << ""                                                                     << std::endl;
  std::cout << "Admin migrationSegments oldConfig newConfig"                          << std::endl;
  std::cout << "    read old segments and write to new segments"                      << std::endl;
  std::cout << ""                                                                     << std::endl;
  std::cout << ""                                                                     << std::endl;
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printUsage();
    return 0;
  }

  std::string subCommand  = argv[1];

  if (subCommand == "addCluster") {
    assert(argc > 3);
    std::vector<std::string> serverAddrs;
    auto serverCnt = argc - 3;
    for (auto i = 0; i < serverCnt; ++i) {
      serverAddrs.push_back(argv[3 + i]);
    }
    std::string configPath  = argv[2];

    goblin::admin::Admin::addCluster(configPath, serverAddrs);
  } else if (subCommand == "loadMilestoneFromStore") {
    assert(argc == 3);
    std::string configPath  = argv[2];

    goblin::admin::Admin::loadMilestoneFromStoreDB(configPath);
  } else if (subCommand == "saveMilestoneToStore") {
    assert(argc == 4);
    std::string configPath  = argv[2];
    uint64_t milestone = std::stoull(argv[3]);

    goblin::admin::Admin::saveMilestoneToStoreDB(configPath, milestone);
  } else if (subCommand == "getKeyFromOldStore") {
    assert(argc == 4);
    std::string configPath  = argv[2];
    std::string key = argv[3];

    goblin::admin::Admin::getKeyFromOldStoreDB(configPath, key);
  } else if (subCommand == "getKeyFromStore") {
    assert(argc == 4);
    std::string configPath  = argv[2];
    std::string key = argv[3];

    goblin::admin::Admin::getKeyFromStoreDB(configPath, key);
  } else if (subCommand == "putKVToStore") {
    assert(argc == 6);
    std::string configPath  = argv[2];
    std::string key = argv[3];
    std::string value = argv[4];
    uint64_t version = std::stoull(argv[5]);

    goblin::admin::Admin::putKVToStoreDB(configPath, key, value, version);
  } else if (subCommand == "migrationDB") {
    assert(argc == 4);
    std::string oldConfig = argv[2];
    std::string newConfig = argv[3];

    goblin::admin::Admin::migrationDB(oldConfig, newConfig);
  } else if (subCommand == "compareDB") {
    assert(argc == 4);
    std::string oldConfig = argv[2];
    std::string newConfig = argv[3];

    goblin::admin::Admin::compareDB(oldConfig, newConfig);
  } else if (subCommand == "migrationSegments") {
    assert(argc == 4);
    std::string oldConfig = argv[2];
    std::string newConfig = argv[3];

    goblin::admin::Admin::migrateSegments(oldConfig, newConfig);
  } else {
    printUsage();
  }
}

