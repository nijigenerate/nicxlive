# nic☒live

`nic☒live` is a straight C++ port of the D-based `nijilive` library. It aims to mirror the original node graph and rendering behavior as closely as possible, while documenting compatibility gaps.

## Goals
- Implement full compatibility with `nijilive` in C++ (node graph + render behavior)
- Serve as a drop-in native library for the Unity plugin (replace D-based plugin)
- Enable WASM builds for web environments
- Replace D-based development to avoid platform/library constraints (graphics backends, toolchains, etc.)

## Dependencies
- CMake 3.20+
- C++20 compiler
- Boost headers (uses `property_tree` / `qvm`; header-only is sufficient)
- stb_image (bundled at `core/third_party/stb_image.h`)
- (WASM build) Emscripten toolchain + Node.js (Emscripten uses node as emulator) and Boost headers available to the toolchain

## Build
### Generic (Unix-like / cross-platform)
```bash
cd nicxlive
cmake -S . -B build
cmake --build build
```

### Windows (MSVC + vcpkg)
See `BUILD.md` for a step-by-step guide (vcpkg classic, Boost headers, CMake toolchain file).

### Build for WASM (Emscripten)
```bash
# Set up emsdk and PATH to emcc, then:
emcmake cmake -S . -B build-wasm -DBUILD_WASM=ON -DCMAKE_BUILD_TYPE=Release \
  -DBoost_INCLUDE_DIR=$(brew --prefix)/include
cmake --build build-wasm
```
`BUILD_WASM=ON` is only honored when using the Emscripten toolchain. If you need specific exports, uncomment and adjust the `add_link_options` hints in `CMakeLists.txt`.

## Progress

### Node Layer (D → C++ line counts with compat docs)
| # | nijilive (D) | Lines | nicxlive (C++) | Lines | Compat note |
| - | --- | ---:| --- | ---:| --- |
| 1 | nodes/package.d | 1271 | core/nodes/node.cpp | 1063 | [compat-node](doc/compat-node.md) |
| 2 | nodes/filter.d | 124 | core/nodes/filter.cpp | 59 | [compat-filter](doc/compat-filter.md) |
| 3 | nodes/deformable.d | 185 | core/nodes/deformable.cpp | 136 | [compat-deformable](doc/compat-deformable.md) |
| 4 | nodes/drawable.d | 645 | core/nodes/drawable.cpp | 874 | [compat-drawable](doc/compat-drawable.md) |
| 5 | nodes/part/package.d | 715 | core/nodes/part.cpp | 635 | [compat-part](doc/compat-part.md) |
| 6 | nodes/mask/package.d | 120 | core/nodes/mask.cpp | 65 | [compat-mask](doc/compat-mask.md) |
| 7 | nodes/composite/projectable.d | 1116 | core/nodes/projectable.cpp | 743 | [compat-projectable](doc/compat-projectable.md) |
| 8 | nodes/composite/package.d | 298 | core/nodes/composite.cpp | 213 | [compat-composite](doc/compat-composite.md) |
| 9 | nodes/composite/dcomposite.d | 34 | core/nodes/dynamic_composite.cpp | 11 | [compat-dcomposite](doc/compat-dcomposite.md) |
| 10 | nodes/meshgroup/package.d | 537 | core/nodes/mesh_group.cpp | 512 | [compat-meshgroup](doc/compat-meshgroup.md) |
| 11 | nodes/deformer/base.d | 14 | core/nodes/deformer_base.hpp | 24 | [compat-deformable](doc/compat-deformable.md) |
| 12 | nodes/deformer/grid.d | 677 | core/nodes/grid_deformer.cpp | 572 | [compat-grid_deformer](doc/compat-grid_deformer.md) |
| 13 | nodes/deformer/path.d | 1204 | core/nodes/path_deformer.cpp | 1260 | [compat-path_deformer](doc/compat-path_deformer.md) |
| 14 | nodes/drivers/package.d | 47 | core/nodes/driver.hpp | 29 | (WIP) |
| 15 | nodes/drivers/simplephysics.d | 874 | core/nodes/simple_physics_driver.cpp | 599 | (WIP) |
| nijilive (total) | 7,861 | nicxlive (total) | 6,795 |  |

