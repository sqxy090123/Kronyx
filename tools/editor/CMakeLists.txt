cmake_minimum_required(VERSION 3.15)
project(kronyx_editor CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -std=c++20")

# 查找ImGui依赖
find_package(PkgConfig REQUIRED)
pkg_check_modules(IMGUI REQUIRED imgui)

# 添加ImGui include目录
include_directories(${IMGUI_INCLUDE_DIRS})

# 编辑器源文件
set(EDITOR_SOURCES
    src/editor.c
)

# 编辑器C++源文件
set(EDITOR_CPP_SOURCES
    src/viewport_panel.cpp
    src/hierarchy_panel.cpp
    src/properties_panel.cpp
    src/console_panel.cpp
)

# 编辑器头文件
set(EDITOR_HEADERS
    include/kronyx/editor.h
)

# 创建静态库
add_library(kronyx_editor STATIC ${EDITOR_SOURCES} ${EDITOR_CPP_SOURCES} ${EDITOR_HEADERS})

# 链接依赖
target_link_libraries(kronyx_editor PRIVATE
    kronyx_core
    kronyx_script
    kronyx_render
    ${IMGUI_LIBRARIES}
)

# 包含目录
target_include_directories(kronyx_editor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
)

# 包含目录
target_include_directories(kronyx_editor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/include
)

# 预留：ImGui后端适配器
# target_sources(kronyx_editor PRIVATE src/imgui_backend_gl.c)
