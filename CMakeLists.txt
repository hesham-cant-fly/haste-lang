cmake_minimum_required(VERSION 3.12.0)
project(haste)

set(LANGUAGE C)
set(C_STANDARD 11)
set(CMAKE_C_STANDARD 11)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SANITIZE ON)

file(GLOB_RECURSE C_SRC src/*.c)

add_executable(${CMAKE_PROJECT_NAME} ${C_SRC} main.c)
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC include)
target_compile_options(${CMAKE_PROJECT_NAME} PUBLIC -Wall -Wextra -Wpedantic -Wno-attributes)
target_link_libraries(${CMAKE_PROJECT_NAME} m)

if(${SANITIZE})
  target_compile_options(${CMAKE_PROJECT_NAME} PUBLIC -fsanitize=address)
  target_link_options(${CMAKE_PROJECT_NAME} PUBLIC -fsanitize=address)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC DEBUG_TRACE_EXECUTION)
endif()

if(${CMAKE_BUILD_TYPE} STREQUAL Debug)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC DEBUG)
else()
  target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC RELEASE)
endif()

