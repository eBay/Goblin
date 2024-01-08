project(ObjectStore)

# Generated sources

# proto

# Include generated *.pb.h files

# Source files
set(APP_SRC Admin.cpp OldRocksDBKVStore.cpp)

# Libraries
add_library(admin_app STATIC
        ${APP_SRC})

#add_dependencies(object_store_app)
target_link_libraries(admin_app
        object_store_util
        object_store_model
        object_store_network
        object_store_migration
        kv_engine)

# Executables
add_executable(AdminMain Main.cpp)
target_link_libraries(AdminMain admin_app)

