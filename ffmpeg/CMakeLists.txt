cmake_minimum_required(VERSION 3.5)
project(ffmpeg CXX)

if (WIN32)
    set(CMAKE_FIND_LIBRARY_PREFIXES "")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib" ".dll")
else()
    set(CMAKE_FIND_LIBRARY_PREFIXES "lib")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".so" ".a")
endif()

find_library(FFMPEG_AVCODEC_LIB
    NAMES
        avcodec
    HINTS
        ${CMAKE_SOURCE_DIR}/ffmpeg/win32/lib/
    REQUIRED
)

find_library(FFMPEG_AVFORMAT_LIB
    NAMES
        avformat
    HINTS
        ${CMAKE_SOURCE_DIR}/ffmpeg/win32/lib/
    REQUIRED
)

find_library(FFMPEG_AVUTIL_LIB
    NAMES
        avutil
    HINTS
        ${CMAKE_SOURCE_DIR}/ffmpeg/win32/lib/
    REQUIRED
)

find_library(FFMPEG_AVDEVICE_LIB
    NAMES
        avdevice
    HINTS
        ${CMAKE_SOURCE_DIR}/ffmpeg/win32/lib/
    REQUIRED
)

set(FFMPEG_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/ffmpeg/win32/include/)

message("FFMPEG_AVCODEC_LIB: ${FFMPEG_AVCODEC_LIB}")
message("FFMPEG_AVFORMAT_LIB: ${FFMPEG_AVFORMAT_LIB}")
message("FFMPEG_AVUTIL_LIB: ${FFMPEG_AVUTIL_LIB}")
message("FFMPEG_AVDEVICE_LIB: ${FFMPEG_AVDEVICE_LIB}")
message("FFMPEG_INCLUDE_DIRS: ${FFMPEG_INCLUDE_DIRS}")

if (WIN32)

    find_library(FFMPEG_AVCODEC_DYNLIB MODULE
        NAMES
            avcodec-58
        HINTS
            ${CMAKE_SOURCE_DIR}/ffmpeg/win32/bin/
        NO_CMAKE_SYSTEM_PATH
    )
    find_library(FFMPEG_AVFORMAT_DYNLIB MODULE
        NAMES
            avformat-58
        HINTS
            ${CMAKE_SOURCE_DIR}/ffmpeg/win32/bin/
        NO_CMAKE_SYSTEM_PATH
    )
    find_library(FFMPEG_AVUTIL_DYNLIB MODULE
        NAMES
            avutil-56
        HINTS
            ${CMAKE_SOURCE_DIR}/ffmpeg/win32/bin/
        NO_CMAKE_SYSTEM_PATH
    )
    find_library(FFMPEG_AVDEVICE_DYNLIB MODULE
        NAMES
            avdevice-58
        HINTS
            ${CMAKE_SOURCE_DIR}/ffmpeg/win32/bin/
        NO_CMAKE_SYSTEM_PATH
    )
    find_library(FFMPEG_AVFILTER_DYNLIB MODULE
        NAMES
            avfilter-7
        HINTS
            ${CMAKE_SOURCE_DIR}/ffmpeg/win32/bin/
        NO_CMAKE_SYSTEM_PATH
    )
    find_library(FFMPEG_POSTPROC_DYNLIB MODULE
        NAMES
            postproc-55
        HINTS
            ${CMAKE_SOURCE_DIR}/ffmpeg/win32/bin/
        NO_CMAKE_SYSTEM_PATH
    )
    find_library(FFMPEG_SWRESAMPLE_DYNLIB MODULE
        NAMES
            swresample-3
        HINTS
            ${CMAKE_SOURCE_DIR}/ffmpeg/win32/bin/
        NO_CMAKE_SYSTEM_PATH
    )
    find_library(FFMPEG_SWSCALE_DYNLIB MODULE
        NAMES
            swscale-5
        HINTS
            ${CMAKE_SOURCE_DIR}/ffmpeg/win32/bin/
        NO_CMAKE_SYSTEM_PATH
    )

    if(NOT FFMPEG_AVCODEC_DYNLIB)
        message(FATAL_ERROR "Ffmpeg avcodec dynlib not found")
    endif()
    if(NOT FFMPEG_AVFORMAT_DYNLIB)
        message(FATAL_ERROR "Ffmpeg avformat dynlib not found")
    endif()
    if(NOT FFMPEG_AVUTIL_DYNLIB)
        message(FATAL_ERROR "Ffmpeg avutil dynlib not found")
    endif()
    if(NOT FFMPEG_AVDEVICE_DYNLIB)
        message(FATAL_ERROR "Ffmpeg avdevice dynlib not found")
    endif()
    if(NOT FFMPEG_AVFILTER_DYNLIB)
        message(FATAL_ERROR "Ffmpeg avfilter dynlib not found")
    endif()
    if(NOT FFMPEG_POSTPROC_DYNLIB)
        message(FATAL_ERROR "Ffmpeg postproc dynlib not found")
    endif()
    if(NOT FFMPEG_SWRESAMPLE_DYNLIB)
        message(FATAL_ERROR "Ffmpeg swresample dynlib not found")
    endif()
    if(NOT FFMPEG_SWSCALE_DYNLIB)
        message(FATAL_ERROR "Ffmpeg swscale dynlib not found")
    endif()

    message("FFMPEG_AVCODEC_DYNLIB: ${FFMPEG_AVCODEC_DYNLIB}")
    message("FFMPEG_AVFORMAT_DYNLIB: ${FFMPEG_AVFORMAT_DYNLIB}")
    message("FFMPEG_AVUTIL_DYNLIB: ${FFMPEG_AVUTIL_DYNLIB}")
    message("FFMPEG_AVDEVICE_DYNLIB: ${FFMPEG_AVDEVICE_DYNLIB}")
    message("FFMPEG_AVFILTER_DYNLIB: ${FFMPEG_AVFILTER_DYNLIB}")
    message("FFMPEG_POSTPROC_DYNLIB: ${FFMPEG_POSTPROC_DYNLIB}")
    message("FFMPEG_SWRESAMPLE_DYNLIB: ${FFMPEG_SWRESAMPLE_DYNLIB}")
    message("FFMPEG_SWSCALE_DYNLIB: ${FFMPEG_SWSCALE_DYNLIB}")

endif()


################################################################################
# Targets

# ffmpeg library

add_library(ffmpeg INTERFACE)
target_include_directories(ffmpeg INTERFACE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(ffmpeg INTERFACE
    ${FFMPEG_AVCODEC_LIB}
    ${FFMPEG_AVFORMAT_LIB}
    ${FFMPEG_AVUTIL_LIB}
    ${FFMPEG_AVDEVICE_LIB}
)
