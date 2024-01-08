
# Generated sources
set(proto_generated_dir "../../../../protocols/generated")
file(MAKE_DIRECTORY ${proto_generated_dir})

# proto

# Include generated *.pb.h files

# Source files
set(MODEL_SRC
        ScanCommand.cpp
        user-define/TransferCommand.cpp)

# Libraries
add_library(object_store_model ${MODEL_SRC})
target_link_libraries(object_store_model kv_engine_model kv_engine_model_proto_library)

# Executables

