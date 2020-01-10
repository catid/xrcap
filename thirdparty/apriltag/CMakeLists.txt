cmake_minimum_required(VERSION 3.5)
project(apriltag LANGUAGES C CXX)


################################################################################
# Source

file(GLOB_RECURSE INCLUDE_FILES
    LIST_DIRECTORIES false
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

file(GLOB_RECURSE SOURCE_FILES
    LIST_DIRECTORIES false
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)
set(SOURCE_FILES
    ${INCLUDE_FILES}
    ${SOURCE_FILES}
    src/common/PthreadWin32Adapter.cpp
)

include_directories(include include/common)


################################################################################
# Targets

# apriltag library

add_library(apriltag STATIC ${SOURCE_FILES})
target_include_directories(apriltag PUBLIC include)

# apriltag test

add_executable(apriltag_test tests/test.cpp)
target_link_libraries(apriltag_test
    apriltag
)

install(TARGETS apriltag_test DESTINATION bin)
