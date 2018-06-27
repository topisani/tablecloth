# Version 3.8 is required for c++17
cmake_minimum_required(VERSION 3.8)
project(TABLECLOTH VERSION 0.0.1 LANGUAGES CXX C)

set(CMAKE_CXX_EXTENSIONS OFF)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TC_USE_LIBCXX "Use libc++ instead of libstdc++")
option(TC_BUILD_TESTS "Build tablecloth tests")

if (TC_USE_LIBCXX)
  message("Using libc++ instead of libstdc++")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

set(TABLECLOTH_EXTERNAL_DIR ${TABLECLOTH_SOURCE_DIR}/external/)

add_subdirectory(src)

if (TC_BUILD_TESTS)
  #add_subdirectory(test)
endif()
