cmake_minimum_required(VERSION 3.5)
project(capture LANGUAGES CXX)


################################################################################
# Source

set(INCLUDE_FILES
    include/RgbdImage.hpp
    include/K4aTools.hpp
    include/CaptureManager.hpp
    include/CaptureDevice.hpp
    include/BatchProcessor.hpp
    include/TimeConverter.hpp
    include/RuntimeConfiguration.hpp
    include/CaptureSettings.hpp
)

set(SOURCE_FILES
    ${INCLUDE_FILES}
    src/RgbdImage.cpp
    src/K4aTools.cpp
    src/CaptureManager.cpp
    src/CaptureDevice.cpp
    src/BatchProcessor.cpp
    src/TimeConverter.cpp
    src/RuntimeConfiguration.cpp
    src/CaptureSettings.cpp
)

include_directories(include)


################################################################################
# Targets

# capture library

add_library(capture STATIC ${SOURCE_FILES})

target_include_directories(capture PUBLIC include)
target_link_libraries(capture PUBLIC
    core
    k4a::k4a # Kinect SDK
    depth_mesh # Mesh culling
    zdepth_igpu # Depth image compression
    capture_protocol # Network protocol
    mfx_codecs # Video encoding
    yaml # Capture settings
)

install(FILES ${INCLUDE_FILES} DESTINATION include)
install(TARGETS capture DESTINATION lib)

# capture_test app

# Disabled for now
return()

add_executable(capture_test test/capture_test.cpp)
target_link_libraries(capture_test capture)
install(TARGETS capture_test DESTINATION bin)


################################################################################
# Dependencies

if (WIN32)

    # Copy Windows DLLs:

    set(DYNLIB_TARGET capture_test)
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
