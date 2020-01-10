cmake_minimum_required(VERSION 3.5)
project(tbb CXX)

include_directories(include)


################################################################################
# Dependencies

if (WIN32)
    set(CMAKE_FIND_LIBRARY_PREFIXES "")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib" ".dll")
else()
    set(CMAKE_FIND_LIBRARY_PREFIXES "lib")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".so" ".a")
endif()

find_library(TBB_STATIC_LIB MODULE
    NAMES
        "tbb_static"
    HINTS
        "${CMAKE_SOURCE_DIR}/extern/bin/tbb/lib/"
    NO_CMAKE_SYSTEM_PATH
    REQUIRED
)

find_path(TBB_INCLUDE_DIR
    NAMES
        tbb/tbb.h
    PATHS
        "${CMAKE_SOURCE_DIR}/extern/bin/tbb/include"
    REQUIRED
)

message("TBB_INCLUDE_DIR: ${TBB_INCLUDE_DIR}")
message("TBB_STATIC_LIB: ${TBB_STATIC_LIB}")


################################################################################
# Targets

# tbb library

add_library(tbb INTERFACE)
target_include_directories(tbb INTERFACE ${TBB_INCLUDE_DIR})
target_link_libraries(tbb INTERFACE ${TBB_STATIC_LIB})
target_compile_definitions(tbb INTERFACE __TBB_NO_IMPLICIT_LINKAGE=1)
