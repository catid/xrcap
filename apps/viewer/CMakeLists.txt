cmake_minimum_required(VERSION 3.5)
project(viewer LANGUAGES CXX)


################################################################################
# Source

set(VIEWER_SOURCE_FILES
    src/ViewerWindow.hpp
    src/ViewerWindow.cpp
    src/viewer_main.cpp
    src/ViewerSettings.hpp
    src/ViewerSettings.cpp
    src/Gltf2Writer.hpp
    src/Gltf2Writer.cpp
)


################################################################################
# Targets

# viewer application

add_executable(viewer ${VIEWER_SOURCE_FILES})
target_link_libraries(viewer PRIVATE
    core
    capture_client_igpu
    depth_mesh
    glfw
    glad
    nuklear
    vectormath
    yaml
    nfd # nativefiledialog: Portable file open dialog
    rapidjson # glTF encoder
    jpegturbo # glTF encoder
)

install(TARGETS viewer DESTINATION bin)

if (WIN32)

    # Copy Windows DLLs:

    set(DYNLIB_TARGET viewer)
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
