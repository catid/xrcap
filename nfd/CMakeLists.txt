cmake_minimum_required(VERSION 3.5)
project(nfd LANGUAGES CXX)


################################################################################
# Source

set(INCLUDE_FILES
    include/nfd.h
)

set(SOURCE_FILES
    ${INCLUDE_FILES}
    src/common.h
    src/nfd_common.c
    src/nfd_common.h
    src/simple_exec.h
    src/nfd_win.cpp # Windows specialized version
    #src/nfd_gtk.c # Linux specialized version
)

include_directories(include)


################################################################################
# Targets

# nfd library

add_library(nfd STATIC ${SOURCE_FILES})
target_include_directories(nfd PUBLIC include)
