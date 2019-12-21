cmake_minimum_required(VERSION 3.5)
project(capture_protocol LANGUAGES CXX)


################################################################################
# Source

set(INCLUDE_FILES
    include/CaptureProtocol.hpp
)

set(SOURCE_FILES
    ${INCLUDE_FILES}
    src/CaptureProtocol.cpp
)

include_directories(include)


################################################################################
# Targets

# capture_protocol library

add_library(capture_protocol STATIC ${SOURCE_FILES})

target_include_directories(capture_protocol PUBLIC include)
target_link_libraries(capture_protocol PUBLIC
    core
    depth_mesh # Used for serializing the calibration structure
)

install(FILES ${INCLUDE_FILES} DESTINATION include)
install(TARGETS capture_protocol DESTINATION lib)
