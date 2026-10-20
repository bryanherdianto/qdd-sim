cmake_minimum_required(VERSION 3.20)
project(qdd-sim VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(QDD_WERROR "Treat warnings as errors" OFF)
option(BUILD_TESTING "Build test suite" ON)

# Compiler warning flags
add_library(qdd_warnings INTERFACE)
if(MSVC)
    target_compile_options(qdd_warnings INTERFACE /W4 /permissive-)
    if(QDD_WERROR)
        target_compile_options(qdd_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(qdd_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
    )
    if(QDD_WERROR)
        target_compile_options(qdd_warnings INTERFACE -Werror)
    endif()
endif()

# Core static library
add_library(qdd STATIC
    src/complex_table.cpp
    src/package.cpp
    src/ops_add.cpp
    src/ops_mul.cpp
    src/ops_adjoint.cpp
    src/gates.cpp
    src/qasm_lexer.cpp
    src/qasm_parser.cpp
    src/simulator.cpp
    src/ec.cpp
    src/stats.cpp
    src/memory_usage.cpp
)

target_include_directories(qdd
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(qdd PUBLIC qdd_warnings)

if(WIN32)
    target_link_libraries(qdd PUBLIC psapi)
endif()

# CLI Application
add_executable(qdd-sim apps/qdd-sim/main.cpp)
target_link_libraries(qdd-sim PRIVATE qdd)

# Tests
if(BUILD_TESTING)
    enable_testing()

    find_package(Catch2 3 QUIET)
    if(NOT Catch2_FOUND)
        message(STATUS "Catch2 v3 not found locally, fetching via FetchContent...")
        include(FetchContent)
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.5.2
        )
        FetchContent_MakeAvailable(Catch2)
    endif()

    add_executable(qdd_tests
        tests/test_complex_table.cpp
        tests/test_normalisation.cpp
        tests/test_vector_ops.cpp
        tests/test_matrix_ops.cpp
        tests/test_gates.cpp
        tests/test_qasm.cpp
        tests/test_simulator.cpp
        tests/test_ec.cpp
    )

    target_include_directories(qdd_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(qdd_tests PRIVATE qdd Catch2::Catch2WithMain)

    include(CTest)
    include(Catch)
    catch_discover_tests(qdd_tests WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
