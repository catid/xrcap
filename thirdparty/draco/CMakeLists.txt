cmake_minimum_required(VERSION 3.5)
project(draco CXX)

include_directories(include)


################################################################################
# Dependencies

message("draco_INCLUDE_DIR: ${draco_INCLUDE_DIR}")
message("draco_LIBRARY_DIR: ${draco_LIBRARY_DIR}")

if (WIN32)
    set(CMAKE_FIND_LIBRARY_PREFIXES "")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib" ".dll")
else()
    set(CMAKE_FIND_LIBRARY_PREFIXES "lib")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".so" ".a")
endif()

find_library(DRACO_STATIC_LIB MODULE
    NAMES
        "draco"
    HINTS
        "${CMAKE_SOURCE_DIR}/thirdparty/bin/draco/lib"
    NO_CMAKE_SYSTEM_PATH
    REQUIRED
)

find_path(DRACO_INCLUDE_DIR
    NAMES
        draco/compression/encode.h
    PATHS
        "${CMAKE_SOURCE_DIR}/thirdparty/bin/draco/include"
    REQUIRED
)

message("DRACO_STATIC_LIB: ${DRACO_STATIC_LIB}")
message("DRACO_INCLUDE_DIR: ${DRACO_INCLUDE_DIR}")


################################################################################
# Targets

# draco library

add_library(draco INTERFACE)
target_include_directories(draco INTERFACE ${DRACO_INCLUDE_DIR})
target_link_libraries(draco INTERFACE ${DRACO_STATIC_LIB})
