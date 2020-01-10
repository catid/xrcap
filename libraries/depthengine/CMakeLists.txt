cmake_minimum_required(VERSION 3.5)
project(depthengine CXX)

set(DEPTH_LIBRARY_NAMES depthengine_1_0 depthengine_2_0)

if (WIN32)
    set(CMAKE_FIND_LIBRARY_PREFIXES "")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib" ".dll")

    if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
        set(DEPTH_LIBRARY_HINTS ${CMAKE_CURRENT_LIST_DIR}/win_amd64)
    else()
        set(DEPTH_LIBRARY_HINTS ${CMAKE_CURRENT_LIST_DIR}/win_x86)
    endif()

else()
    set(CMAKE_FIND_LIBRARY_PREFIXES "lib")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".so" ".a")
endif()

find_library(DEPTH_LIBRARY MODULE
    NAMES
        ${DEPTH_LIBRARY_NAMES}
    HINTS
        ${DEPTH_LIBRARY_HINTS}
    NO_CMAKE_SYSTEM_PATH
)

if(NOT DEPTH_LIBRARY)
    message(FATAL_ERROR "${DEPTH_LIBRARY_NAMES} not found")
else()
    message("DEPTH_LIBRARY: ${DEPTH_LIBRARY}")
endif()
