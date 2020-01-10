cmake_minimum_required(VERSION 3.5)
project(capture_server LANGUAGES CXX)


################################################################################
# Source

set(SOURCE_FILES
    src/CaptureFrontend.hpp
    src/CaptureFrontend.cpp
    src/CaptureServer.hpp
    src/CaptureServer.cpp
    src/ServerSettings.hpp
    src/ServerSettings.cpp
    src/Main.cpp
)

include_directories(include)


################################################################################
# Targets

# Capture server application

add_executable(capture_server ${SOURCE_FILES})
target_link_libraries(capture_server PRIVATE
    capture
    tonk
    capture_protocol
    nuklear
    glad
    yaml
    sodium
)

install(TARGETS capture_server DESTINATION bin)


################################################################################
# Dependencies

if (WIN32)

    # Copy Windows DLLs:

    set(DYNLIB_TARGET capture_server)
    set(DYNLIB_DEST "$<TARGET_FILE_DIR:${DYNLIB_TARGET}>")

    message("Copying font to build folder")
    add_custom_command(TARGET ${DYNLIB_TARGET} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_SOURCE_DIR}/resources/FiraCode-Retina.ttf"
            "${DYNLIB_DEST}"
    )

    message("Copying k4a library to build folder")
    add_custom_command(TARGET ${DYNLIB_TARGET} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:k4a::k4a>"
            "${DYNLIB_DEST}"
    )

    message("Copying depth library to build folder: ${DEPTH_LIBRARY}")
    add_custom_command(TARGET ${DYNLIB_TARGET} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${DEPTH_LIBRARY}"
            "${DYNLIB_DEST}"
    )

    message("Copying nvJPEG library to build folder: ${NVJPEG_DYNLIB}")
    add_custom_command(TARGET ${DYNLIB_TARGET} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${NVJPEG_DYNLIB}"
            "${DYNLIB_DEST}"
    )

    message("Copying Nvidia Performance Primitives Color Conversion library to build folder: ${NPPICC_DYNLIB}")
    add_custom_command(TARGET ${DYNLIB_TARGET} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${NPPICC_DYNLIB}"
            "${DYNLIB_DEST}"
    )

    message("Copying Nvidia Performance Primitives Core library to build folder: ${NPPC_DYNLIB}")
    add_custom_command(TARGET ${DYNLIB_TARGET} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${NPPC_DYNLIB}"
            "${DYNLIB_DEST}"
    )

endif()
