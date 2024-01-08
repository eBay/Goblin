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

#ifndef SERVER_SRC_KV_ENGINE_CRYPTO_MICROVAULTSECRETKEY_H_
#define SERVER_SRC_KV_ENGINE_CRYPTO_MICROVAULTSECRETKEY_H_

#include <infra/util/SecretKey.h>

#include "microvaultclient/MicrovaultClient.h"

namespace goblin::kvengine::crypto {

class MicrovaultSecretKey : public gringofts::SecretKey {
 public:
  MicrovaultSecretKey() = delete;
  explicit MicrovaultSecretKey(int port, const std::string &keyPath);
  ~MicrovaultSecretKey() override = default;

  const unsigned char * getKeyByVersion(gringofts::SecKeyVersion version) override;

 private:
  MicrovaultClient mClient;
  const std::string mKeyPath;
};

}  /// namespace goblin::kvengine::crypto

#endif  // SERVER_SRC_KV_ENGINE_CRYPTO_MICROVAULTSECRETKEY_H_
