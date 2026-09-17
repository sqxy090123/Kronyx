cmake_minimum_required(VERSION 3.20)
project(Kronyx VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(cmake/options.cmake)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wno-unused-parameter)
elseif(CMAKE_C_COMPILER_ID MATCHES "MSVC")
  add_compile_options(/W3 /WX-)
endif()

if(KYR_ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# In sanitizer builds, load LSAN suppressions via ctest ENVIRONMENT
if(KYR_ENABLE_SANITIZERS AND EXISTS "${CMAKE_SOURCE_DIR}/lsan.supp")
  set(_ky_lsan_env "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/lsan.supp")
endif()

add_library(ky_core STATIC
  src/core/math.c
  src/core/memory.c
  src/core/array.c
  src/core/hashmap.c
  src/core/string.c
  src/core/log.c
  src/core/time.c
  src/core/event.c
  src/core/input.c
  src/core/arena.c
  src/core/pool.c
  src/core/file.c
)
target_include_directories(ky_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(UNIX)
  target_link_libraries(ky_core PUBLIC m)
endif()

add_library(ky_engine STATIC
  src/ecs/ecs.c
  src/scene/scene.c
  src/resource/resource.c
  src/render/render.c
  src/render/console_backend.c
  src/render/gl_backend.c
  src/render/2d.c
  src/render/anim2d.c
  src/physics/physics.c
  src/physics/force_field.c
  src/script/script.c
  src/script/parser.c
  src/script/vm.c
  src/script/bindings.c
  src/pack/pack.c
)
target_link_libraries(ky_engine PUBLIC ky_core)
target_compile_definitions(ky_engine PRIVATE KY_PACK_ENGINE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")

# 可选 EGL/GLES 硬件后端（无则 gl_backend 回落 stub）
find_path(KY_EGL_INCLUDE_DIR EGL/egl.h)
find_library(KY_EGL_LIBRARY EGL)
find_library(KY_GLES_LIBRARY GLESv2)
if(KY_EGL_INCLUDE_DIR AND KY_EGL_LIBRARY AND KY_GLES_LIBRARY)
  target_compile_definitions(ky_engine PUBLIC KY_HAS_EGL=1)
  target_include_directories(ky_engine PUBLIC ${KY_EGL_INCLUDE_DIR})
  target_link_libraries(ky_engine PUBLIC ${KY_EGL_LIBRARY} ${KY_GLES_LIBRARY})
  message(STATUS "Kronyx: EGL/GLES backend enabled")
else()
  message(STATUS "Kronyx: EGL/GLES not found, gl backend = stub")
endif()

if(KYR_BUILD_TESTS)
  enable_testing()

  # 为所有测试注入 LSAN 抑制（仅在 sanitizer 构建下生效）
  function(ky_add_test _name)
    add_test(NAME ${_name} COMMAND ${ARGN})
    if(_ky_lsan_env)
      set_tests_properties(${_name} PROPERTIES ENVIRONMENT "${_ky_lsan_env}")
    endif()
  endfunction()

  add_executable(ky_test_core tests/test_core.c)
  target_link_libraries(ky_test_core PRIVATE ky_core)
  ky_add_test(core ky_test_core)

  add_executable(ky_test_math tests/test_math.c)
  target_link_libraries(ky_test_math PRIVATE ky_core)
  ky_add_test(math ky_test_math)

  add_executable(ky_test_ecs tests/test_ecs.c)
  target_link_libraries(ky_test_ecs PRIVATE ky_engine)
  ky_add_test(ecs ky_test_ecs)

  add_executable(ky_test_render tests/test_render.c)
  target_link_libraries(ky_test_render PRIVATE ky_engine)
  ky_add_test(render ky_test_render)

  add_executable(ky_test_physics tests/test_physics.c)
  target_link_libraries(ky_test_physics PRIVATE ky_engine)
  ky_add_test(physics ky_test_physics)

  add_executable(ky_test_script tests/test_script.c)
  target_link_libraries(ky_test_script PRIVATE ky_engine)
  ky_add_test(script ky_test_script)

  add_executable(ky_test_pack tests/test_pack.c)
  target_link_libraries(ky_test_pack PRIVATE ky_engine)
  ky_add_test(pack ky_test_pack)

  add_executable(ky_test_2d tests/test_2d.c)
  target_link_libraries(ky_test_2d PRIVATE ky_engine)
  ky_add_test(2d ky_test_2d)

  add_executable(ky_test_hooks tests/test_hooks.c)
  target_link_libraries(ky_test_hooks PRIVATE ky_engine ky_core)
  ky_add_test(hooks ky_test_hooks)

  add_executable(ky_test_event tests/test_event.c)
  target_link_libraries(ky_test_event PRIVATE ky_core)
  ky_add_test(event ky_test_event)

  add_executable(ky_test_anti_tamper tests/test_anti_tamper.c)
  target_link_libraries(ky_test_anti_tamper PRIVATE ky_antitamper ${CMAKE_DL_LIBS})
  ky_add_test(anti_tamper ky_test_anti_tamper)
  add_executable(ky_test_input tests/test_input.c)
  target_link_libraries(ky_test_input PRIVATE ky_core)
  ky_add_test(input ky_test_input)

  add_executable(ky_test_demo_loop tests/test_demo_loop.c)
  target_include_directories(ky_test_demo_loop PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/demo)
  target_link_libraries(ky_test_demo_loop PRIVATE ky_platformer ky_engine ky_core)
  ky_add_test(demo_loop ky_test_demo_loop)

  add_executable(ky_test_resource_flow tests/test_resource_flow.c)
  target_link_libraries(ky_test_resource_flow PRIVATE ky_engine ky_core)
  ky_add_test(resource_flow ky_test_resource_flow)

  add_executable(ky_test_kyx_bindings tests/test_kyx_bindings.c)
  target_link_libraries(ky_test_kyx_bindings PRIVATE ky_engine ky_core)
  ky_add_test(kyx_bindings ky_test_kyx_bindings)

  # Editor core (headless, no ImGui/GLFW dependency)
  add_library(editor_core STATIC tools/editor/src/editor_core.c)
  target_include_directories(editor_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(editor_core PUBLIC ky_engine ky_core)
  add_executable(ky_test_editor_world tests/test_editor_world.c)
  target_link_libraries(ky_test_editor_world PRIVATE editor_core ky_engine ky_core)
  ky_add_test(editor_world ky_test_editor_world)

  # Scene serialization
  add_executable(ky_test_scene_serialize tests/test_scene_serialize.c)
  target_link_libraries(ky_test_scene_serialize PRIVATE ky_engine ky_core)
  ky_add_test(scene_serialize ky_test_scene_serialize)

  add_executable(ky_test_spritesheet tests/test_spritesheet.c)
  target_link_libraries(ky_test_spritesheet PRIVATE ky_engine ky_core)
  ky_add_test(spritesheet ky_test_spritesheet)

  add_executable(ky_test_extreme tests/test_extreme.c)
  target_link_libraries(ky_test_extreme PRIVATE ky_engine ky_core)
  ky_add_test(extreme ky_test_extreme)


endif()

# 资产管理工具：生成 demo 用的 8x8 RGBA 方块资产（无 PNG 依赖）
add_executable(ky_mk_demo_asset tools/mk_demo_asset.c)
add_custom_target(demo_asset
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets
  COMMAND $<TARGET_FILE:ky_mk_demo_asset> ${CMAKE_CURRENT_SOURCE_DIR}/assets/hero_8x8.bin
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Generating assets/hero_8x8.bin"
)

# 编辑器模块（可选）
option(KYR_BUILD_EDITOR "Build ImGui editor" OFF)
if(KYR_BUILD_EDITOR)
  add_subdirectory(tools/editor)
  if(KYR_BUILD_TESTS)
    add_executable(ky_test_editor tests/test_editor.c)
    target_link_libraries(ky_test_editor PRIVATE ky_editor)
     add_test(NAME editor COMMAND ky_test_editor)
   endif()
endif()

# Engine library (GLFW windowing + game loop)
add_library(ky_runtime STATIC src/engine/glfw.c src/engine/engine.c)
target_include_directories(ky_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ky_runtime PUBLIC ky_core)

if(WIN32)
  # vcpkg 提供 find_package(glfw3 CONFIG)，target 名为 glfw
  find_package(glfw3 CONFIG QUIET)
  if(glfw3_FOUND)
    target_link_libraries(ky_runtime PUBLIC glfw)
    message(STATUS "Kronyx: GLFW3 found via vcpkg")
  else()
    message(WARNING "Kronyx: GLFW3 not found, ky_runtime will be stub")
  endif()
else()
  find_library(GLFW3_LIBRARY glfw PATHS /usr/lib/x86_64-linux-gnu /opt/homebrew/lib)
  find_path(GLFW3_INCLUDE_DIR GLFW/glfw3.h PATHS /usr/include /opt/homebrew/include)
  target_link_libraries(ky_runtime PUBLIC ${GLFW3_LIBRARY} ${CMAKE_DL_LIBS})
  target_include_directories(ky_runtime PUBLIC ${GLFW3_INCLUDE_DIR})
  if(UNIX AND NOT APPLE)
    target_link_libraries(ky_runtime PUBLIC X11 Xi Xrandr)
  endif()
endif()

# Anti-tamper library (game exe integration)
add_library(ky_antitamper STATIC src/engine/anti_tamper_game.c)
target_include_directories(ky_antitamper PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ky_antitamper PUBLIC ky_core)
if(WIN32)
  target_link_libraries(ky_antitamper PRIVATE crypt32)
  target_compile_definitions(ky_antitamper PUBLIC KY_ANTITEMPER_BUILDING_LIB)
elseif(APPLE)
  target_link_libraries(ky_antitamper PRIVATE "-framework Security" "-framework CoreFoundation")
  set_target_properties(ky_antitamper PROPERTIES POSITION_INDEPENDENT_CODE ON)
else()
  target_link_libraries(ky_antitamper PRIVATE ${CMAKE_DL_LIBS})
  target_compile_definitions(ky_antitamper PRIVATE HAVE_OPENSSL)
  target_link_libraries(ky_antitamper PRIVATE OpenSSL::Crypto)
  # Set rpath to find companion .so in same directory
  set_target_properties(ky_antitamper PROPERTIES
    BUILD_RPATH "\$ORIGIN"
    INSTALL_RPATH "\$ORIGIN"
  )
endif()

# KyAntiTamper validation library (shared, cross-platform)
add_library(KyAntiTamper SHARED src/engine/anti_tamper_dll.cpp)
set_source_files_properties(src/engine/anti_tamper_dll.cpp PROPERTIES LANGUAGE CXX)
target_compile_definitions(KyAntiTamper PRIVATE KY_ANTITEMPER_BUILDING_LIB)
target_include_directories(KyAntiTamper PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(WIN32)
  target_link_libraries(KyAntiTamper PRIVATE crypt32)
  set_target_properties(KyAntiTamper PROPERTIES
    OUTPUT_NAME "KyAntiTamper"
    SUFFIX ".dll"
  )
elseif(APPLE)
  target_link_libraries(KyAntiTamper PRIVATE "-framework Security" "-framework CoreFoundation")
  set_target_properties(KyAntiTamper PROPERTIES
    OUTPUT_NAME "KyAntiTamper"
    SUFFIX ".dylib"
    POSITION_INDEPENDENT_CODE ON
  )
else()
  find_package(OpenSSL REQUIRED)
  target_compile_definitions(KyAntiTamper PRIVATE HAVE_OPENSSL)
  target_link_libraries(KyAntiTamper PUBLIC OpenSSL::Crypto)
  set_target_properties(KyAntiTamper PROPERTIES
    OUTPUT_NAME "KyAntiTamper"
    SUFFIX ".so"
    POSITION_INDEPENDENT_CODE ON
    BUILD_RPATH "\$ORIGIN"
    INSTALL_RPATH "\$ORIGIN"
  )
endif()

# G1: 2D platformer vertical slice (shared demo world built on ECS + 2D + physics)
add_library(ky_platformer STATIC src/demo/platformer_world.c)
target_include_directories(ky_platformer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/demo)
target_link_libraries(ky_platformer PUBLIC ky_engine ky_core)

# First runnable engine demo: 2D platformer (headed loop or headless smoke)
add_executable(ky_demo src/demo/main.c)
target_include_directories(ky_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/demo)
target_link_libraries(ky_demo PRIVATE ky_platformer ky_runtime ky_antitamper ky_engine ky_core)
# Set rpath so demo can find libKyAntiTamper.so in same directory
set_target_properties(ky_demo PROPERTIES
  BUILD_RPATH "\$ORIGIN"
  INSTALL_RPATH "\$ORIGIN"
)

# Simple render demo
add_executable(ky_test_render_demo tests/test_render_demo.c)
target_link_libraries(ky_test_render_demo PRIVATE ky_engine)
