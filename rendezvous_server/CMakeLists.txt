cmake_minimum_required(VERSION 3.5)
project(rendezvous_server LANGUAGES CXX)


################################################################################
# Source

set(INCLUDE_FILES
    include/RendezvousServer.hpp
)

set(SOURCE_FILES
    ${INCLUDE_FILES}
    src/RendezvousServer.cpp
    app/Main.cpp
)

include_directories(include)


################################################################################
# Targets

# Rendezvous server application

add_executable(rendezvous_server
    ${SOURCE_FILES}
    app/Main.cpp
)
target_link_libraries(rendezvous_server PRIVATE
    core
    tonk
    capture_protocol
)

install(TARGETS rendezvous_server DESTINATION bin)
