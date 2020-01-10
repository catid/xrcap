cmake_minimum_required(VERSION 3.5)
project(capture_client LANGUAGES CXX)


################################################################################
# Source

set(INCLUDE_FILES
    include/capture_client.h
    include/CaptureClient.hpp
    include/NetClient.hpp
    include/FrameInfo.hpp
    include/CaptureDecoder.hpp
    include/DejitterQueue.hpp
    include/FileFormat.hpp
    include/FileWriter.hpp
    include/FileReader.hpp
)

set(SOURCE_FILES
    ${INCLUDE_FILES}
    src/capture_client.cpp
    src/CaptureClient.cpp
    src/NetClient.cpp
    src/CaptureDecoder.cpp
    src/DejitterQueue.cpp
    src/FileFormat.cpp
    src/FileWriter.cpp
    src/FileReader.cpp
)

include_directories(include)


################################################################################
# Targets

# capture_client_igpu library

add_library(capture_client_igpu STATIC ${SOURCE_FILES})

target_include_directories(capture_client_igpu PUBLIC include)
target_link_libraries(capture_client_igpu PUBLIC
    core
    tonk
    capture_protocol
    depth_mesh
    sodium
    zdepth_igpu
    tbb
    mfx_codecs
)

install(FILES ${INCLUDE_FILES} DESTINATION include)
install(TARGETS capture_client_igpu DESTINATION lib)

# xrcap library

add_library(xrcap SHARED ${SOURCE_FILES})

target_include_directories(xrcap PUBLIC include)
target_compile_definitions(xrcap PRIVATE XRCAP_BUILDING=1 XRCAP_DLL=1)
target_link_libraries(xrcap PUBLIC capture_client_igpu)

install(FILES ${INCLUDE_FILES} DESTINATION include)
install(TARGETS xrcap DESTINATION lib)

# Test application

add_executable(capture_client_test tests/Test.cpp)
target_link_libraries(capture_client_test PRIVATE xrcap)

install(TARGETS capture_client_test DESTINATION bin)
