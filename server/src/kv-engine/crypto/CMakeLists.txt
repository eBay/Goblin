add_subdirectory(microvaultclient)

set(KV_ENGINE_CRYPTO_SRC MicrovaultSecretKey.cpp)
add_library(kv_engine_crypto STATIC ${KV_ENGINE_CRYPTO_SRC})
target_link_libraries(kv_engine_crypto microvault_client)