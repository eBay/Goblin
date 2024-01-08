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

#include "MicrovaultSecretKey.h"

#include "../utils/TimeUtil.h"

namespace goblin::kvengine::crypto {

MicrovaultSecretKey::MicrovaultSecretKey(int port, const std::string &keyPath) :
    mClient(port), mKeyPath(keyPath) {
  int latestVersion = kInvalidSecKeyVersion;
  grpc::Status s = mClient.getLatestVersion(mKeyPath, &latestVersion);
  assert(s.ok());
  assert(latestVersion >= kOldestSecKeyVersion);

  // clear
  mAllKeys.reserve(latestVersion + 5);
  mAllKeys.resize(latestVersion + 1);
  mLatestVersion = latestVersion;

  for (gringofts::SecKeyVersion v = kOldestSecKeyVersion; v <= mLatestVersion; ++v) {
    std::string encodedKey;
    grpc::Status s = mClient.getSecretByVersion(mKeyPath, v, &encodedKey);
    assert(s.ok());
    decodeBase64Key(encodedKey, mAllKeys[v].data(), kKeyLen);
  }

  mLatestVersionGauge.set(mLatestVersion);
  SPDLOG_INFO("{} AES keys fetched from microvault, the latest version is {}", mAllKeys.size() - 1, mLatestVersion);
}

const unsigned char * MicrovaultSecretKey::getKeyByVersion(gringofts::SecKeyVersion version) {
  if (version > mLatestVersion) {
    auto startTime = utils::TimeUtil::currentTimeInNanos();
    mAllKeys.resize(version + 1);
    for (gringofts::SecKeyVersion v = mLatestVersion + 1; v <= version; ++v) {
      std::string encodedKey;
      grpc::Status s = mClient.getSecretByVersion(mKeyPath, v, &encodedKey);
      if (!s.ok()) {
        SPDLOG_ERROR("Invalid key version: {}", v);
        exit(1);
      }
      decodeBase64Key(encodedKey, mAllKeys[v].data(), kKeyLen);
      SPDLOG_INFO("AES key version {} has fetched from microvault", v);
    }
    auto endTime = utils::TimeUtil::currentTimeInNanos();
    SPDLOG_INFO("It takes {} ms to fetch {} AES keys from microvault",
                  (endTime - startTime) / 1000000, version - mLatestVersion);
    mLatestVersion = version;
    mLatestVersionGauge.set(mLatestVersion);
  }

  return mAllKeys[version].data();
}

}  // namespace goblin::kvengine::crypto
