cmake_minimum_required(VERSION 3.5)
project(xrcap)


################################################################################
# Build Options

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# In debug mode, add -DDEBUG
add_compile_options("$<$<CONFIG:DEBUG>:-DDEBUG>")

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 -D_ENABLE_EXTENDED_ALIGNED_STORAGE=1")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /FORCE") # /PROFILE
    #add_definitions("/MP")
    #add_definitions("/MT")
else()
    # Warnings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")

    # Remove Asio warning
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-local-typedefs")

    # Static library: -fPIC
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -march=native -fstack-protector")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native")
endif()

IF(MSVC)
   ADD_DEFINITIONS(/arch:AVX)
ENDIF(MSVC)


################################################################################
# Build Dependencies

# If dependencies are not downloaded/built yet:
find_package(k4a
    PATHS
        "${CMAKE_SOURCE_DIR}/thirdparty/bin/azurekinectsdk/lib/cmake/k4a"
)
if (NOT k4a_FOUND)
    set(THIRDPARTY_DIR ${CMAKE_SOURCE_DIR}/thirdparty)
    file(MAKE_DIRECTORY ${THIRDPARTY_DIR}/build)

    # "cd build"
    # "cmake -DCMAKE_BUILD_TYPE=Release .."
    # "cmake --build ."
    execute_process(
        COMMAND
            ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} ..
        WORKING_DIRECTORY
            ${THIRDPARTY_DIR}/build
    )
    execute_process(
        COMMAND
            ${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE}
        WORKING_DIRECTORY
            ${THIRDPARTY_DIR}/build
    )
endif()


################################################################################
# Dependencies

find_package(mimalloc 1.0 EXACT
    PATHS
        "${CMAKE_SOURCE_DIR}/thirdparty/bin/mimalloc/lib/mimalloc-1.0/cmake"
    REQUIRED
)
find_package(glfw3 3.3.0 EXACT
    PATHS
        "${CMAKE_SOURCE_DIR}/thirdparty/bin/glfw/lib/cmake"
    REQUIRED
)
find_package(k4a 1.3.0 EXACT
    PATHS
        "${CMAKE_SOURCE_DIR}/thirdparty/bin/azurekinectsdk/lib/cmake/k4a"
    REQUIRED
)

# Use exact version of spdlog because k4a includes a different version
find_package(spdlog 1.3.1 EXACT
    PATHS
        "${CMAKE_SOURCE_DIR}/thirdparty/bin/spdlog/lib/cmake/spdlog"
    REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)


################################################################################
# Common Tools

# Must come first because it defines our version of spdlog
if (NOT TARGET core)
    add_subdirectory(libraries/core core)
endif()


################################################################################
# Thirdparty Libraries

if (NOT TARGET sodium)
    add_subdirectory(thirdparty/sodium sodium)
endif()
if (NOT TARGET vectormath)
    add_subdirectory(thirdparty/vectormath vectormath)
endif()
if (NOT TARGET nuklear)
    add_subdirectory(thirdparty/nuklear nuklear)
endif()
if (NOT TARGET jpegturbo)
    add_subdirectory(thirdparty/jpegturbo jpegturbo)
endif()
if (NOT TARGET tbb)
    add_subdirectory(thirdparty/tbb tbb)
endif()
if (NOT TARGET open3d)
    add_subdirectory(thirdparty/open3d open3d)
endif()
if (NOT TARGET enoki)
    add_subdirectory(thirdparty/enoki enoki)
endif()
if (NOT TARGET zstd)
    add_subdirectory(thirdparty/zstd zstd)
endif()
if (NOT TARGET yaml)
    add_subdirectory(thirdparty/yaml yaml)
endif()
if (NOT TARGET nfd)
    add_subdirectory(thirdparty/nfd nfd)
endif()
if (NOT TARGET apriltag)
    add_subdirectory(thirdparty/apriltag apriltag)
endif()
if (NOT TARGET rapidjson)
    add_subdirectory(thirdparty/rapidjson rapidjson)
endif()


################################################################################
# Firstparty Libraries

if (NOT TARGET depthengine)
    add_subdirectory(libraries/depthengine depthengine)
endif()
if (NOT TARGET glad)
    add_subdirectory(libraries/glad glad)
endif()
if (NOT TARGET tonk)
    add_subdirectory(libraries/tonk tonk)
endif()
if (NOT TARGET depth_mesh)
    add_subdirectory(libraries/depth_mesh depth_mesh)
endif()
if (NOT TARGET capture_protocol)
    add_subdirectory(libraries/capture_protocol capture_protocol)
endif()
if (NOT TARGET capture)
    add_subdirectory(libraries/capture capture)
endif()


################################################################################
# Video Codecs

if (NOT TARGET nvcuvid)
    add_subdirectory(codecs/nvcuvid nvcuvid)
endif()
if (NOT TARGET nvcuvid_codecs)
    add_subdirectory(codecs/nvcuvid_codecs nvcuvid_codecs)
endif()
if (NOT TARGET mfx)
    add_subdirectory(codecs/mfx mfx)
endif()
if (NOT TARGET mfx_codecs)
    add_subdirectory(codecs/mfx_codecs mfx_codecs)
endif()
if (NOT TARGET zdepth)
    add_subdirectory(codecs/zdepth zdepth)
endif()


################################################################################
# Apps

if (NOT TARGET capture_client_igpu)
    add_subdirectory(apps/capture_client capture_client)
endif()
if (NOT TARGET rendezvous_server)
    add_subdirectory(apps/rendezvous_server rendezvous_server)
endif()
if (NOT TARGET capture_server)
    add_subdirectory(apps/capture_server capture_server)
endif()
if (NOT TARGET viewer)
    add_subdirectory(apps/viewer viewer)
endif()



# Print all variables?
if (0)
    get_cmake_property(_variableNames VARIABLES)
    list (SORT _variableNames)
    foreach (_variableName ${_variableNames})
        message(STATUS "${_variableName}=${${_variableName}}")
    endforeach()
endif()
