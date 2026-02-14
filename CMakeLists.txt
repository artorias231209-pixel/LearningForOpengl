cmake_minimum_required(VERSION 3.18)

file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/Debug)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/Release)

function(copy_to_global_bin target_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                ${CMAKE_SOURCE_DIR}/bin/$<CONFIG>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:${target_name}>
                ${CMAKE_SOURCE_DIR}/bin/$<CONFIG>/
        COMMENT "Copying ${target_name} ($<CONFIG>) to global bin directory"
    )
endfunction()

project(App LANGUAGES CXX)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    message(WARNING "CMAKE_TOOLCHAIN_FILE not set; please set vcpkg toolchain path (e.g. -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake)")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_EXAMPLES "Build example app" ON)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()

if(MSVC)
    add_compile_options(/permissive- /Zc:__cplusplus /utf-8)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

list(APPEND CMAKE_PREFIX_PATH "C:/Qt/6.10.2/msvc2022_64/lib/cmake")
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets)
find_package(OpenGL REQUIRED)
find_package(OpenCASCADE REQUIRED)

# add_subdirectory(data)
# add_subdirectory(controller)
add_subdirectory(ui)
# add_subdirectory(script)

add_executable(app app/src/main.cpp)

target_link_libraries(app
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::OpenGLWidgets
        OpenGL::GL
        ui
)

if(TARGET OpenCASCADE::OpenCASCADE)
    target_link_libraries(app PRIVATE OpenCASCADE::OpenCASCADE)
elseif(OpenCASCADE_FOUND)
    target_link_libraries(app PRIVATE ${OpenCASCADE_LIBRARIES})
endif()

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app/include)
target_compile_definitions(app PRIVATE UNICODE _UNICODE)

set_target_properties(app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/Release"
    PDB_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin/Debug"
)

message(STATUS "CMake generator: ${CMAKE_GENERATOR}")
message(STATUS "CMake build dir: ${CMAKE_BINARY_DIR}")
if(DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
endif()
if(DEFINED ENV{VCPKG_ROOT})
    message(STATUS "VCPKG_ROOT = $ENV{VCPKG_ROOT}")
endif()
