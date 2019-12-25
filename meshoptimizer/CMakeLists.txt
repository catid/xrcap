cmake_minimum_required(VERSION 3.5)
project(meshoptimizer CXX)

if (WIN32)
    set(CMAKE_FIND_LIBRARY_PREFIXES "")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib" ".dll")
else()
    set(CMAKE_FIND_LIBRARY_PREFIXES "lib")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".so" ".a")
endif()

find_library(meshoptimizer_LIB
    NAMES
        meshoptimizer
    PATHS
        ${CMAKE_SOURCE_DIR}/extern/bin/meshoptimizer/lib/
    REQUIRED
)

find_path(meshoptimizer_INCLUDE_DIRS meshoptimizer.h
    PATHS
        ${CMAKE_SOURCE_DIR}/extern/bin/meshoptimizer/include/
    REQUIRED
)

message("meshoptimizer_LIB: ${meshoptimizer_LIB}")
message("meshoptimizer_INCLUDE_DIRS: ${meshoptimizer_INCLUDE_DIRS}")


################################################################################
# Targets

# meshoptimizer library

add_library(meshoptimizer INTERFACE)
target_include_directories(meshoptimizer INTERFACE ${meshoptimizer_INCLUDE_DIRS})
target_link_libraries(meshoptimizer INTERFACE
    ${meshoptimizer_LIB}
)
