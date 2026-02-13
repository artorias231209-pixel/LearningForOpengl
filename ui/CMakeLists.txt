# ui/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(ui LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC_SEARCH_PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
)

message(${CMAKE_CURRENT_SOURCE_DIR})

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt6 COMPONENTS OpenGL OpenGLWidgets QUIET)

file(GLOB_RECURSE UI_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

file(GLOB_RECURSE UI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
)

file(GLOB_RECURSE UI_FORMS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.ui"
)

add_library(ui SHARED
    ${UI_HEADERS}
    ${UI_SOURCES}
    ${UI_FORMS}
)

target_include_directories(ui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(ui PRIVATE UI_LIBRARY)

target_link_libraries(ui
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        # controller
        # data
)

if (TARGET Qt6::OpenGL AND TARGET Qt6::OpenGLWidgets)
    target_link_libraries(ui PUBLIC Qt6::OpenGL Qt6::OpenGLWidgets)
endif()

target_compile_features(ui PUBLIC cxx_std_17)

if (MSVC)
    target_compile_options(ui PRIVATE /permissive- /Zc:__cplusplus /utf-8)
endif()

message(STATUS "UI module sources auto-collected:")
message(STATUS "  Headers: ${UI_HEADERS}")
message(STATUS "  Sources: ${UI_SOURCES}")
message(STATUS "  Forms:   ${UI_FORMS}")

# 拷贝构建结果到 bin 目录
copy_to_global_bin(ui)
# add_custom_command(TARGET ui POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E make_directory
#             ${CMAKE_SOURCE_DIR}/../bin
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#             $<TARGET_FILE:ui>
#             ${CMAKE_SOURCE_DIR}/../bin/
# )
