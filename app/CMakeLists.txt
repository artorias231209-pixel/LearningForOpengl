# app/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(app)

if(MSVC)
  add_compile_options("/utf-8")
  add_compile_options(/Zc:__cplusplus)
  add_compile_options(/permissive-)
endif()


add_executable(my_app src/main.cpp)
target_link_libraries(my_app PRIVATE ui controller data script)

# 如果使用 Qt, 需要 Qt Widgets
find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)
target_link_libraries(my_app PRIVATE Qt6::Widgets)

# 设置 RPATH 或复制 dlls 在 Windows 上（如果使用 shared libs via vcpkg）
# 这里不行，原因不明，qt动态库部署暂时用 windeployqt.exe "D:/Demo/LearningForOpengl/bin/app.exe"
# if(WIN32 OR WIN64)
#   # 获取 windeployqt 的完整路径
#   get_target_property(QT_BIN_DIR Qt6::Core IMPORTED_LOCATION)
#   get_filename_component(QT_BIN_DIR ${QT_BIN_DIR} DIRECTORY)
  
#   find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt.exe 
#     PATHS ${QT_BIN_DIR} 
#     NO_DEFAULT_PATH)
  
#   if(WINDEPLOYQT_EXECUTABLE)
#     add_custom_command(TARGET my_app POST_BUILD
#       COMMAND ${CMAKE_COMMAND} -E echo "Running windeployqt..."
#       COMMAND ${WINDEPLOYQT_EXECUTABLE} "$<TARGET_FILE:my_app>"
#     )
#   else()
#     message(WARNING "windeployqt.exe not found. Qt dependencies may not be deployed correctly.")
#   endif()
# endif()
