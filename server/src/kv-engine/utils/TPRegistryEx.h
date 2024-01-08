/************************************************************************
Copyright 2019-2020 eBay Inc.
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

#ifndef SERVER_SRC_KV_ENGINE_UTILS_TPREGISTRYEX_H_
#define SERVER_SRC_KV_ENGINE_UTILS_TPREGISTRYEX_H_

#include <infra/util/TestPointProcessor.h>

namespace goblin::kvengine::utils {

struct TPRegistryEx : public gringofts::TPRegistry {
  /// set enum to be larger value so that we avoid conflicts with TPRegistry
  /// format: class_method_purpose
  static constexpr gringofts::PointKey RocksDBKVStore_readKV_mockGetResult = 1000;
  static constexpr gringofts::PointKey EventApplyLoop_run_interceptApplyResult = 1001;
  /// RaftCore_electionTimeout_interceptTimeout
};

}  // namespace goblin::kvengine::utils

#endif  // SERVER_SRC_KV_ENGINE_UTILS_TPREGISTRYEX_H_
