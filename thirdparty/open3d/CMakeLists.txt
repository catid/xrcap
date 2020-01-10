cmake_minimum_required(VERSION 3.5)
project(open3d CXX)

find_package(Open3D 0.8.0.0 EXACT
    PATHS
        "${CMAKE_SOURCE_DIR}/thirdparty/bin/Open3D/CMake"
    REQUIRED
)

if (WIN32)
    set(CMAKE_FIND_LIBRARY_PREFIXES "")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib" ".dll")
else()
    set(CMAKE_FIND_LIBRARY_PREFIXES "lib")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".so" ".a")
endif()

message("Open3D_INCLUDE_DIRS: ${Open3D_INCLUDE_DIRS}")
message("Open3D_LIBRARY_DIRS: ${Open3D_LIBRARY_DIRS}")
message("Open3D_LIBRARIES: ${Open3D_LIBRARIES}")

find_library(OPEN3D_LIB
    NAMES
        Open3D
    PATHS
        ${Open3D_LIBRARY_DIRS}
)
if(NOT OPEN3D_LIB)
    message(FATAL_ERROR "Open3D library not found")
endif()

find_library(QHULL_LIB
    NAMES
        qhullstatic_r
    PATHS
        ${Open3D_LIBRARY_DIRS}
)
if(NOT QHULL_LIB)
    message(FATAL_ERROR "qhullstatic_r library not found")
endif()

find_library(QHULLCPP_LIB
    NAMES
        qhullcpp
    PATHS
        ${Open3D_LIBRARY_DIRS}
)
if(NOT QHULLCPP_LIB)
    message(FATAL_ERROR "qhullcpp library not found")
endif()


################################################################################
# Targets

# open3d library

add_library(open3d INTERFACE)
target_include_directories(open3d INTERFACE ${Open3D_INCLUDE_DIRS})
target_link_libraries(open3d INTERFACE
    ${OPEN3D_LIB}
    ${QHULL_LIB}
    ${QHULLCPP_LIB}
)
