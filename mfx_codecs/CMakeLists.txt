cmake_minimum_required(VERSION 3.5)
project(mfx_codecs LANGUAGES CXX)


# This requires Windows and the Intel Media SDK 2019 R1 are installed.
# https://software.intel.com/en-us/media-sdk/choose-download/client


################################################################################
# Dependencies

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")

    message("Detected Windows platform: Enabling Direct3D dependencies")

    set(D3D_LIBS
        d3d9.lib
        dxva2.lib
    )

endif()


################################################################################
# Source

set(INCLUDE_FILES
    include/MfxTools.hpp
    include/MfxDirect3D9.hpp
    include/MfxVideoDecoder.hpp
    include/MfxVideoEncoder.hpp
)

set(SOURCE_FILES
    ${INCLUDE_FILES}
    src/MfxTools.cpp
    src/MfxDirect3D9.cpp
    src/MfxVideoDecoder.cpp
    src/MfxVideoEncoder.cpp
)

include_directories(include)


################################################################################
# Targets

# mfx_codecs library

add_library(mfx_codecs STATIC ${SOURCE_FILES})
target_include_directories(mfx_codecs PUBLIC include ${MFX_INCLUDE_DIR})
target_link_libraries(mfx_codecs PUBLIC
    core
    mfx
    ${D3D_LIBS}
)

install(FILES ${INCLUDE_FILES} DESTINATION include)
install(TARGETS mfx_codecs DESTINATION lib)

# mfx_test app

add_executable(mfx_test tests/mfx_test.cpp)
target_link_libraries(mfx_test
    mfx_codecs
)

install(TARGETS mfx_test DESTINATION bin)
