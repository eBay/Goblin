# Generated sources

# Include generated *.pb.h files


# Source files
set(UTIL_SRC AppUtil.cpp ClusterInfoUtil.cpp TessDNSResolver.cpp)

# Libraries
add_library(kv_engine_util ${UTIL_SRC})
#target_link_libraries(kv_engine_util util_proto_library)

# Executables
