cmake_minimum_required(VERSION 3.5)
project(nuklear LANGUAGES CXX)


################################################################################
# Source

set(INCLUDE_FILES
    include/core_nuklear.hpp
    include/nuklear.h
    include/nuklear_glfw_gl3.h
)

set(SOURCE_FILES
    ${INCLUDE_FILES}
    src/core_nuklear.cpp
)

include_directories(include)


################################################################################
# Targets

# nuklear library

add_library(nuklear STATIC ${SOURCE_FILES})
target_include_directories(nuklear PUBLIC include)
target_link_libraries(nuklear PUBLIC
    glfw
    glad
)

install(FILES ${INCLUDE_FILES} DESTINATION include)
install(TARGETS nuklear DESTINATION lib)
