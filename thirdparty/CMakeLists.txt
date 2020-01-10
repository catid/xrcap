cmake_minimum_required(VERSION 2.8)

message("External projects CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

include(ExternalProject)

# Wenzel's tbb
ExternalProject_Add(
    tbb
    GIT_REPOSITORY https://github.com/wjakob/tbb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
    BUILD_COMMAND  ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE}
    SOURCE_DIR     ${CMAKE_SOURCE_DIR}/src/tbb
    CMAKE_ARGS     -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DTBB_BUILD_SHARED=OFF -DTBB_BUILD_TBBMALLOC=OFF -DTBB_BUILD_TBBMALLOC_PROXY=OFF -DTBB_BUILD_TESTS=OFF -DTBB_BUILD_STATIC=ON -DCMAKE_INSTALL_PREFIX=${CMAKE_SOURCE_DIR}/bin/tbb
)

# Wenzel's updated nanogui
ExternalProject_Add(
    nanogui
    GIT_REPOSITORY https://github.com/mitsuba-renderer/nanogui.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
    BUILD_COMMAND  ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE}
    SOURCE_DIR     ${CMAKE_SOURCE_DIR}/src/nanogui
    CMAKE_ARGS     -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DNANOGUI_BUILD_EXAMPLE=OFF -DNANOGUI_BUILD_SHARED=OFF -DNANOGUI_BUILD_PYTHON=OFF -DNANOGUI_INSTALL=ON -DCMAKE_INSTALL_PREFIX=${CMAKE_SOURCE_DIR}/bin/nanogui
)

# yaml-cpp
# Dependency of Open3D
# Disabled: Tests, tools
ExternalProject_Add(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG        yaml-cpp-0.6.3
    GIT_SHALLOW    TRUE
    BUILD_COMMAND  ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE}
    SOURCE_DIR     ${CMAKE_SOURCE_DIR}/src/yaml-cpp
    CMAKE_ARGS     -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DYAML_CPP_BUILD_TESTS=OFF -DYAML_CPP_BUILD_TOOLS=OFF -DYAML_CPP_BUILD_CONTRIB=OFF -DYAML_CPP_INSTALL=ON -DCMAKE_INSTALL_PREFIX=${CMAKE_SOURCE_DIR}/bin/yaml-cpp
)

# libsodium
# Copies the files into src folder but does not build.
ExternalProject_Add(
    libsodium
    GIT_REPOSITORY    https://github.com/jedisct1/libsodium.git
    GIT_TAG           1.0.18
    GIT_SHALLOW       TRUE
    CONFIGURE_COMMAND ""
    BUILD_COMMAND     ""
    INSTALL_COMMAND   ""
    SOURCE_DIR        ${CMAKE_SOURCE_DIR}/src/libsodium
)

# jpegturbo
# Using nasm for optimizations
ExternalProject_Add(
    jpegturbo
    GIT_REPOSITORY  https://github.com/libjpeg-turbo/libjpeg-turbo.git
    GIT_TAG         2.0.2
    GIT_SHALLOW     TRUE
    BUILD_COMMAND   ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE}
    SOURCE_DIR      ${CMAKE_SOURCE_DIR}/src/jpegturbo
    CMAKE_ARGS      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_ASM_NASM_COMPILER=${CMAKE_SOURCE_DIR}/nasm/nasm.exe -DCMAKE_INSTALL_PREFIX=${CMAKE_SOURCE_DIR}/bin/jpegturbo
)

# spdlog : Logging library
# Disabled: Benchmarks, examples
ExternalProject_Add(
    spdlog
    GIT_REPOSITORY  https://github.com/gabime/spdlog.git
    GIT_TAG         v1.3.1
    GIT_SHALLOW     TRUE
    BUILD_COMMAND   ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE}
    SOURCE_DIR      ${CMAKE_SOURCE_DIR}/src/spdlog
    CMAKE_ARGS      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DSPDLOG_BUILD_BENCH=OFF -DSPDLOG_BUILD_EXAMPLES=OFF -DSPDLOG_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=${CMAKE_SOURCE_DIR}/bin/spdlog
)

# mimalloc : Microsoft's memory allocator
ExternalProject_Add(
    mimalloc
    GIT_REPOSITORY  https://github.com/microsoft/mimalloc.git
    GIT_TAG         v1.0.6
    GIT_SHALLOW     TRUE
    BUILD_COMMAND   ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE}
    SOURCE_DIR      ${CMAKE_SOURCE_DIR}/src/mimalloc
    CMAKE_ARGS      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${CMAKE_SOURCE_DIR}/bin/mimalloc
)

# GLFW : Cross-platform OpenGL windowing library
ExternalProject_Add(
    glfw3
    GIT_REPOSITORY  https://github.com/glfw/glfw.git
    GIT_TAG         3.3
    GIT_SHALLOW     TRUE
    BUILD_COMMAND   ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE}
    SOURCE_DIR      ${CMAKE_SOURCE_DIR}/src/glfw
    CMAKE_ARGS      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${CMAKE_SOURCE_DIR}/bin/glfw
)

# Open3D : Pointcloud library that implements Fast Global Registration and Color ICP
# Disabled: Jupyter notebook, Python
ExternalProject_Add(
    Open3D
    GIT_REPOSITORY  https://github.com/intel-isl/Open3D.git
    GIT_TAG         v0.8.0
    GIT_SHALLOW     TRUE
    BUILD_COMMAND   ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE}
    SOURCE_DIR      ${CMAKE_SOURCE_DIR}/src/Open3D
    CMAKE_ARGS      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DENABLE_JUPYTER=OFF -DBUILD_PYTHON_MODULE=OFF -DBUILD_PYBIND11=OFF -DBUILD_CPP_EXAMPLES=OFF -DCMAKE_INSTALL_PREFIX=${CMAKE_SOURCE_DIR}/bin/Open3D
)

# Azure Kinect SDK : Microsoft's SDK for the depth sensor
# Disabled: Clang format validation, docs
ExternalProject_Add(
    AzureKinectSDK
    GIT_REPOSITORY  https://github.com/microsoft/Azure-Kinect-Sensor-SDK.git
    GIT_TAG         v1.3.0
    GIT_SHALLOW     TRUE
    BUILD_COMMAND   ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE}
    SOURCE_DIR      ${CMAKE_SOURCE_DIR}/src/azurekinectsdk
    CMAKE_ARGS      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DK4A_VALIDATE_CLANG_FORMAT=OFF -DK4A_BUILD_DOCS=OFF -DCMAKE_INSTALL_PREFIX=${CMAKE_SOURCE_DIR}/bin/azurekinectsdk
)

# RapidJSON
ExternalProject_Add(
    RapidJSON
    GIT_REPOSITORY  https://github.com/Tencent/rapidjson.git
    GIT_TAG         master
    GIT_SHALLOW     TRUE
    BUILD_COMMAND   ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE}
    SOURCE_DIR      ${CMAKE_SOURCE_DIR}/src/rapidjson
    CMAKE_ARGS      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DRAPIDJSON_BUILD_DOC=OFF -DRAPIDJSON_BUILD_EXAMPLES=OFF -DRAPIDJSON_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=${CMAKE_SOURCE_DIR}/bin/rapidjson
)
