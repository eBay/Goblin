project(ObjectStore)

# Generated sources

# proto

# Include generated *.pb.h files

add_subdirectory(utils)
add_subdirectory(model)
add_subdirectory(network)
add_subdirectory(migration)
add_subdirectory(watch)

# Source files
set(APP_SRC
        ObjectStore.cpp)

# Libraries
add_library(object_store_app STATIC
        ${APP_SRC})

#add_dependencies(object_store_app)
target_link_libraries(object_store_app
        object_store_util
        object_store_model
        object_store_network
        object_store_migration
        object_store_watch
        kv_engine)

# Executables
add_executable(ObjectStoreMain Main.cpp)
target_link_libraries(ObjectStoreMain object_store_app)

