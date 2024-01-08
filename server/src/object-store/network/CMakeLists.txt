
# Generated sources

# proto

# Include generated *.pb.h files

# Source files
set(NETWORK_SRC
        RequestCallData.cpp
        RequestReceiver.cpp
        ObjectManagerClient.cpp
        ObjectStoreClient.cpp
        rpc/KVGetCallData.cpp
        rpc/KVDeleteCallData.cpp
        rpc/KVPutCallData.cpp
        rpc/KVTransCallData.cpp
        rpc/KVGenerateCallData.cpp
        rpc/KVWatchCallData.cpp
        rpc/StartMigrationCallData.cpp
        rpc/EndMigrationCallData.cpp
        rpc/ConnectCallData.cpp)

# Libraries

# Executables
add_library(object_store_network STATIC
        ${NETWORK_SRC})

#add_dependencies(object_store_network)
target_link_libraries(object_store_network object_store_model kv_engine_model_proto_library kv_engine_network_proto_library)

# executables
