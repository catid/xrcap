cmake_minimum_required(VERSION 3.5)
project(mfx LANGUAGES CXX)


################################################################################
# Source

set(INCLUDE_FILES
    include/mfxadapter.h
    include/mfxastructures.h
    include/mfxaudio++.h
    include/mfxaudio.h
    include/mfxbrc.h
    include/mfxcamera.h
    include/mfxcommon.h
    include/mfxdefs.h
    include/mfxdispatcherprefixedfunctions.h
    include/mfxenc.h
    include/mfxfei.h
    include/mfxfeihevc.h
    include/mfxjpeg.h
    include/mfxla.h
    include/mfxmvc.h
    include/mfxpak.h
    include/mfxpcp.h
    include/mfxplugin++.h
    include/mfxplugin.h
    include/mfxsc.h
    include/mfxscd.h
    include/mfxsession.h
    include/mfxstructures.h
    include/mfxvideo++.h
    include/mfxvideo.h
    include/mfxvp8.h
    include/mfxvp9.h
    include/mfxvstructures.h
)

set(SOURCE_FILES
    ${INCLUDE_FILES}
    src/main.cpp
    src/mfxaudio_exposed_functions_list.h
    src/mfx_critical_section.cpp
    src/mfx_critical_section.h
    src/mfx_dispatcher.cpp
    src/mfx_dispatcher.h
    src/mfx_dispatcher_defs.h
    src/mfx_dispatcher_log.cpp
    src/mfx_dispatcher_log.h
    src/mfx_dxva2_device.cpp
    src/mfx_dxva2_device.h
    src/mfx_exposed_functions_list.h
    src/mfx_function_table.cpp
    src/mfx_library_iterator.cpp
    src/mfx_library_iterator.h
    src/mfx_load_dll.cpp
    src/mfx_load_dll.h
    src/mfx_load_plugin.cpp
    src/mfx_load_plugin.h
    src/mfx_plugin_hive.cpp
    src/mfx_plugin_hive.h
    src/mfx_vector.h
    src/mfx_win_reg_key.cpp
    src/mfx_win_reg_key.h
)

include_directories(include)


################################################################################
# Targets

# mfx library

add_library(mfx STATIC ${SOURCE_FILES})
target_include_directories(mfx PUBLIC include ${MFX_INCLUDE_DIR})

install(FILES ${INCLUDE_FILES} DESTINATION include)
install(TARGETS mfx DESTINATION lib)
