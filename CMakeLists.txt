cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Minimum VERSION 1.0.0)

# Only include source files that actually exist in the repo
add_library(${PROJECT_NAME} SHARED
  src/shared_pool.cpp
  src/features/improvements/anti_aliasing.cpp
  src/features/improvements/fast_alt_tab.cpp
  src/features/improvements/force_rgba8888.cpp
  src/features/optimizations/processing/fast_pickup.cpp
  src/features/optimizations/rendering/parallel_batch.cpp
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

if(WIN32)
  target_link_libraries(${PROJECT_NAME} opengl32)
elseif(APPLE)
  target_link_libraries(${PROJECT_NAME} "-framework OpenGL -framework Cocoa")
elseif(ANDROID)
  target_link_libraries(${PROJECT_NAME} GLESv2)
endif()

# Note: large original AlgebraDash sources (parallel_loading, update_visibility_rewrite, etc.)
# are not yet ported. Re-add them to the list above once the files exist and compile on modern Geode.
