# controller/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(controller)

# controller 包含一个 core library, 在尚未实现源码时使用 INTERFACE
# 以便其它模块可以消费 include 路径和 usage requirements
add_library(controller INTERFACE)
target_include_directories(controller INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_definitions(controller INTERFACE "CONTROLLER_IMPL")
target_link_libraries(controller INTERFACE data Qt6::Core)

# 若需要 OpenGL (controller 发起 render commands), 可以声明依赖
find_package(OpenGL QUIET)
if(OpenGL_FOUND)
  target_link_libraries(controller INTERFACE OpenGL::GL)
endif()

# command 作为 header-only 小模块（在 include/controller/Command.h）
add_library(controller_command INTERFACE)
target_include_directories(controller_command INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
# controller 依赖 command
target_link_libraries(controller INTERFACE controller_command)
