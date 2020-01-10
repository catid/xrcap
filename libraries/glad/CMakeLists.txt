cmake_minimum_required(VERSION 3.5)
project(glad LANGUAGES CXX)


################################################################################
# Source

set(INCLUDE_FILES
    include/glad.h
    include/khrplatform.h
    include/GLCore.hpp
    include/ImageTilingRender.hpp
    include/VideoMeshRender.hpp
    include/TrackballCamera.hpp
)

set(SOURCE_FILES
    ${INCLUDE_FILES}
    src/glad.c
    src/GLCore.cpp
    src/ImageTilingRender.cpp
    src/VideoMeshRender.cpp
    src/TrackballCamera.cpp
)


################################################################################
# Targets

include_directories(include)

add_library(glad STATIC ${SOURCE_FILES})
target_include_directories(glad PUBLIC include)
target_link_libraries(glad PUBLIC
    core
    vectormath
)

install(FILES ${INCLUDE_FILES} DESTINATION include)
install(TARGETS glad DESTINATION lib)
