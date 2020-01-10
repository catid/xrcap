cmake_minimum_required(VERSION 3.5)
project(rapidjson CXX)

include_directories(include)


################################################################################
# Dependencies

find_package(rapidjson 1.1.0 EXACT
    PATHS
        "${CMAKE_SOURCE_DIR}/thirdparty/bin/rapidjson/cmake"
    REQUIRED
)

message("RapidJSON_INCLUDE_DIR: ${RapidJSON_INCLUDE_DIR}")


################################################################################
# Targets

# rapidjson library

add_library(rapidjson INTERFACE)
target_include_directories(rapidjson INTERFACE ${RapidJSON_INCLUDE_DIR})
