cmake_minimum_required(VERSION 3.5)
project(nvcuvid_codecs LANGUAGES CXX)


################################################################################
# Source

set(INCLUDE_FILES
    include/NvVideoCodec.hpp
    include/NvJpegDecoder.hpp
    include/Cuda.hpp
)

set(SOURCE_FILES
    ${INCLUDE_FILES}
    src/NvVideoCodec.cpp
    src/NvJpegDecoder.cpp
    src/Cuda.cpp
)

include_directories(include ${CUDA_INCLUDE_DIR})


################################################################################
# Targets

# nvcuvid_codecs library

add_library(nvcuvid_codecs STATIC ${SOURCE_FILES})
target_include_directories(nvcuvid_codecs PUBLIC include ${CUDA_INCLUDE_DIR})
target_link_libraries(nvcuvid_codecs PUBLIC
    nvcuvid
    core
    tbb
)

install(FILES ${INCLUDE_FILES} DESTINATION include)
install(TARGETS nvcuvid_codecs DESTINATION lib)
