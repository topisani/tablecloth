set(CMAKE_CXX_STANDARD 17)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-pedantic -Wno-unused-function")
elseif(${CMAKE_CXX_COMPILER_ID} EQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-pedantic -Wno-unused-function")
endif()

# # General purpose externals
# file(GLOB_RECURSE external_src
#   "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
#   "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
# )
# add_library(external ${external_src})
# target_include_directories(external SYSTEM PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include)
