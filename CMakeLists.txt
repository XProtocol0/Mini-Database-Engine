cmake_minimum_required(VERSION 3.24)
project(MiniDB LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MINIDB_BUILD_TESTS "Build unit tests" ON)
option(MINIDB_BUILD_BENCHMARKS "Build benchmarks" ON)
# Sanitizer selection. ASan+UBSan and TSan are mutually exclusive, so they are
# separate build trees (see CMakePresets.json).
set(MINIDB_SANITIZER "" CACHE STRING "Sanitizer build: '', 'address' (ASan+UBSan) or 'thread' (TSan)")
set_property(CACHE MINIDB_SANITIZER PROPERTY STRINGS "" address thread)

# Warnings and sanitizer flags are carried by an interface target so they apply
# to our code only, not to fetched third-party code such as GoogleTest.
add_library(minidb_options INTERFACE)
target_compile_options(minidb_options INTERFACE -Wall -Wextra -Wpedantic)
option(MINIDB_WERROR "Treat warnings as errors" OFF)
if(MINIDB_WERROR)
  target_compile_options(minidb_options INTERFACE -Werror)
endif()

if(MINIDB_SANITIZER STREQUAL "address")
  set(_san_flags -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
elseif(MINIDB_SANITIZER STREQUAL "thread")
  set(_san_flags -fsanitize=thread -fno-omit-frame-pointer)
elseif(NOT MINIDB_SANITIZER STREQUAL "")
  message(FATAL_ERROR "Unknown MINIDB_SANITIZER '${MINIDB_SANITIZER}'")
endif()
if(_san_flags)
  # Sanitizers must be applied globally: mixing instrumented and
  # uninstrumented code (e.g. GoogleTest) gives TSan false positives.
  add_compile_options(${_san_flags})
  add_link_options(${_san_flags})
endif()

find_package(Threads REQUIRED)

add_subdirectory(src)

if(MINIDB_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(MINIDB_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()
