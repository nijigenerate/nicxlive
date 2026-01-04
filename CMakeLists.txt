cmake_minimum_required(VERSION 3.20)
project(nicxlive LANGUAGES CXX)

# Allow FindBoost while CMake 3.31+ defaults to Boost Config packages only.
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()

# Require modern C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# WASM (Emscripten) build toggle
option(BUILD_WASM "Build nicxlive for WebAssembly via Emscripten" OFF)

# Boost is used for property_tree and qvm (header-only)
# Prefer a Config package (common on Windows/vcpkg) and fall back to FindBoost.
# On MSVC we default to static, multithreaded runtime unless the user overrides.
if(WIN32 AND NOT DEFINED Boost_USE_STATIC_LIBS)
  set(Boost_USE_STATIC_LIBS ON)
endif()
if(NOT DEFINED Boost_USE_MULTITHREADED)
  set(Boost_USE_MULTITHREADED ON)
endif()

find_package(Boost 1.70 QUIET CONFIG)
if(NOT Boost_FOUND)
  find_package(Boost 1.70 REQUIRED)
endif()

if(TARGET Boost::headers)
  set(NICXLIVE_BOOST_HEADERS_TARGET Boost::headers)
elseif(TARGET Boost::boost)
  set(NICXLIVE_BOOST_HEADERS_TARGET Boost::boost)
else()
  message(FATAL_ERROR "Boost headers target not found. Ensure Boost is installed and provides Boost::headers or Boost::boost.")
endif()

if(BUILD_WASM)
  if(NOT EMSCRIPTEN AND NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    message(FATAL_ERROR "BUILD_WASM=ON requires Emscripten toolchain (use emcmake).")
  endif()
  message(STATUS "Configuring for WASM (Emscripten)")
  add_compile_options(-sALLOW_MEMORY_GROWTH=1 -sWASM=1)
  # Linker options for typical modularized builds; adjust as needed for your integration.
  # add_link_options(-sMODULARIZE=1 -sEXPORT_NAME=\"Module\")
endif()

set(NICXLIVE_SOURCES
    core/texture.cpp
    core/puppet.cpp
    core/unity_native.cpp
    core/runtime_state.cpp
    core/timing.cpp
    core/math/camera.cpp
    utils/stb_image_write_impl.cpp
    core/render/common.cpp
    core/render/commands.cpp
    core/render/command_emitter.cpp
    core/render/backend_queue.cpp
    core/render/graph_builder.cpp
    core/render/profiler.cpp
    core/render/scheduler.cpp
    core/render/immediate.cpp
    core/render/shared_deform_buffer.cpp
    core/render/commands.cpp
    core/nodes/node.cpp
    core/nodes/part.cpp
    core/nodes/mask.cpp
    core/nodes/composite.cpp
    core/nodes/dynamic_composite.cpp
    core/nodes/projectable.cpp
    core/nodes/drawable.cpp
    core/nodes/mesh_group.cpp
    core/nodes/grid_deformer.cpp
    core/nodes/path_deformer.cpp
    core/nodes/simple_physics_driver.cpp
    core/nodes/curve.cpp
    core/nodes/deformable.cpp
    core/nodes/filter.cpp
    core/nodes/deformer/drivers/phys.cpp
    core/math/triangle.cpp
    core/math/mat3.cpp
    # math/transform.hpp is header-only
)

add_library(nicxlive STATIC ${NICXLIVE_SOURCES})
add_library(nicxlive::nicxlive ALIAS nicxlive)

target_include_directories(nicxlive
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/core
        ${CMAKE_CURRENT_SOURCE_DIR}/core/third_party
)
if(Boost_INCLUDE_DIRS)
    target_include_directories(nicxlive PUBLIC ${Boost_INCLUDE_DIRS})
endif()

target_link_libraries(nicxlive
    PUBLIC
        ${NICXLIVE_BOOST_HEADERS_TARGET}
)

target_compile_features(nicxlive PUBLIC cxx_std_20)
