cmake_minimum_required(VERSION 3.16)
project(AutonomousMicrogridSimulator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

# Header-only style for simplicity + one executable
add_executable(microgrid_sim
    src/main.cpp
)

target_include_directories(microgrid_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Simple unit-test executable (no external test framework dependency)
add_executable(microgrid_tests
    tests/test_main.cpp
)

target_include_directories(microgrid_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

enable_testing()
add_test(NAME core_tests COMMAND microgrid_tests)

# Install rules (optional)
install(TARGETS microgrid_sim DESTINATION bin)

message(STATUS "Autonomous Microgrid Simulator - Educational C++ project")
message(STATUS "  Build: cmake -B build && cmake --build build")
message(STATUS "  Run:   ./build/microgrid_sim [--steps N] [--seed S]")
