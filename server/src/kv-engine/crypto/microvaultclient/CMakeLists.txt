set(proto_generated_dir "generated/grpc")
file(MAKE_DIRECTORY ${proto_generated_dir})

ADD_PROTO_RESOURCE(
        microvault_proto_library
        "microvault.proto"
        ${proto_generated_dir})

set(APP_UTIL_CLIENT_SRC MicrovaultClient.cpp)

add_library(microvault_client STATIC ${APP_UTIL_CLIENT_SRC})
target_link_libraries(microvault_client microvault_proto_library ${ABSL_LIBS})