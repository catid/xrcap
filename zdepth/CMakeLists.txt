cmake_minimum_required(VERSION 3.5)
project(zdepth LANGUAGES CXX)


################################################################################
# Source

set(INCLUDE_FILES
    include/zdepth_lossy.hpp
    include/zdepth_lossless.hpp
)

set(SOURCE_FILES
    ${INCLUDE_FILES}
    src/zdepth_lossy.cpp
    src/zdepth_lossless.cpp
    src/libdivide.h
)

include_directories(include)


################################################################################
# Targets

install(FILES ${INCLUDE_FILES} DESTINATION include)

# zdepth_nvidia library
# This one uses Nvidia codecs

add_library(zdepth_nvidia STATIC ${SOURCE_FILES})
target_link_libraries(zdepth_nvidia PUBLIC
    zstd
    core
    mfx_codecs
    nvcuvid_codecs
)
target_compile_definitions(zdepth_nvidia PUBLIC ZDEPTH_MFX=1 ZDEPTH_NVCUVID=1)
target_include_directories(zdepth_nvidia PUBLIC include)

install(TARGETS zdepth_nvidia DESTINATION lib)

# zdepth_igpu library
# This is used to remove the obnoxious NVidia dependencies that refuse to install on laptops without Nvidia cards.

add_library(zdepth_igpu STATIC ${SOURCE_FILES})
target_link_libraries(zdepth_igpu PUBLIC
    zstd
    core
    mfx_codecs
)
target_compile_definitions(zdepth_igpu PUBLIC ZDEPTH_MFX=1)
target_include_directories(zdepth_igpu PUBLIC include)

install(TARGETS zdepth_igpu DESTINATION lib)

# zdepth test application

add_executable(zdepth_tests tests/zdepth_tests.cpp)
target_link_libraries(zdepth_tests zdepth_nvidia)

install(TARGETS zdepth_tests DESTINATION bin)
