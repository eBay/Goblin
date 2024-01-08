project(ObjectManager)

# Generated sources
set(proto_generated_dir "../../../../protocols/generated")
file(MAKE_DIRECTORY ${proto_generated_dir})

# proto
set(proto_file_list
        ../../../../protocols/common.proto
        ../../../../protocols/service.proto
        ../../../../protocols/control.proto)
ADD_PROTO_SET(object_manager_proto_library "${proto_file_list}" ${proto_generated_dir})

# Include generated *.pb.h files

# Source files
set(NETWORK_SRC
        RequestCallData.cpp
        RequestReceiver.cpp
        rpc/RouterCallData.cpp
        rpc/OnStartupCallData.cpp
        rpc/OnMigrationCallData.cpp
        rpc/AddClusterCallData.cpp
        rpc/RemoveClusterCallData.cpp
        rpc/BaseCallData.cpp)

# Libraries

# Executables
add_library(object_manager_network STATIC ${NETWORK_SRC})
target_link_libraries(object_manager_network
        object_manager_proto_library
        object_manager_route
        object_manager_migration
        object_manager_module)

# executables


