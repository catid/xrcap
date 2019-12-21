cmake_minimum_required(VERSION 3.5)
project(yaml CXX)

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

find_library(YAML_STATIC_LIB MODULE
    NAMES
        libyaml-cppmd
        libyaml-cppmdd
        libyaml-cpp
    HINTS
        "${CMAKE_SOURCE_DIR}/extern/bin/yaml-cpp/lib/"
    NO_CMAKE_SYSTEM_PATH
    REQUIRED
)

find_path(YAML_INCLUDE_DIR
    NAMES
        yaml-cpp/yaml.h
    PATHS
        "${CMAKE_SOURCE_DIR}/extern/bin/yaml-cpp/include"
    REQUIRED
)

message("YAML_INCLUDE_DIR: ${YAML_INCLUDE_DIR}")
message("YAML_STATIC_LIB: ${YAML_STATIC_LIB}")


################################################################################
# Targets

# yaml library

add_library(yaml INTERFACE)
target_include_directories(yaml INTERFACE ${YAML_INCLUDE_DIR})
target_link_libraries(yaml INTERFACE ${YAML_STATIC_LIB})
