cmake_minimum_required(VERSION 3.5)
project(vectormath CXX)

include_directories(include)


################################################################################
# Targets

# vectormath library (header-only)

add_library(vectormath INTERFACE)
target_include_directories(vectormath INTERFACE include)
