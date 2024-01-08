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

#include <cassert>
#include <sstream>
#include <spdlog/spdlog.h>
#include "HashRing.h"


namespace goblin::objectmanager::route {

  std::unique_ptr<HashRing> HashRing::instance() {
    return std::make_unique<HashRing>();
  }

  HashRing::HashRing() : virtualNodeCount(DEFAULT_VIRTUAL_NODE_COUNT) {
    // VIRTUAL_NODE_COUNT is a configurable env in yaml
    const char * virNodeCnt = getenv("VIRTUAL_NODE_COUNT");
    if (NULL != virNodeCnt) {
      std::stringstream ss(virNodeCnt);
      int i;
      assert(ss >> i && i >= 1);
      virtualNodeCount = i;
    }
  }

  ShardIdType HashRing::calcShard(const std::string& key) {
    ShardIdType shard = mHashFunc(key) % RING_LENGTH;
    /// resolve potential hash conflict
    while (clusterRingMap.find(shard) != clusterRingMap.end()) {
      shard = (shard + 1) % RING_LENGTH;
    }
    return shard;
  }

  std::string HashRing::buildKey(ClusterIdType clusterId, int order) {
    std::string key = std::to_string(clusterId);
    if (order > 0) {
      return key.append("#").append(std::to_string(order));
    }
    return key;
  }

  void HashRing::deleteClusterId(ClusterIdType clusterId) {
    auto it = clusterRingMap.begin();
    while (it != clusterRingMap.end()) {
      if (it->second == clusterId) {
        clusterRingMap.erase(it++);
        continue;
      }
      ++it;
    }
  }

  void HashRing::addClusterId(ClusterIdType clusterId, std::map<ClusterIdType, std::vector<ShardIdType>>* pMap) {
    for (int i = 0; i < virtualNodeCount; ++i) {
      std::string key = buildKey(clusterId, i);
      ShardIdType shard = calcShard(key);

      SPDLOG_INFO("hash ring, shard is {}, key is {}, cluster is {}", shard, key, clusterId);
      clusterRingMap.insert(std::pair<ShardIdType , ClusterIdType>(shard, clusterId));
      if (clusterRingMap.size() <= virtualNodeCount) {
        continue;
      }

      if (pMap != nullptr) {
        auto& map = *pMap;
        auto it = clusterRingMap.lower_bound(shard);
        if (it == clusterRingMap.begin()) {
          it = --clusterRingMap.end();
        } else {
          --it;
        }
        if (it->second != clusterId && pMap->find(clusterId) == pMap->end()) {
          /// find the first key smaller than clusterId, parts of its shards will be taken by clusterId
          map[it->second];
        }

        auto iter = clusterRingMap.upper_bound(shard);
        if (iter == clusterRingMap.end()) {
          iter = clusterRingMap.begin();
        }
        ShardIdType start = shard, end = iter->first;
        if (end < shard) end += RING_LENGTH;
        while (start < end) {
          map[it->second].push_back(start % RING_LENGTH);
          start += 1;
        }
      }
    }
  }

  /**
   * Parameter: map, key is clusterId, value is shard array to serve
   * */
  void HashRing::extractShardConfig(std::map<ClusterIdType, std::vector<ShardIdType>>& resultMap) {
    if (clusterRingMap.empty()) return;

    auto it = clusterRingMap.begin();
    auto preKey = it->first, preClusterId = it->second;
    ++it;

    while (it != clusterRingMap.end()) {
      auto curKey = it->first;
      while (preKey < curKey) {
        if (resultMap.find(preClusterId) == resultMap.end()) {
          resultMap[preClusterId] = {};
        }
        resultMap[preClusterId].push_back(preKey);
        preKey += 1;
      }
      preClusterId = it->second;
      ++it;
    }

    if (resultMap.find(preClusterId) == resultMap.end()) {
      resultMap[preClusterId] = {};
    }
    it = clusterRingMap.begin();
    while (preKey < it->first + RING_LENGTH) {
      resultMap[preClusterId].push_back(preKey % RING_LENGTH);
      preKey += 1;
    }
  }
}  ///  end namespace goblin::objectmanager::route
