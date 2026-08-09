cmake_minimum_required(VERSION 3.20)
project(navycraft_construct_prototype LANGUAGES CXX)

find_package(SQLite3 QUIET)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(navycraft_construct_core_sources
    ../engine-overlay/src/navycraft/chat_style.cpp
    ../engine-overlay/src/navycraft/construct/dynamic_construct.cpp
    ../engine-overlay/src/navycraft/construct/construct_node_state.cpp
    ../engine-overlay/src/navycraft/construct/construct_registry.cpp
    ../engine-overlay/src/navycraft/construct/construct_simulation.cpp
    ../engine-overlay/src/navycraft/construct/construct_network_clock.cpp
    ../engine-overlay/src/navycraft/construct/construct_handshake.cpp
    ../engine-overlay/src/navycraft/construct/construct_runtime.cpp
    ../engine-overlay/src/navycraft/construct/construct_effects.cpp
    ../engine-overlay/src/navycraft/construct/construct_projectiles.cpp
    ../engine-overlay/src/navycraft/construct/construct_fire_control.cpp
    ../engine-overlay/src/navycraft/construct/construct_navigation.cpp
    ../engine-overlay/src/navycraft/construct/construct_structure.cpp
    ../engine-overlay/src/navycraft/construct/construct_articulation.cpp
    ../engine-overlay/src/navycraft/construct/construct_articulated_motion.cpp
    ../engine-overlay/src/navycraft/construct/construct_liquids.cpp
    ../engine-overlay/src/navycraft/construct/construct_special_nodes.cpp
    ../engine-overlay/src/navycraft/construct/construct_articulation_packets.cpp
    ../engine-overlay/src/navycraft/construct/construct_interaction.cpp
    ../engine-overlay/src/navycraft/construct/construct_interaction_packets.cpp
    ../engine-overlay/src/navycraft/construct/construct_serialization.cpp
    ../engine-overlay/src/navycraft/construct/construct_section.cpp
    ../engine-overlay/src/navycraft/construct/construct_replication.cpp
    ../engine-overlay/src/navycraft/construct/construct_snapshot_buffer.cpp
    ../engine-overlay/src/navycraft/construct/construct_geometry.cpp
    ../engine-overlay/src/navycraft/construct/construct_collision_world.cpp
    ../engine-overlay/src/navycraft/construct/platform_contact.cpp
    ../engine-overlay/src/navycraft/construct/rider_motion.cpp
    ../engine-overlay/src/navycraft/construct/rider_packets.cpp
    ../engine-overlay/src/navycraft/construct/moving_hull_collision.cpp
    ../engine-overlay/src/navycraft/construct/carried_body.cpp
    ../engine-overlay/src/navycraft/construct/construct_mesh.cpp
    ../engine-overlay/src/navycraft/construct/construct_packets.cpp
    ../engine-overlay/src/navycraft/construct/client_construct_state.cpp
    ../engine-overlay/src/navycraft/network/construct_replication_queue.cpp
)
if(SQLite3_FOUND)
    list(APPEND navycraft_construct_core_sources
        ../engine-overlay/src/navycraft/construct/construct_database.cpp
        ../engine-overlay/src/navycraft/construct/construct_persistence.cpp
    )
endif()

add_library(navycraft_construct_core STATIC ${navycraft_construct_core_sources})
if(SQLite3_FOUND)
    target_link_libraries(navycraft_construct_core PUBLIC SQLite::SQLite3)
    target_compile_definitions(navycraft_construct_core PUBLIC NAVYCRAFT_HAS_SQLITE=1)
else()
    message(WARNING "SQLite3 was not found; prototype database persistence tests are disabled")
    target_compile_definitions(navycraft_construct_core PUBLIC NAVYCRAFT_HAS_SQLITE=0)
