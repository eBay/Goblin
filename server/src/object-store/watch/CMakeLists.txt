
# Generated sources

# proto

# Include generated *.pb.h files

# Source files
set(WATCH_SRC
        WatchObserver.cpp
        WatchCenter.cpp
        WatchEntry.cpp)

# Libraries

# Executables
add_library(object_store_watch STATIC
        ${WATCH_SRC})

#add_dependencies(object_store_network)
target_link_libraries(object_store_watch kv_engine_model kv_engine_model_proto_library)

# executables