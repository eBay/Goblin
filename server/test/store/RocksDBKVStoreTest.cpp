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
#include <iostream>

#include <gtest/gtest-spi.h>

#include "../../Gringofts/src/infra/util/Util.h"
#include "../../src/kv-engine/store/RocksDBKVStore.h"

namespace goblin::kvengine::store {

using gringofts::Util;

class RocksDBKVStoreTest : public ::testing::Test {
 protected:
    void SetUp() override {
      Util::executeCmd("rm -rf " + mBaseDir);
      Util::executeCmd("mkdir " + mBaseDir);
      std::vector<std::string> wsNames = {mCFName};
      mKVStore = std::make_unique<RocksDBKVStore>(
          mWalDir,
          mDBDir,
          wsNames,
          [cf = mCFName](const store::KeyType &key) { return cf; });
    }

    void TearDown() override {
      Util::executeCmd("rm -rf " + mBaseDir);
    }

  const std::string mCFName = "cf_rocksdb_kv_store_test";
  const std::string mBaseDir = "../test/store/tempData";
  const std::string mWalDir = mBaseDir + "/rocksdb";
  const std::string mDBDir = mBaseDir + "/rocksdb";
  const std::map<KeyType, std::tuple<VersionType, ValueType, TTLType, utils::TimeType>> mData = {
          {"key1", {1, "value1", 0, 0}},
          {"key2", {2, "value2", 20, utils::TimeUtil::secondsSinceEpoch() + 20}},
          {"key3", {3, "value3", 3, utils::TimeUtil::secondsSinceEpoch() - 1}},
          {"key4", {5, "value4", 0, 0}},
  };
  std::unique_ptr<RocksDBKVStore> mKVStore;
};

TEST_F(RocksDBKVStoreTest, PutGetTest) {
  for (auto &[key, tuple] : mData) {
    auto &[version, value, ttl, deadline] = tuple;
    utils::Status::assertOK(mKVStore->writeKV(key, value, version));
    utils::Status::assertOK(mKVStore->commit(version));
  }
  for (auto &[key, tuple] : mData) {
    auto &[version, value, ttl, deadline] = tuple;
    ValueType outValue;
    TTLType outTTL;
    VersionType outVersion;
    utils::Status::assertOK(mKVStore->readKV(key, &outValue, &outTTL, &outVersion));
    EXPECT_EQ(outValue, value);
    EXPECT_EQ(outVersion, version);
  }
}

TEST_F(RocksDBKVStoreTest, PutGetTTLTest) {
  for (auto &[key, tuple] : mData) {
    auto &[version, value, ttl, deadline] = tuple;
    utils::Status::assertOK(mKVStore->writeTTLKV(key, value, version, ttl, deadline));
    utils::Status::assertOK(mKVStore->commit(version));
  }
  for (auto &[key, tuple] : mData) {
    auto &[version, value, ttl, deadline] = tuple;
    ValueType outValue;
    TTLType outTTL;
    VersionType outVersion;
    if (key == "key3") {
      /// key3 is expired
      assert(mKVStore->readKV(key, &outValue, &outTTL, &outVersion).isNotFound());
    } else {
      utils::Status::assertOK(mKVStore->readKV(key, &outValue, &outTTL, &outVersion));
      EXPECT_EQ(outValue, value);
      EXPECT_EQ(outVersion, version);
    }
  }
}

TEST_F(RocksDBKVStoreTest, PutOverrideTTLTest) {
  VersionType version = 1;
  KeyType key = "key";
  ValueType value = "value";
  TTLType ttl = 1;
  utils::TimeType deadline = ttl + utils::TimeUtil::secondsSinceEpoch();

  utils::Status::assertOK(mKVStore->writeTTLKV(key, value, version, ttl, deadline));
  utils::Status::assertOK(mKVStore->writeKV(key, value, version));
  mKVStore->commit(version);
  ASSERT_STREQ(value.data(), "value");

  sleep(2);
  TTLType newTTL;
  ValueType newValue;
  utils::Status::assertOK(mKVStore->readKV(key, &newValue, &newTTL, &version));
  ASSERT_STREQ(newValue.data(), "value") << "Override ttl to zero does not take effect";
}

TEST_F(RocksDBKVStoreTest, CompactionFilterTest) {
    for (auto &[key, tuple] : mData) {
      auto &[version, value, ttl, deadline] = tuple;
      utils::Status::assertOK(mKVStore->writeTTLKV(key, value, version, ttl, deadline));
      utils::Status::assertOK(mKVStore->commit(version));
    }

    rocksdb::Slice key("key3");
    RocksDBKVStore::RawValueType raw;
    assert(mKVStore->mRocksDB->Get(mKVStore->mReadOptions, mKVStore->findCFHandle(mCFName), key, &raw).ok());

    utils::Status::assertOK(mKVStore->testCompact());
    auto s = mKVStore->mRocksDB->Get(mKVStore->mReadOptions, mKVStore->findCFHandle(mCFName), key, &raw);
    assert(s.IsNotFound());
}

TEST_F(RocksDBKVStoreTest, PutTTLOverWriteTest) {
  VersionType version = 1;
  KeyType key = "key";
  ValueType value = "value";
  TTLType ttl = 1;
  utils::TimeType deadline = ttl + utils::TimeUtil::secondsSinceEpoch();

  utils::Status::assertOK(mKVStore->writeKV(key, value, version));
  utils::Status::assertOK(mKVStore->writeTTLKV(key, value, version, ttl, deadline));
  ASSERT_STREQ(value.data(), "value");

  sleep(2);
  TTLType newTTL;
  ValueType newValue;
  assert(mKVStore->readKV(key, &newValue, &newTTL, &version).isNotFound());
}

TEST_F(RocksDBKVStoreTest, SizeTest) {
  for (int i = 0; i < 10; ++i) {
    utils::Status::assertOK(mKVStore->writeKV(std::to_string(i), std::to_string(i), i));
  }
  utils::Status::assertOK(mKVStore->commit(9));

  uint64_t estimatedSize = mKVStore->size();
  SPDLOG_INFO("Estimated size of rocksdb kv is: {}", estimatedSize);
  EXPECT_EQ(10, estimatedSize);
}

TEST_F(RocksDBKVStoreTest, ReportWriteMetricsTest) {
  uint64_t keySize = 1, valueSize = 1;
  for (int i = 0; i <= 4; ++i) {
    keySize *= 2;
    valueSize *= 10;
    mKVStore->writeKV(std::string(keySize, 'a'), std::string(valueSize, 'b'), i);
    mKVStore->commit(i);
  }
  keySize = 1, valueSize = 1;
  for (int i = 0; i < 4; ++i) {
    keySize *= 2;
    valueSize *= 10;
    std::string keySizeLabel = std::to_string(keySize);
    std::string valueSizeLabel = std::to_string(valueSize);
    EXPECT_NE(0, gringofts::getCounter("write_key_size", {{"bucket_ceil", keySizeLabel}}).value());
    EXPECT_NE(0, gringofts::getCounter("write_value_size", {{"bucket_ceil", valueSizeLabel}}).value());
  }
}

}  /// namespace goblin::kvengine::store
