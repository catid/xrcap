cmake_minimum_required(VERSION 3.5)
project(depth_mesh LANGUAGES CXX)


################################################################################
# Source

set(INCLUDE_FILES
    include/DepthMesh.hpp
    include/DepthCalibration.hpp
    include/CameraExtrinsics.hpp
    include/ColorNormalization.hpp
)

set(SOURCE_FILES
    ${INCLUDE_FILES}
    src/DepthMesh.cpp
    src/CameraExtrinsics.cpp
    src/ColorNormalization.cpp
)

include_directories(include)


################################################################################
# Targets

# depth_mesh library

add_library(depth_mesh STATIC ${SOURCE_FILES})
target_link_libraries(depth_mesh PUBLIC
    core
    open3d # Implicitly includes Eigen
    apriltag
)
target_include_directories(depth_mesh PUBLIC include)

# depth_mesh_tests application

add_executable(depth_mesh_tests tests/depth_mesh_tests.cpp)
target_link_libraries(depth_mesh_tests PRIVATE
    core
    depth_mesh
)

install(TARGETS depth_mesh_tests DESTINATION bin)
