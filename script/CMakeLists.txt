cmake_minimum_required(VERSION 3.18)
project(script)

# Minimal script module (header-only / interface) for now. It consumes
# `controller_command` so scripts can call commands exposed by controller.
add_library(script INTERFACE)
target_include_directories(script INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(script INTERFACE controller_command)
