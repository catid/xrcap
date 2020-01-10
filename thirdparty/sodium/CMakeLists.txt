cmake_minimum_required(VERSION 3.5)
project(sodium CXX)


################################################################################
# Source Files

set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/src/libsodium/src/libsodium/include")

file(GLOB_RECURSE SOURCE_FILES
    LIST_DIRECTORIES false
    "${CMAKE_SOURCE_DIR}/thirdparty/src/libsodium/src/libsodium/*.c"
)

################################################################################
# Targets

# This library is ridiculous
include_directories(
    include
    include/sodium
    src
    ${INCLUDE_DIR}
    "${INCLUDE_DIR}/sodium"
)

# sodium library

add_library(sodium STATIC
    ${SOURCE_FILES}
    src/crypto_spake.c
)
target_compile_definitions(sodium PRIVATE SODIUM_STATIC=1)
target_include_directories(sodium PUBLIC ${INCLUDE_DIR} include)
