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
find_package(Vulkan REQUIRED)

if(NOT Vulkan_GLSLC_EXECUTABLE AND DEFINED VCPKG_INSTALLED_DIR)
    find_program(Vulkan_GLSLC_EXECUTABLE NAMES glslc
        HINTS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/shaderc"
    )
endif()

if(NOT Vulkan_GLSLC_EXECUTABLE)
    message(FATAL_ERROR "A Vulkan SDK with glslc is required to build the Vulkan renderer.")
endif()

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

set(VULKAN_SHADER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh.frag
)
set(VULKAN_SHADER_OUTPUTS)
foreach(VULKAN_SHADER_SOURCE IN LISTS VULKAN_SHADER_SOURCES)
    get_filename_component(VULKAN_SHADER_NAME ${VULKAN_SHADER_SOURCE} NAME)
    set(VULKAN_SHADER_OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/generated_shaders/${VULKAN_SHADER_NAME}.spv)
    add_custom_command(
        OUTPUT ${VULKAN_SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory
                ${CMAKE_CURRENT_BINARY_DIR}/generated_shaders
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} --target-env=vulkan1.0
                -o ${VULKAN_SHADER_OUTPUT} ${VULKAN_SHADER_SOURCE}
        DEPENDS ${VULKAN_SHADER_SOURCE}
        VERBATIM
    )
    set_source_files_properties(${VULKAN_SHADER_OUTPUT}
        PROPERTIES QT_RESOURCE_ALIAS "${VULKAN_SHADER_NAME}.spv")
    list(APPEND VULKAN_SHADER_OUTPUTS ${VULKAN_SHADER_OUTPUT})
endforeach()
add_custom_target(vulkan_shaders DEPENDS ${VULKAN_SHADER_OUTPUTS})
add_dependencies(ui vulkan_shaders)

set_source_files_properties(${VULKAN_SHADER_OUTPUTS} PROPERTIES GENERATED TRUE)
qt_add_resources(ui vulkan_shader_resources
    PREFIX "/vulkan"
    FILES ${VULKAN_SHADER_OUTPUTS}
)

target_include_directories(ui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/data/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_definitions(ui PRIVATE UI_LIBRARY)

target_link_libraries(ui
    PRIVATE       
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        data
        Vulkan::Vulkan
)

if (TARGET Qt6::OpenGL AND TARGET Qt6::OpenGLWidgets)
    target_link_libraries(ui PRIVATE Qt6::OpenGL Qt6::OpenGLWidgets)
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
