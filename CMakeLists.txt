cmake_minimum_required(VERSION 3.14)
project(netbridge CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Emit compile_commands.json so clangd / the Cursor C++ extension can see
# the -std=c++17 flag and the include paths. Without this, std::optional,
# std::variant, structured bindings, etc. are flagged as unknown even
# though the project builds cleanly.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(FetchContent)

# Core library: aggregates all non-entry-point source files.
set(NB_CORE_SOURCES
    src/net/packet.cpp
    src/net/transport.cpp
    src/net/emulator_packet.cpp
    src/net/link_state.cpp
)

# Phase 2: events + runtime manager
list(APPEND NB_CORE_SOURCES
    src/core/ordered_event_queue.cpp
    src/core/runtime_manager.cpp
)

# Phase 3: topology + parsers
list(APPEND NB_CORE_SOURCES
    src/sim/commands_parser.cpp
    src/sim/simulation_commands_parser.cpp
    src/sim/emulation_commands_parser.cpp
    src/sim/trawler_commands_parser.cpp
    src/sim/topology.cpp
    src/sim/edge.cpp
)

# Phase 4: Simulator loop + console IO + StackNode (ping layer).
list(APPEND NB_CORE_SOURCES
    src/sim/simulator.cpp
    src/sim/console_io_thread.cpp
    src/node/stack_node.cpp
)

# Phase 5: TCP transport stack.
list(APPEND NB_CORE_SOURCES
    src/node/tcp_manager.cpp
    src/node/tcp_socket.cpp
)

# Phase 6: timer-driven apps.
list(APPEND NB_CORE_SOURCES
    src/apps/timer_task.cpp
    src/apps/transfer_client.cpp
    src/apps/transfer_server.cpp
)

# Phase 7 partial: emulated link + ARP command serializer + Emulator and
# TrawlerService scaffolding.
list(APPEND NB_CORE_SOURCES
    src/emu/emulated_link.cpp
    src/emu/trawler_node_arp_commands.cpp
    src/emu/emulated_node.cpp
    src/emu/emulator.cpp
    src/emu/trawler_service.cpp
)

add_library(nb_core STATIC ${NB_CORE_SOURCES})
target_include_directories(nb_core PUBLIC include)
find_package(Threads REQUIRED)
target_link_libraries(nb_core PUBLIC Threads::Threads)

add_executable(netstack src/main.cpp)
target_link_libraries(netstack PRIVATE nb_core)

add_executable(trawler src/trawler_main.cpp)
target_link_libraries(trawler PRIVATE nb_core)

# Tests
enable_testing()

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    URL_HASH SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(nb_tests
    test/test_packet.cpp
    test/test_transport.cpp
    test/test_emulator_packet.cpp
    test/test_ordered_event_queue.cpp
    test/test_trawler_node_arp_commands.cpp
    test/test_commands_parser.cpp
)
target_link_libraries(nb_tests PRIVATE nb_core gtest_main)

include(GoogleTest)
gtest_discover_tests(nb_tests)
