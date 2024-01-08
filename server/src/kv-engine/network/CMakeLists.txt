
# Generated sources
set(proto_generated_dir "../../../../protocols/generated")
file(MAKE_DIRECTORY ${proto_generated_dir})

# proto
set(proto_file_list
        ../../../../protocols/netadmin.proto)
ADD_PROTO_SET(kv_engine_network_proto_library "${proto_file_list}" ${proto_generated_dir})

# Include generated *.pb.h files

# Source files

# Libraries
# add_library(kv_engine_network_proto_library)

# Executables

