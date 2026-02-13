# ui/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(ui)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 COMPONENTS Widgets REQUIRED)

# Use INTERFACE while implementation files are not yet added. This exposes
# include directories and Qt usage requirements to consumers.
add_library(ui INTERFACE)
target_include_directories(ui INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(ui INTERFACE Qt6::Widgets controller data)

# 如果 UI 模块需要 OpenGL widget (QOpenGLWidget)，link Qt6::OpenGL
find_package(Qt6 COMPONENTS OpenGL QUIET)
if(Qt6OpenGL_FOUND)
    target_link_libraries(ui INTERFACE Qt6::OpenGL)
endif()
