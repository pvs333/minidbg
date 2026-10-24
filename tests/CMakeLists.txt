# ---------------------------------------------------------------------------
# Test programs: small C programs the debugger is pointed at.
# ---------------------------------------------------------------------------
set(MINIDBG_PROGRAMS_DIR ${CMAKE_CURRENT_BINARY_DIR}/programs)

# add_test_program(<name> SOURCES <files...> [FLAGS <compile/link flags...>])
# Default flags: -g -O0 with frame pointers, built as PIE. FLAGS are appended last so they win.
function(add_test_program name)
    cmake_parse_arguments(TP "" "" "SOURCES;FLAGS" ${ARGN})
    list(TRANSFORM TP_SOURCES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/programs/)
    add_executable(${name} ${TP_SOURCES})
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${MINIDBG_PROGRAMS_DIR})
    # Build-type flags (e.g. -O3 for Release) must never leak into the test programs, so
    # set every option explicitly; later flags override earlier ones.
    target_compile_options(${name} PRIVATE -g -O0 -fno-omit-frame-pointer -fPIE ${TP_FLAGS})
    target_link_options(${name} PRIVATE -pie ${TP_FLAGS})
    target_link_libraries(${name} PRIVATE minidbg_warnings)
    list(APPEND MINIDBG_TEST_PROGRAM_TARGETS ${name})
    set(MINIDBG_TEST_PROGRAM_TARGETS ${MINIDBG_TEST_PROGRAM_TARGETS} PARENT_SCOPE)
endfunction()

add_test_program(hello SOURCES hello.c)
add_test_program(exit_code SOURCES exit_code.c)
add_test_program(segfault SOURCES segfault.c)
add_test_program(signals SOURCES signals.c)

# ---------------------------------------------------------------------------
# GoogleTest: prefer the system package, fall back to fetching it.
# ---------------------------------------------------------------------------
find_package(GTest CONFIG QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()
include(GoogleTest)

add_library(test_support STATIC support/test_support.cpp)
target_include_directories(test_support PUBLIC support)
target_link_libraries(test_support PUBLIC GTest::gtest GTest::gmock PRIVATE minidbg_warnings)
target_compile_definitions(test_support PUBLIC
    MINIDBG_BINARY="$<TARGET_FILE:minidbg>"
    MINIDBG_PROGRAMS_DIR="${MINIDBG_PROGRAMS_DIR}"
    MINIDBG_SOURCES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/programs")

# Unit tests: link the core library directly.
add_executable(unit_tests
    unit/test_strings.cpp
    unit/test_process.cpp
)
target_link_libraries(unit_tests PRIVATE minidbg_core test_support GTest::gtest_main minidbg_warnings)
add_dependencies(unit_tests ${MINIDBG_TEST_PROGRAM_TARGETS})
gtest_discover_tests(unit_tests DISCOVERY_TIMEOUT 30)

# End-to-end tests: run the real minidbg binary with commands on stdin.
add_executable(e2e_tests
    e2e/test_e2e_process.cpp
)
target_link_libraries(e2e_tests PRIVATE test_support GTest::gtest_main minidbg_warnings)
add_dependencies(e2e_tests minidbg ${MINIDBG_TEST_PROGRAM_TARGETS})
gtest_discover_tests(e2e_tests DISCOVERY_TIMEOUT 30)
