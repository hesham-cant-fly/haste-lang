cmake_minimum_required(VERSION 3.12.0)
project(haste LANGUAGES CXX)

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")

set(LANGUAGE CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)

set(SANITIZE ON)

file(GLOB_RECURSE CXX_SRC src/*.cpp)

add_executable(${CMAKE_PROJECT_NAME} ${CXX_SRC} main.cpp)
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC include)
<<<<<<< HEAD
=======
target_compile_options(${CMAKE_PROJECT_NAME} PUBLIC -Wall -Wextra -Wpedantic -Wno-attributes)
target_link_libraries(${CMAKE_PROJECT_NAME} m)
>>>>>>> parent of 7f0daf6 (add variable and constant statements)

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

