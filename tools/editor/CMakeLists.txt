cmake_minimum_required(VERSION 3.15)
project(kronyx_editor CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -std=c++20")

# 查找ImGui依赖
find_package(PkgConfig REQUIRED)
pkg_check_modules(IMGUI REQUIRED imgui)

# 查找GLFW和OpenGL依赖
pkg_check_modules(GLFW REQUIRED glfw3)
find_package(OpenGL REQUIRED)

# 添加ImGui include目录
include_directories(${IMGUI_INCLUDE_DIRS})

# 添加GLFW和OpenGL include目录
include_directories(${GLFW_INCLUDE_DIRS})
include_directories(${OPENGL_INCLUDE_DIRS})

# 编辑器C++源文件（包括ImGui的C++代码）
set(EDITOR_CPP_SOURCES
    src/editor.c
    src/editor_main.c
    src/viewport_panel.cpp
    src/hierarchy_panel.cpp
    src/properties_panel.cpp
    src/console_panel.cpp
)

# 编辑器C++源文件
set(EDITOR_CPP_SOURCES
    src/editor.c
    src/editor_main.c
    src/viewport_panel.cpp
    src/hierarchy_panel.cpp
    src/properties_panel.cpp
    src/console_panel.cpp
)

# 编辑器头文件
set(EDITOR_HEADERS
    include/kronyx/editor.h
)

# 创建可执行文件
add_executable(kronyx_editor
    ${EDITOR_SOURCES}
    ${EDITOR_CPP_SOURCES}
    ${EDITOR_HEADERS}
)

# 链接依赖
target_link_libraries(kronyx_editor PRIVATE
    ky_core
    ky_engine
    ${IMGUI_LIBRARIES}
    ${GLFW_LIBRARIES}
    ${OPENGL_LIBRARIES}
    OpenGL::GL
)

# 确保C和C++正确链接
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_link_libraries(kronyx_editor PUBLIC stdc++)
endif()

# 链接GLM（用于矩阵运算）
find_package(glm QUIET)
if(NOT glm_FOUND)
    # 如果系统没有GLM，使用内置的简单矩阵实现
    message(STATUS "GLM not found, using built-in matrix utilities")
    target_compile_definitions(kronyx_editor PRIVATE KYR_USE_BUILTIN_MATRIX)
else()
    message(STATUS "GLM found, using for matrix operations")
    target_link_libraries(kronyx_editor PUBLIC glm::glm)
endif()

# 包含目录
target_include_directories(kronyx_editor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
)

# Windows平台特殊处理
if(WIN32)
    target_link_libraries(kronyx_editor PUBLIC opengl32)
endif()

# Unix平台特殊处理
if(UNIX)
    target_link_libraries(kronyx_editor PUBLIC m)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_link_libraries(kronyx_editor PUBLIC stdc++)
    endif()
endif()

# 安装规则
install(TARGETS kronyx_editor DESTINATION bin)