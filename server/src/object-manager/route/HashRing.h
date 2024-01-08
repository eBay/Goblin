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

#ifndef SERVER_SRC_OBJECT_MANAGER_ROUTE_HASHRING_H_
#define SERVER_SRC_OBJECT_MANAGER_ROUTE_HASHRING_H_

#include <map>
#include <vector>
#include <memory>
#include <set>

#include "../module/BaseTypes.h"

namespace goblin::objectmanager::route {
  using ClusterIdType = module::ClusterIdType;
  using ShardIdType = module::ShardIdType;

class HashRing {
 public:
    static std::unique_ptr<HashRing> instance();
    HashRing();
    HashRing(const HashRing &) = delete;
    HashRing &operator=(const HashRing &) = delete;
    HashRing(HashRing &&) = delete;
    HashRing &operator=(HashRing &&) = delete;
    ~HashRing() = default;

    void deleteClusterId(ClusterIdType clusterId);
    void addClusterId(ClusterIdType, std::map<ClusterIdType, std::vector<ShardIdType>>* = nullptr);
    void extractShardConfig(std::map<ClusterIdType, std::vector<ShardIdType>>&);

    std::pair<int, int> getHashRange() const { return {0, RING_LENGTH}; }

 private:
    ShardIdType calcShard(const std::string&);
    static std::string buildKey(ClusterIdType, int);
    std::hash<std::string> mHashFunc;
    std::map<ShardIdType, ClusterIdType> clusterRingMap;
    int virtualNodeCount;

    static constexpr int RING_LENGTH = 4096;
    static constexpr int DEFAULT_VIRTUAL_NODE_COUNT = 1;
};
}  /// end namespace goblin::objectmanager::route


#endif  //  SERVER_SRC_OBJECT_MANAGER_ROUTE_HASHRING_H_

