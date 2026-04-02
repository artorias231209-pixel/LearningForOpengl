cmake_minimum_required(VERSION 3.18)
project(data)

find_package(glm CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenCASCADE REQUIRED)
# find_package(glad CONFIG REQUIRED)

file(GLOB_RECURSE DATA_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

file(GLOB_RECURSE DATA_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
)

add_library(data SHARED
    ${DATA_HEADERS}
    ${DATA_SOURCES}
)

target_include_directories(data
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(data
    PUBLIC
        glm::glm
        OpenGL::GL
        GLEW::GLEW
        TKernel
        TKMath
        TKBRep
        TKPrim
        TKMesh
        # glad::glad  
)

target_compile_definitions(data
    PRIVATE PROJECT_DATA_LIB_BUILD
)

copy_to_global_bin(data)