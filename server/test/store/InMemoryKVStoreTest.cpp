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

#include <memory>

#include <gtest/gtest-spi.h>

#include "../../Gringofts/src/infra/util/Util.h"
#include "../../src/kv-engine/store/RocksDBKVStore.h"
#include "../../src/kv-engine/store/ReadOnlyKVStore.h"
#include "../../src/kv-engine/store/InMemoryKVStore.h"

namespace goblin::kvengine::store {

using gringofts::Util;

class InMemoryKVStoreTest: public ::testing::Test {
 protected:
    void SetUp() override {
      Util::executeCmd("rm -rf " + mBaseDir);
      Util::executeCmd("mkdir " + mBaseDir);
      std::vector<std::string> wsNames = {mCFName};
      auto rocksdbKVStore = std::make_shared<RocksDBKVStore>(
          mWalDir,
          mDBDir,
          wsNames,
          [this](const store::KeyType &key) { return this->mCFName; });
      auto roKVStore = std::make_shared<ReadOnlyKVStore>(rocksdbKVStore);
      kvStore = std::make_unique<InMemoryKVStore>(roKVStore);
      utils::Status::assertOK(kvStore->open());
    }

    void TearDown() override {
      Util::executeCmd("rm -rf " + mBaseDir);
      utils::Status::assertOK(kvStore->close());
    }

  const std::string mCFName = "cf_inmemory_kv_store_test";
  const std::string mBaseDir = "../test/store/tempData";
  const std::string mWalDir = mBaseDir + "/rocksdb";
  const std::string mDBDir = mBaseDir + "/rocksdb";
  const std::map<KeyType, std::tuple<VersionType, ValueType, TTLType, utils::TimeType>> mData = {
    {"key1", {1, "value1", 0, 0}},
    {"key2", {2, "value2", 20, utils::TimeUtil::secondsSinceEpoch() + 20}},
    {"key3", {3, "value3", 3, utils::TimeUtil::secondsSinceEpoch() - 1}},
    {"key4", {5, "value4", 0, 0}},
  };
  std::unique_ptr<KVStore> kvStore;
};

TEST_F(InMemoryKVStoreTest, PutGetTest) {
  for (auto &[key, tuple] : mData) {
    auto &[version, value, ttl, deadline] = tuple;
    utils::Status::assertOK(kvStore->writeKV(key, value, version));
    ValueType outValue;
    TTLType outTTL;
    VersionType outVersion;
    utils::Status::assertOK(kvStore->readKV(key, &outValue, &outTTL, &outVersion));
    EXPECT_EQ(outValue, value);
    EXPECT_EQ(outVersion, version);
  }
}

TEST_F(InMemoryKVStoreTest, writeDeleteGetTest) {
  VersionType deleteRecordVersion = 0, writeRecordVersion = deleteRecordVersion + 1;

  for (auto &[key, tuple] : mData) {
    auto &[version, value, ttl, deadline] = tuple;
    writeRecordVersion = deleteRecordVersion + 1;
    // write
    utils::Status::assertOK(kvStore->writeKV(key, value, writeRecordVersion));

    // read
    ValueType outValue;
    TTLType outTTL;
    VersionType outVersion;
    utils::Status::assertOK(kvStore->readKV(key, &outValue, &outTTL, &outVersion));
    EXPECT_EQ(outValue, value);
    EXPECT_EQ(outVersion, writeRecordVersion);

    // delete
    deleteRecordVersion = writeRecordVersion + 1;
    utils::Status::assertOK(kvStore->deleteKV(key, deleteRecordVersion, outVersion));

    // read again
    auto s = kvStore->readKV(key, &outValue, &outTTL, &outVersion);
    EXPECT_TRUE(s.isNotFound());
    EXPECT_EQ(outVersion, deleteRecordVersion);
  }
}

TEST_F(InMemoryKVStoreTest, writeDeleteClearGetTest) {
  for (auto &[key, tuple] : mData) {
    auto &[version, value, ttl, deadline] = tuple;
    // write
    utils::Status::assertOK(kvStore->writeKV(key, value, version));

    // read
    ValueType outValue;
    TTLType outTTL;
    VersionType outVersion;
    utils::Status::assertOK(kvStore->readKV(key, &outValue, &outTTL, &outVersion));
    EXPECT_EQ(outValue, value);
    EXPECT_EQ(outVersion, version);

    // delete
    VersionType deleteRecordVersion = version + mData.size();
    utils::Status::assertOK(kvStore->deleteKV(key, deleteRecordVersion, outVersion));

    // clear
    kvStore->clear();

    // read again
    auto s = kvStore->readKV(key, &outValue, &outTTL, &outVersion);
    EXPECT_TRUE(s.isNotFound());
    EXPECT_EQ(outVersion, version);
  }
}

TEST_F(InMemoryKVStoreTest, PutGetTTLTest) {
  for (auto &[key, tuple] : mData) {
    auto &[version, value, ttl, deadline] = tuple;
    utils::Status::assertOK(kvStore->writeTTLKV(key, value, version, ttl, deadline));
    TTLType outTTL;
    ValueType outValue;
    VersionType outVersion;
    if (key == "key3") {
      /// found in map
      assert(kvStore->readKV(key, &outValue, &outTTL, &outVersion).isNotFound());
      /// not found in map
      assert(kvStore->readKV(key, &outValue, &outTTL, &outVersion).isNotFound());
    } else {
      utils::Status::assertOK(kvStore->readKV(key, &outValue, &outTTL, &outVersion));
      EXPECT_EQ(outValue, value);
      EXPECT_EQ(outVersion, version);
    }
  }
}

TEST_F(InMemoryKVStoreTest, PutOverrideTTLTest) {
  VersionType version = 1;
  KeyType key = "key";
  ValueType value = "value";
  TTLType ttl = 1;
  utils::TimeType deadline = ttl + utils::TimeUtil::secondsSinceEpoch();

  utils::Status::assertOK(kvStore->writeTTLKV(key, value, version, ttl, deadline));
  utils::Status::assertOK(kvStore->writeKV(key, value, version));
  ASSERT_STREQ(value.data(), "value");

  sleep(2);
  TTLType newTTL;
  ValueType newValue;
  utils::Status::assertOK(kvStore->readKV(key, &newValue, &newTTL, &version));
  ASSERT_STREQ(newValue.data(), "value") << "Override ttl to zero does not take effect";
}

TEST_F(InMemoryKVStoreTest, PutTTLOverWriteTest) {
  VersionType version = 1;
  KeyType key = "key";
  ValueType value = "value";
  TTLType ttl = 1;
  utils::TimeType deadline = ttl + utils::TimeUtil::secondsSinceEpoch();

  utils::Status::assertOK(kvStore->writeKV(key, value, version));
  utils::Status::assertOK(kvStore->writeTTLKV(key, value, version, ttl, deadline));
  ASSERT_STREQ(value.data(), "value");

  sleep(2);
  TTLType newTTL;
  ValueType newValue;
  assert(kvStore->readKV(key, &newValue, &newTTL, &version).isNotFound());
}
}  /// namespace goblin::kvengine::store
