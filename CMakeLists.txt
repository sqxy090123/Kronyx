cmake_minimum_required(VERSION 3.20)
project(Kronyx VERSION 0.1.0 LANGUAGES C)

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

add_library(ky_core STATIC
  src/core/math.c
  src/core/memory.c
  src/core/array.c
  src/core/hashmap.c
  src/core/string.c
  src/core/log.c
  src/core/time.c
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
  src/physics/physics.c
  src/script/script.c
  src/script/parser.c
  src/script/vm.c
)
target_link_libraries(ky_engine PUBLIC ky_core)

if(KYR_BUILD_TESTS)
  enable_testing()
  add_executable(ky_test_core tests/test_core.c)
  target_link_libraries(ky_test_core PRIVATE ky_core)
  add_test(NAME core COMMAND ky_test_core)

  add_executable(ky_test_math tests/test_math.c)
  target_link_libraries(ky_test_math PRIVATE ky_core)
  add_test(NAME math COMMAND ky_test_math)

  add_executable(ky_test_ecs tests/test_ecs.c)
  target_link_libraries(ky_test_ecs PRIVATE ky_engine)
  add_test(NAME ecs COMMAND ky_test_ecs)

  add_executable(ky_test_render tests/test_render.c)
  target_link_libraries(ky_test_render PRIVATE ky_engine)
  add_test(NAME render COMMAND ky_test_render)

  add_executable(ky_test_physics tests/test_physics.c)
  target_link_libraries(ky_test_physics PRIVATE ky_engine)
  add_test(NAME physics COMMAND ky_test_physics)

  add_executable(ky_test_script tests/test_script.c)
  target_link_libraries(ky_test_script PRIVATE ky_engine)
  add_test(NAME script COMMAND ky_test_script)
endif()
