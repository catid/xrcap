cmake_minimum_required(VERSION 3.5)
project(blake2 LANGUAGES CXX)


################################################################################
# Source

set(INCLUDE_FILES
    include/blake2.h
    include/blake2b-load-sse2.h
    include/blake2b-load-sse41.h
    include/blake2b-round.h
)

set(SOURCE_FILES
    ${INCLUDE_FILES}
    src/blake2b.c
    src/blake2bp.c
    src/blake2s.c
    src/blake2sp.c
    src/blake2xb.c
    src/blake2xs.c
)

include_directories(include)


################################################################################
# Targets

# blake2 library

add_library(blake2 STATIC ${SOURCE_FILES})
target_include_directories(blake2 PUBLIC include)
