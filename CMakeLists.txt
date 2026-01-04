cmake_minimum_required(VERSION 3.20)
project(nicxlive LANGUAGES CXX)

# Require modern C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# WASM (Emscripten) build toggle
option(BUILD_WASM "Build nicxlive for WebAssembly via Emscripten" OFF)

# Boost is used for property_tree and qvm (header-only)
find_package(Boost REQUIRED)

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
    core/nodes/connected_physics_driver.cpp
    core/nodes/phys.cpp
)

add_library(nicxlive STATIC ${NICXLIVE_SOURCES})
add_library(nicxlive::nicxlive ALIAS nicxlive)

target_include_directories(nicxlive
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/core
        ${CMAKE_CURRENT_SOURCE_DIR}/core/third_party
        ${Boost_INCLUDE_DIRS}
)

target_link_libraries(nicxlive
    PUBLIC
        Boost::boost
)

target_compile_features(nicxlive PUBLIC cxx_std_20)
