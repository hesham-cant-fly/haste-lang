cmake_minimum_required(VERSION 3.12.0)
project(haste)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(DEBUG_BUILD ON)
  set(DEBUG_DEV_BUILD OFF)
elseif (CMAKE_BUILD_TYPE STREQUAL "DebugDev")
  set(DEBUG_BUILD ON)
  set(DEBUG_DEV_BUILD ON)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
  set(DEBUG_BUILD OFF)
  set(DEBUG_DEV_BUILD OFF)
endif()

if (DEBUG_BUILD)
  set(EXPORT_COMPILE_COMMANDS ON)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

set(C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(C_STANDARD 99)
set(CMAKE_C_STANDARD 99)

# Only enable warnings in debug build
add_compile_options(
  -Wall
  -Wextra
  -Wpedantic

  -Wno-unused-function
  -Wno-implicit-fallthrough
  -Wno-return-type
)
if (DEBUG_BUILD)
  add_compile_options(-Wno-attributes)
endif()
if (DEBUG_DEV_BUILD)
  add_compile_options(-Werror)
endif()

file(GLOB_RECURSE SOURCES PUBLIC src/*.c)
add_executable(${CMAKE_PROJECT_NAME} main.c ${SOURCES})
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC include/)

if (DEBUG_BUILD)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC DEBUG)
  target_compile_options(${CMAKE_PROJECT_NAME} PUBLIC -fsanitize=address)
  target_link_options(
    ${CMAKE_PROJECT_NAME}
    PUBLIC
    -fsanitize=address
  )
else()
  target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC RELEASE)
  target_link_options(
    ${CMAKE_PROJECT_NAME}
    PUBLIC
    -static
  )
endif()
