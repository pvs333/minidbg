cmake_minimum_required(VERSION 3.20)
project(minidbg VERSION 0.1.0 LANGUAGES C CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    message(FATAL_ERROR "minidbg only supports Linux x86-64")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(MINIDBG_BUILD_TESTS "Build the test programs and test suites" ON)

# Warning policy for all first-party code (C++ and the C test programs).
add_library(minidbg_warnings INTERFACE)
target_compile_options(minidbg_warnings INTERFACE -Wall -Wextra -Werror)

# Vendored third-party code is compiled without -Werror: we don't own its warnings.
add_library(linenoise STATIC third_party/linenoise/linenoise.c)
target_include_directories(linenoise SYSTEM PUBLIC third_party/linenoise)

add_library(minidbg_core STATIC
    src/cli.cpp
    src/debugger.cpp
    src/process.cpp
    src/strings.cpp
)
target_include_directories(minidbg_core PUBLIC src)
target_link_libraries(minidbg_core PUBLIC linenoise PRIVATE minidbg_warnings)

add_executable(minidbg src/main.cpp)
target_link_libraries(minidbg PRIVATE minidbg_core minidbg_warnings)

if(MINIDBG_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
