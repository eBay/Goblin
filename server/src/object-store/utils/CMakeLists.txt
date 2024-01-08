# Generated sources
set(proto_generated_dir "../../../..//protocols/generated")
file(MAKE_DIRECTORY ${proto_generated_dir})

# proto
set(proto_file_list
        ../../../../protocols/microvault.proto)
ADD_PROTO_SET(object_store_util_proto_library "${proto_file_list}" ${proto_generated_dir})

# Include generated *.pb.h files


# Source files
set(UTIL_SRC ThreadPoolService.cpp)

# Libraries
add_library(object_store_util ${UTIL_SRC})
target_link_libraries(object_store_util absl::str_format object_store_util_proto_library)

# Executables
