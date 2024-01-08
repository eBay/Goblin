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

#include "ObjectManager.h"

#include "../kv-engine/utils/FileUtil.h"
#include "migration/ClusterProxy.h"

namespace goblin::objectmanager {

ObjectManager::ObjectManager(const char *configPath) : mIsShutdown(false) {
  SPDLOG_INFO("current working dir: {}", kvengine::utils::FileUtil::currentWorkingDir());

  INIReader reader(configPath);
  if (reader.ParseError() < 0) {
    SPDLOG_WARN("Cannot load config file {}, exiting", configPath);
    throw std::runtime_error("Cannot load config file");
  }

  migration::ClusterProxy::instance().init(kvengine::utils::TlsUtil::parseTlsConf(reader, "tls"));

  auto wsLookupFunc = [ws = mWSName](const kvengine::store::KeyType &key) {
    return ws;
  };
  std::vector<kvengine::store::WSName> allWS = {mWSName};
  mKVEngine = std::make_shared<kvengine::KVEngine>();
  mKVEngine->Init(configPath, allWS, wsLookupFunc);

  mRequestReceiver = std::make_unique<network::RequestReceiver>(reader, mKVEngine);
}

ObjectManager::~ObjectManager() {
  SPDLOG_INFO("deleting app");
}

void ObjectManager::startRequestReceiver() {
  mRequestReceiver->run();
}

void ObjectManager::run() {
  SPDLOG_INFO("ObjectManager starts running");
  if (mIsShutdown) {
    SPDLOG_WARN("ObjectManager is already down. Will not run again.");
  } else {
    startRequestReceiver();
    mRequestReceiver->join();
  }
  SPDLOG_INFO("ObjectManager finishes running");
}

void ObjectManager::shutdown() {
  SPDLOG_INFO("Shutting down ObjectManager");
  if (mIsShutdown) {
    SPDLOG_INFO("ObjectManager is already down");
  } else {
    mRequestReceiver->shutdown();
    mKVEngine->Destroy();
  }
}

ObjectManager::ObjectManager(
    const char *configPath,
    std::shared_ptr<kvengine::KVEngine> engine) : mIsShutdown(false) {
  INIReader reader(configPath);
  if (reader.ParseError() < 0) {
    SPDLOG_WARN("Cannot load config file {}, exiting", configPath);
    throw std::runtime_error("Cannot load config file");
  }
  migration::ClusterProxy::instance().init(kvengine::utils::TlsUtil::parseTlsConf(reader, "tls"));
  /// this mock engine should be inited
  mKVEngine = engine;
  mRequestReceiver = std::make_unique<network::RequestReceiver>(reader, mKVEngine);
}

}  /// namespace goblin::objectmanager
