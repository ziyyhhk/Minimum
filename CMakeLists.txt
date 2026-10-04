cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Minimum VERSION 4.0.0)

add_library(${PROJECT_NAME} SHARED
  src/main.cpp
  src/config.cpp
  src/frame_gate.cpp
  src/particles.cpp
  src/app_delegate.cpp
  src/system_tuning.cpp
  src/background.cpp
  src/hotkeys.cpp
  src/latency.cpp
  src/pause_menu.cpp
)

if (NOT DEFINED ENV{GEODE_SDK})
  message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
  message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} $ENV{GEODE_SDK}/build)

setup_geode_mod(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PRIVATE include)

target_link_libraries(${PROJECT_NAME} geode-sdk)
