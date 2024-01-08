/************************************************************************
Copyright 2019-2021 eBay Inc.
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

#ifndef SERVER_SRC_KV_ENGINE_CRYPTO_SECRETKEYFACTORY_H_
#define SERVER_SRC_KV_ENGINE_CRYPTO_SECRETKEYFACTORY_H_

#include <INIReader.h>
#include <infra/util/SecretKeyFactory.h>

#include "MicrovaultSecretKey.h"

namespace goblin::kvengine::crypto {

class SecretKeyFactory : public gringofts::SecretKeyFactoryDefault {
 public:
  SecretKeyFactory() = default;
  virtual ~SecretKeyFactory() = default;
  std::shared_ptr<gringofts::SecretKey> create(const INIReader &reader) const override {
    int port = reader.GetInteger("aes", "microvault.port", 10000);
    std::string keyPathOnMicrovault = reader.Get("aes", "microvault.key_path", "");

    if (!keyPathOnMicrovault.empty()) {
      return std::make_shared<MicrovaultSecretKey>(port, keyPathOnMicrovault);
    } else {
      return gringofts::SecretKeyFactoryDefault::create(reader);
    }
  }
};

}  /// namespace goblin::kvengine::crypto

#endif  // SERVER_SRC_KV_ENGINE_CRYPTO_SECRETKEYFACTORY_H_
