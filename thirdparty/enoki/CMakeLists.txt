cmake_minimum_required(VERSION 3.5)
project(enoki CXX)


################################################################################
# Targets

# enoki library (header-only)

add_library(enoki INTERFACE)
target_include_directories(enoki INTERFACE extern/include)
