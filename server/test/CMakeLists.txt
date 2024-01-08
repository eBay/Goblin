project(test)

# Sources
set(UNIT_TEST_SRC
        TestRunner.cc
        ../Gringofts/test/infra/raft/v2/ClusterTestUtil.cpp
        ../Gringofts/test/test_util/SyncPointProcessor.cpp
        execution/ExecutionServiceTest.cpp
        execution/LoopTest.cpp
        execution/QueueWorkerTest.cpp
        execution/CommandProcessLoopTest.cpp
        mock/MockAppClient.cpp
        mock/MockNetAdminClient.cpp
        model/TTLCommandTest.cpp
        model/BasicCommandTest.cpp
        model/ErrorCaseCommandTest.cpp
        model/LeadershipCommandTest.cpp
        model/TransCommandTest.cpp
        model/UDFTransCommandTest.cpp
        model/FollowerReadCommandTest.cpp
        store/RocksDBKVStoreTest.cpp
        store/InMemoryKVStoreTest.cpp
        store/CacheEvictionTest.cpp
        network/NetAdminServiceTest.cpp)

# unit-test on googletest
# TODO: use our own gtest third_party ?
include_directories(BEFORE ../Gringofts/third_party/gtest/googletest)
include_directories(BEFORE ../Gringofts/third_party/gtest/googletest/include)
# add gmock
include_directories(BEFORE ../Gringofts/third_party/gtest/googlemock)
include_directories(BEFORE ../Gringofts/third_party/gtest/googlemock/include)

# Executable
add_executable(TestRunner
        ${UNIT_TEST_SRC}
        ../Gringofts/third_party/gtest/googlemock/src/gmock-all.cc
        ../Gringofts/third_party/gtest/googletest/src/gtest-all.cc)
target_link_libraries(TestRunner ${GRINGOFTS_LIB} gringofts_infra kv_engine_model_proto_library kv_engine_store kv_engine_model kv_engine_execution object_store_util object_store_network object_store_app object_manager_app)

# add coverage target here otherwise the app source code will not be covered