### Render layer (D → C++)
| # | nijilive (D) | Lines | nicxlive (C++) | Lines | Compat note |
| - | --- | ---:| --- | ---:| --- |
| RQ1 | render/commands.d | 195 | core/render/commands.cpp | 99 | [compat-commands](doc/compat-commands.md) |
| RQ2 | render/command_emitter.d | 23 | core/render/command_emitter.cpp | 197 | [compat-command_emitter](doc/compat-command_emitter.md) |
| RQ3 | render/graph_builder.d | 226 | core/render/graph_builder.cpp | 185 | [compat-render-graph](doc/compat-render-graph.md) |
| RQ4 | render/scheduler.d | 84 | core/render/scheduler.cpp | 35 | [compat-scheduler](doc/compat-scheduler.md) |
| RQ5 | render/shared_deform_buffer.d | 194 | core/render/shared_deform_buffer.cpp | 120 | [compat-queue](doc/compat-queue.md) |
| RQ6 | render/immediate.d | 38 | core/render/immediate.cpp | 22 | [compat-queue](doc/compat-queue.md) |
| RQ7 | render/passes.d | 34 | core/render/render_pass.hpp | 14 | [compat-queue](doc/compat-queue.md) |
| RQ8 | render/profiler.d | 93 | core/render/profiler.cpp | 93 | [compat-queue](doc/compat-queue.md) |
| RQ9 | render/backends/queue/package.d | 358 | core/render/backend_queue.cpp | 214 | [compat-queue](doc/compat-queue.md) |
| RQ10 | render/backends/opengl/* | 3,682 | (not yet ported) | 0 | (WIP) |
| RQ11 | render/backends/directx12/* | 2,989 | (not yet ported) | 0 | (WIP) |
| RT1 | core/texture.d | 494 | core/texture.cpp | 166 | [compat-texture](doc/compat-texture.md) |

### fmt layer (D → C++)
| # | nijilive (D) | Lines | nicxlive (C++) | Lines | Compat note |
| - | --- | ---:| --- | ---:| --- |
| F1 | fmt/package.d | 277 | fmt/fmt.hpp | 180 | [compat-fmt](doc/compat-fmt.md) |
| F2 | fmt/io.d | 61 | fmt/io.hpp | 49 | [compat-io](doc/compat-io.md) |
| F3 | fmt/serialize.d | 83 | fmt/serialize.hpp | 53 | [compat-serialize](doc/compat-serialize.md) |
| F4 | fmt/binfmt.d | 27 | fmt/binfmt.hpp | 26 | [compat-binfmt](doc/compat-binfmt.md) |

### Unity integration (D → C++)
| # | nijilive (D) | Lines | nicxlive (C++) | Lines | Compat note |
| - | --- | ---:| --- | ---:| --- |
| U1 | integration/unity.d | 691 | core/unity_native.cpp + unity_native.hpp | 714 | [compat-unity_native](doc/compat-unity_native.md) |

### Math / Common (D → C++)
| # | nijilive (D) | Lines | nicxlive (C++) | Lines | Compat note |
| - | --- | ---:| --- | ---:| --- |
| M1 | math/veca.d | 585 | core/math/veca.hpp | 796 | [compat-vec2array](doc/compat-vec2array.md) |
| M2 | math/transform.d | 150 | core/math/transform.hpp | 108 | [compat-transform](doc/compat-transform.md) |
| M3 | math/camera.d | 62 | core/math/camera.hpp + camera.cpp | 45 | [compat-runtime_state](doc/compat-runtime_state.md) |
| M4 | math/triangle.d | 140 | core/math/triangle.hpp + triangle.cpp | 33 | [compat-triangle](doc/compat-triangle.md) |
| C1 | core/nodes/common.d | 242 | core/nodes/common.hpp | 65 | [compat-common](doc/compat-common.md) |

See `doc/compat-*.md` for detailed compatibility notes and remaining gaps.
