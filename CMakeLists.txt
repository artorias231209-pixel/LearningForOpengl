cmake_minimum_required(VERSION 3.18)

function(copy_to_global_bin target_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                ${CMAKE_SOURCE_DIR}/bin
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:${target_name}>
                ${CMAKE_SOURCE_DIR}/bin/
        COMMENT "Copying ${target_name} to global bin directory"
    )
endfunction()

# ---------------------------
# Try to auto-detect vcpkg toolchain if not passed explicitly, You may also provide VCPKG_TARGET_TRIPLET externally, e.g. -DVCPKG_TARGET_TRIPLET=x64-windows
# It is recommended to set system environment variable CMAKE_TOOLCHAIN_FILE to your vcpkg installation path(./vcpkg/scripts/buildsystems/vcpkg.cmake)
# VCPKG_ROOT 
# ---------------------------
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "CMAKE_TOOLCHAIN_FILE not set; make sure to pass vcpkg toolchain for dependencies")
endif()

project(App LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # useful for clang-format / VS Code C++ ext

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_EXAMPLES "Build example app" ON)

if (MSVC)
    # /permissive-   -> standards-conforming mode (required by Qt6)
    # /Zc:__cplusplus -> make __cplusplus reflect real standard
    # /utf-8         -> ensure source and compiler diagnostics use UTF-8
    add_compile_options(/permissive- /Zc:__cplusplus /utf-8)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

list(APPEND CMAKE_PREFIX_PATH "C:/Qt/6.10.2/msvc2022_64/lib/cmake")
find_package(Qt6 COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets REQUIRED)
find_package(OpenGL REQUIRED)

# vcpkg provides an OpenCASCADE port; find_package should work if toolchain points to vcpkg
find_package(OpenCASCADE REQUIRED)

#  add_subdirectory(data)
#  add_subdirectory(controller)
add_subdirectory(ui)
#  add_subdirectory(script)

add_executable(app
    app/src/main.cpp
    # GTest
)

# Link Qt & OpenGL
target_link_libraries(app
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::OpenGLWidgets
        OpenGL::GL
)
target_link_libraries(app PRIVATE ui)

# Link OpenCASCADE if available
if (TARGET OpenCASCADE::OpenCASCADE)
    target_link_libraries(app PRIVATE OpenCASCADE::OpenCASCADE)
elseif (OpenCASCADE_FOUND)
    # vcpkg's OpenCASCADE package exports many imported targets (e.g. TKernel,
    # TKOpenGl, TKV3d). Use the OpenCASCADE_LIBRARIES variable from the package
    # config as a best-effort list of targets to link.
    target_link_libraries(app PRIVATE ${OpenCASCADE_LIBRARIES})
else()
    message(WARNING "OpenCASCADE not linked (not found). OCC-dependent code will fail to link.")
endif()

# Include directories for the app (if you keep headers next to sources)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app/include)

# On Windows, ensure we build with Unicode defines
if (WIN32 OR WIN64)
    target_compile_definitions(app PRIVATE UNICODE _UNICODE)
endif()

set_target_properties(app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin"
)

# ---------------------------
# Helpful messages for developer
# ---------------------------
message(STATUS "CMake generator: ${CMAKE_GENERATOR}")
message(STATUS "CMake build dir: ${CMAKE_BINARY_DIR}")
if (DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
endif()
if (DEFINED ENV{VCPKG_ROOT})
    message(STATUS "VCPKG_ROOT = $ENV{VCPKG_ROOT}")
endif()
