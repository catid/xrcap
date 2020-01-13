cmake_minimum_required(VERSION 3.5)
project(jpegturbo CXX)

if (WIN32)
    set(CMAKE_FIND_LIBRARY_PREFIXES "")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib" ".dll")
else()
    set(CMAKE_FIND_LIBRARY_PREFIXES "lib")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".so" ".a")
endif()

find_library(TURBOJPEG_STATIC_LIB
    NAMES
        # turbojpeg-static.lib for static linkage
        turbojpeg-static
    HINTS
        ${CMAKE_SOURCE_DIR}/thirdparty/bin/jpegturbo/lib/
    NO_CMAKE_SYSTEM_PATH
)

find_path(TURBOJPEG_INCLUDE_DIRS turbojpeg.h
    PATHS
        ${CMAKE_SOURCE_DIR}/thirdparty/bin/jpegturbo/include/
    REQUIRED
)

message("TURBOJPEG_STATIC_LIB: ${TURBOJPEG_STATIC_LIB}")
message("TURBOJPEG_INCLUDE_DIRS: ${TURBOJPEG_INCLUDE_DIRS}")

find_library(TURBOJPEG_DYNLIB MODULE
    NAMES
        # turbojpeg.dll for dynamic linkage
        turbojpeg
    HINTS
        ${CMAKE_SOURCE_DIR}/thirdparty/bin/jpegturbo/bin/
    NO_CMAKE_SYSTEM_PATH
)

if(NOT TURBOJPEG_DYNLIB)
    message(FATAL_ERROR "turbojpeg dynlib not found")
else()
    message("TURBOJPEG_DYNLIB: ${TURBOJPEG_DYNLIB}")
endif()


################################################################################
# Targets

# jpegturbo library

add_library(jpegturbo INTERFACE)
target_include_directories(jpegturbo INTERFACE ${TURBOJPEG_INCLUDE_DIRS})
target_link_libraries(jpegturbo INTERFACE ${TURBOJPEG_STATIC_LIB})
