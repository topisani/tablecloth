set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE headers ${TABLECLOTH_SOURCE_DIR}/src/*.hpp)
file(GLOB_RECURSE sources ${TABLECLOTH_SOURCE_DIR}/src/*.cpp)
list(REMOVE_ITEM sources ${TABLECLOTH_SOURCE_DIR}/src/main.cpp)

find_package (Threads)
find_package(OpenGL REQUIRED)
include(FindPkgConfig)

find_library(LIBRT rt)
mark_as_advanced(LIBRT)

# Library
add_library(tablecloth ${sources})
target_link_libraries(tablecloth PUBLIC ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(tablecloth PUBLIC ./)

if (NOT TC_USE_LIBCXX)
  target_link_libraries(tablecloth PUBLIC atomic)
endif()

# Executable
add_executable(tablecloth_exec ${TABLECLOTH_SOURCE_DIR}/src/main.cpp)
target_link_libraries(tablecloth_exec PUBLIC tablecloth)
set_target_properties(tablecloth_exec PROPERTIES OUTPUT_NAME tablecloth)

add_subdirectory(${TABLECLOTH_EXTERNAL_DIR} ${TABLECLOTH_BINARY_DIR}/external)
#target_link_libraries(tablecloth PUBLIC external)

target_link_libraries(tablecloth PUBLIC wayland-client++ wayland-client-extra++ wayland-cursor++ "${OPENGL_gl_LIBRARY}")
target_include_directories(tablecloth PUBLIC "${OPENGL_INCLUDE_DIR}")

if(LIBRT)
  target_link_libraries(tablecloth PUBLIC "${LIBRT}")
endif()

# Enable warnings for local code
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-long-long -Wpedantic -Wno-c++14-compat -Wno-psabi -Wno-unknown-pragmas")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-long-long -Wpedantic \
-Wno-c++11-compat -Wno-c++98-compat -Wno-c++14-compat -Wc++17-compat \
-Wno-shadow-field-in-constructor \
-Wno-documentation-unknown-command \
-Wno-unknown-pragmas \
")
endif()

