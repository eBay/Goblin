project(ObjectManager)

# Generated sources

# proto

# Include generated *.pb.h files
add_subdirectory(module)
add_subdirectory(migration)
add_subdirectory(route)
add_subdirectory(network)

# Source files
set(APP_SRC
  ObjectManager.cpp)

# Libraries
add_library(object_manager_app STATIC
        ${APP_SRC})

#add_dependencies(object_manager_app)
target_link_libraries(object_manager_app
        kv_engine
        object_manager_network
        )

# Executables
add_executable(ObjectManagerMain Main.cpp)
target_link_libraries(ObjectManagerMain
        object_manager_app
        object_manager_module
        object_manager_route
        object_manager_migration)

