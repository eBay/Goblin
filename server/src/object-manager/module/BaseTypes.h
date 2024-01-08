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

#ifndef SERVER_SRC_OBJECT_MANAGER_MODULE_BASETYPES_H_
#define SERVER_SRC_OBJECT_MANAGER_MODULE_BASETYPES_H_

#include <cstdint>

namespace goblin::objectmanager::module {

  using ClusterIdType = uint32_t;
  using ClusterVersionType = uint64_t;
  using VersionType = uint64_t;
  using TaskVersionType = uint64_t;
  using ShardIdType = uint32_t;
  using MigrationTaskIdType = uint64_t;
  using RouteVersionType = uint64_t;
}  //  namespace goblin::objectmanager::module

#endif  //  SERVER_SRC_OBJECT_MANAGER_MODULE_BASETYPES_H_
