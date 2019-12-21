cmake_minimum_required(VERSION 3.5)
project(core LANGUAGES CXX)


################################################################################
# Source

file(GLOB INCLUDE_FILES include/core*.hpp)
file(GLOB SOURCE_FILES src/core*.cpp)

include_directories(include)


################################################################################
# Targets

# core library

add_library(core STATIC ${INCLUDE_FILES} ${SOURCE_FILES})
target_include_directories(core PUBLIC include)
target_link_libraries(core PUBLIC
    spdlog::spdlog
    mimalloc-static
    Threads::Threads
)

install(FILES ${INCLUDE_FILES} DESTINATION include)
install(TARGETS core DESTINATION lib)

# core test application

add_executable(core_test tests/core_test.cpp)
target_link_libraries(core_test PRIVATE core)

install(TARGETS core_test DESTINATION bin)
