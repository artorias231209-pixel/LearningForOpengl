# data/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(data)

add_library(data INTERFACE)
target_include_directories(data INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 公共宏（可选）
target_compile_definitions(data INTERFACE "PROJECT_DATA_LIB")

# 如果 data 模块需要第三方 lib, 在这里 find_package 并 target_link_libraries(data PUBLIC ...)
# e.g., 稍后 data 可能需要 boost
find_package(Boost QUIET)
if(Boost_FOUND)
  target_link_libraries(data INTERFACE Boost::boost)
endif()
