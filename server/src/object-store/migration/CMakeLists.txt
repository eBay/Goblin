# Generated sources

# proto

# Include generated *.pb.h files


# Source files
set(MIGRATION_SRC MigrationManager.cpp)

# Libraries
add_library(object_store_migration ${MIGRATION_SRC})
target_link_libraries(object_store_migration absl::str_format object_store_util object_store_util_proto_library object_store_model object_store_network)

# Executables