endif()
target_include_directories(navycraft_construct_core PUBLIC
    ../engine-overlay/src/navycraft/construct
    ../engine-overlay/src/navycraft
)
if(MSVC)
    target_compile_options(navycraft_construct_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(navycraft_construct_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_library(navycraft_script_api_syntax OBJECT
    ../engine-overlay/src/navycraft/script_api.cpp
)
target_include_directories(navycraft_script_api_syntax PRIVATE
    stubs
    ../engine-overlay/src
    ../engine-overlay/src/navycraft
)
if(MSVC)
    target_compile_options(navycraft_script_api_syntax PRIVATE /W4 /permissive-)
else()
    target_compile_options(navycraft_script_api_syntax PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()



add_library(navycraft_client_integration_syntax OBJECT
    ../engine-overlay/src/navycraft/client/client_integration.cpp
)
target_include_directories(navycraft_client_integration_syntax PRIVATE
    client_stubs
    ../engine-overlay/src/navycraft
    ../engine-overlay/src
)
if(MSVC)
    target_compile_options(navycraft_client_integration_syntax PRIVATE /W4 /permissive-)
else()
    target_compile_options(navycraft_client_integration_syntax PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()


add_library(navycraft_client_scene_syntax OBJECT
    ../engine-overlay/src/navycraft/client/client_construct_scene.cpp
)
target_include_directories(navycraft_client_scene_syntax PRIVATE
    scene_stubs
    ../engine-overlay/src/navycraft
    ../engine-overlay/src
)
if(MSVC)
    target_compile_options(navycraft_client_scene_syntax PRIVATE /W4 /permissive-)
else()
    target_compile_options(navycraft_client_scene_syntax PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_library(navycraft_client_effects_syntax OBJECT
    ../engine-overlay/src/navycraft/client/client_construct_effects.cpp
)
target_include_directories(navycraft_client_effects_syntax PRIVATE
    effect_stubs
    ../engine-overlay/src/navycraft
    ../engine-overlay/src
)
if(MSVC)
    target_compile_options(navycraft_client_effects_syntax PRIVATE /W4 /permissive-)
else()
    target_compile_options(navycraft_client_effects_syntax PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_library(navycraft_server_integration_syntax OBJECT
    ../engine-overlay/src/navycraft/server/server_integration.cpp
)
add_library(navycraft_server_handshake_syntax OBJECT
    ../engine-overlay/src/navycraft/server/handshake_integration.cpp
)
target_include_directories(navycraft_server_handshake_syntax PRIVATE
    integration_stubs
    ../engine-overlay/src/navycraft
    ../engine-overlay/src
)
if(MSVC)
    target_compile_options(navycraft_server_handshake_syntax PRIVATE /W4 /permissive-)
else()
    target_compile_options(navycraft_server_handshake_syntax PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_library(navycraft_server_simulation_syntax OBJECT
    ../engine-overlay/src/navycraft/server/simulation_integration.cpp
)
target_include_directories(navycraft_server_simulation_syntax PRIVATE
    integration_stubs
    ../engine-overlay/src/navycraft
    ../engine-overlay/src
)
if(MSVC)
    target_compile_options(navycraft_server_simulation_syntax PRIVATE /W4 /permissive-)
else()
    target_compile_options(navycraft_server_simulation_syntax PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_include_directories(navycraft_server_integration_syntax PRIVATE
    integration_stubs
    ../engine-overlay/src/navycraft
    ../engine-overlay/src
)
if(MSVC)
    target_compile_options(navycraft_server_integration_syntax PRIVATE /W4 /permissive-)
else()
    target_compile_options(navycraft_server_integration_syntax PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_library(navycraft_server_rider_syntax OBJECT
    ../engine-overlay/src/navycraft/server/rider_integration.cpp
)
target_include_directories(navycraft_server_rider_syntax PRIVATE
    integration_stubs
    ../engine-overlay/src/navycraft
    ../engine-overlay/src
)
if(MSVC)
    target_compile_options(navycraft_server_rider_syntax PRIVATE /W4 /permissive-)
else()
    target_compile_options(navycraft_server_rider_syntax PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()


add_library(navycraft_server_interaction_syntax OBJECT
    ../engine-overlay/src/navycraft/server/interaction_integration.cpp
)
target_include_directories(navycraft_server_interaction_syntax PRIVATE
    integration_stubs
    ../engine-overlay/src/navycraft
    ../engine-overlay/src
)
if(MSVC)
    target_compile_options(navycraft_server_interaction_syntax PRIVATE /W4 /permissive-)
else()
    target_compile_options(navycraft_server_interaction_syntax PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(navycraft_construct_tests tests/construct_core_tests.cpp)
target_link_libraries(navycraft_construct_tests PRIVATE navycraft_construct_core)


add_executable(navycraft_construct_benchmark tests/construct_benchmark.cpp)
target_link_libraries(navycraft_construct_benchmark PRIVATE navycraft_construct_core)

include(CTest)
add_test(NAME construct_core COMMAND navycraft_construct_tests)
