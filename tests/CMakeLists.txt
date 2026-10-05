# Host tests for include/minimum_logic.hpp. Needs no Geode SDK, only a C++20 compiler:
#
#   cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests --output-on-failure

cmake_minimum_required(VERSION 3.21)
project(MinimumLogicTests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(logic_test logic_test.cpp)
target_include_directories(logic_test PRIVATE ../include)
if (NOT MSVC)
  target_compile_options(logic_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME minimum_logic COMMAND logic_test)
