project(ObjectManager)

# Generated sources

# proto

# Include generated *.pb.h files

# proto
# Source files
set(ROUTE_SRC
        HashRing.cpp)

# Include generated *.pb.h files
# Libraries

# Executables
add_library(object_manager_route STATIC ${ROUTE_SRC})
#target_link_libraries(object_manager_route object_manager_proto_library)