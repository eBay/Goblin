project(ObjectManager)

# Generated sources

# proto

# Include generated *.pb.h files

# Source files
set(MODULE_SRC
        Cluster.cpp
        Base.cpp
        Migration.cpp
        ClusterSetManager.cpp)

# Libraries
add_library(object_manager_module STATIC ${MODULE_SRC})
target_link_libraries(object_manager_module object_manager_proto_library object_manager_route)
#add_dependencies(object_manager_app)

# Executables

