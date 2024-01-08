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
#include <thread>

#include <gtest/gtest-spi.h>
#include <gmock/gmock.h>

#include "../../src/kv-engine/strategy/CacheEviction.h"

namespace goblin::kvengine::strategy {

using CommandPtr = std::shared_ptr<model::Command>;

class MockExecutionService : public execution::ExecutionService<CommandPtr> {
 public:
  MOCK_METHOD0(start, void());
  MOCK_METHOD0(shutdown, void());
  MOCK_METHOD1(submit, void(const CommandPtr &));
};

class MockEventApplyService : public execution::EventApplyService {
 public:
  MOCK_CONST_METHOD0(getLastAppliedVersion, store::VersionType());
};

class MockKVStore : public store::KVStore {
 public:
  MOCK_METHOD0(open, utils::Status());
  MOCK_METHOD0(close, utils::Status());
  MOCK_METHOD6(writeKV,
               utils::Status(const store::KeyType &key,
                 const store::ValueType &value,
                 const store::VersionType &version,
                 const utils::TimeType &updateTime,
                 const goblin::proto::UserDefinedMeta &udfMeta,
                 store::WSLookupFunc wsLookup));
  MOCK_METHOD8(writeTTLKV, utils::Status(const store::KeyType &key,
      const store::ValueType &value,
      const store::VersionType &version,
      const store::TTLType &ttl,
      const utils::TimeType &deadline,
      const utils::TimeType &updateTime,
      const goblin::proto::UserDefinedMeta &udfMeta,
      store::WSLookupFunc wsLookup));
  MOCK_METHOD5(readKV,
                utils::Status(const store::KeyType &key,
                store::ValueType *outValue,
                store::TTLType *outTTL,
                store::VersionType * outVersion,
                store::WSLookupFunc wsLookup));
  MOCK_METHOD4(deleteKV, utils::Status(const store::KeyType &key,
        const store::VersionType &deleteRecordVersion, const store::VersionType &deletedVersion,
        store::WSLookupFunc wsLookup));
  MOCK_METHOD3(evictKV, utils::Status(const std::set<store::KeyType> &keys,
        store::VersionType guardVersion, store::WSLookupFunc wsLookup));
  MOCK_METHOD3(readMeta, utils::Status(const store::KeyType &key,
        goblin::proto::Meta *meta, store::WSLookupFunc wsLookup));
  MOCK_METHOD2(commit, utils::Status(const store::MilestoneType &milestone, store::WSLookupFunc wsLookup));
  MOCK_METHOD2(loadMilestone, utils::Status(store::MilestoneType * milestone, store::WSLookupFunc wsLookup));
  MOCK_METHOD0(clear, void());
};

class CacheEvictionTest : public ::testing::Test {
};

TEST_F(CacheEvictionTest, lruTest) {
  MockExecutionService execution_service;
  MockEventApplyService event_apply_service;
  std::shared_ptr<MockKVStore> kvStorePtr = std::make_shared<MockKVStore>();
  EXPECT_CALL(event_apply_service, getLastAppliedVersion()).Times(1).WillOnce(::testing::Return(100));
  EXPECT_CALL(execution_service, submit(::testing::_)).WillOnce(::testing::Invoke(
      [kvStorePtr](CommandPtr ptr) {
        ptr->finish(kvStorePtr, {});
      }));
  EXPECT_CALL(*kvStorePtr, readKV(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillRepeatedly(
      ::testing::Invoke(
          [](const store::KeyType &key, store::ValueType *value,
            store::TTLType *ttl, store::VersionType *version, store::WSLookupFunc wsLookup) {
            *version = 10;
            return utils::Status::ok();
          }));
  EXPECT_CALL(*kvStorePtr, evictKV(::testing::_, ::testing::_, ::testing::_)).WillRepeatedly(
      ::testing::Return(utils::Status::ok()));
  std::shared_ptr<LRUEviction> lru = std::make_shared<LRUEviction>(
      event_apply_service,
      execution_service,
      10, 10, 1);
  // write 3
  lru->onWriteValue("1", 1);
  lru->onWriteValue("2", 2);
  lru->onWriteValue("3", 3);
  // read 2
  lru->onReadValue("2");
  lru->onWriteValue("4", 5);
  // will remove 1
}

TEST_F(CacheEvictionTest, lruTest2) {
  MockExecutionService execution_service;
  MockEventApplyService event_apply_service;
  std::shared_ptr<MockKVStore> kvStorePtr = std::make_shared<MockKVStore>();
  EXPECT_CALL(event_apply_service, getLastAppliedVersion()).Times(1).WillOnce(::testing::Return(100));
  EXPECT_CALL(execution_service, submit(::testing::_)).WillOnce(::testing::Invoke(
      [kvStorePtr](CommandPtr ptr) {
        ptr->finish(kvStorePtr, {});
      }));
  EXPECT_CALL(*kvStorePtr, readKV(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillRepeatedly(
      ::testing::Invoke(
          [](const store::KeyType &key, store::ValueType *value,
            store::TTLType *ttl, store::VersionType *version, store::WSLookupFunc wsLookup) {
            // version bigger than 100, won't be remove
            *version = 110;
            return utils::Status::ok();
          }));
  EXPECT_CALL(*kvStorePtr, evictKV(::testing::_, ::testing::_, ::testing::_)).WillRepeatedly(
      ::testing::Return(utils::Status::ok()));
  std::shared_ptr<LRUEviction> lru = std::make_shared<LRUEviction>(
      event_apply_service,
      execution_service,
      10, 10, 1);
  // write 3
  lru->onWriteValue("1", 1);
  lru->onWriteValue("2", 2);
  lru->onWriteValue("3", 3);
  // read 2
  lru->onReadValue("2");
  lru->onWriteValue("4", 5);
  // will remove 1
}

TEST_F(CacheEvictionTest, lruTest3) {
  MockExecutionService execution_service;
  MockEventApplyService event_apply_service;
  std::shared_ptr<MockKVStore> kvStorePtr = std::make_shared<MockKVStore>();
  EXPECT_CALL(event_apply_service, getLastAppliedVersion()).Times(1).WillOnce(::testing::Return(100));
  EXPECT_CALL(execution_service, submit(::testing::_)).WillOnce(::testing::Invoke(
      [kvStorePtr](CommandPtr ptr) {
        ptr->finish(kvStorePtr, {});
      }));
  EXPECT_CALL(*kvStorePtr, readKV(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillRepeatedly(
      ::testing::Invoke(
          [](const store::KeyType &key, store::ValueType *value,
            store::TTLType *ttl, store::VersionType *version, store::WSLookupFunc wsLookup) {
            // version bigger than 100, won't be remove
            *version = 10;
            return utils::Status::ok();
          }));
  EXPECT_CALL(*kvStorePtr, evictKV(::testing::_, ::testing::_, ::testing::_)).WillRepeatedly(
      ::testing::Return(utils::Status::ok()));
  std::shared_ptr<LRUEviction> lru = std::make_shared<LRUEviction>(
      event_apply_service,
      execution_service,
      10, 7, 1);
  // write 3
  lru->onWriteValue("1", 1);
  lru->onWriteValue("2", 2);
  lru->onWriteValue("3", 3);
  // read 2
  lru->onReadValue("2");
  lru->onWriteValue("4", 5);
  // will remove 1
}
}  // namespace goblin::kvengine::strategy
