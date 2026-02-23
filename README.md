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
| 1 | core/nodes/node.d | 1458 | core/nodes/node.cpp | 1107 | [compat-node](doc/compat-node.md) |
| 2 | core/nodes/filter.d | 146 | core/nodes/filter.cpp | 65 | [compat-filter](doc/compat-filter.md) |
| 3 | core/nodes/deformable.d | 209 | core/nodes/deformable.cpp | 220 | [compat-deformable](doc/compat-deformable.md) |
| 4 | core/nodes/drawable.d | 742 | core/nodes/drawable.cpp | 966 | [compat-drawable](doc/compat-drawable.md) |
| 5 | core/nodes/part/package.d | 895 | core/nodes/part.cpp | 827 | [compat-part](doc/compat-part.md) |
| 6 | core/nodes/mask/package.d | 149 | core/nodes/mask.cpp | 77 | [compat-mask](doc/compat-mask.md) |
| 7 | core/nodes/composite/projectable.d | 1335 | core/nodes/projectable.cpp | 1001 | [compat-projectable](doc/compat-projectable.md) |
| 8 | core/nodes/composite/composite.d | 674 | core/nodes/composite.cpp | 595 | [compat-composite](doc/compat-composite.md) |
| 9 | core/nodes/composite/dcomposite.d | 41 | core/nodes/dynamic_composite.cpp | 15 | [compat-dcomposite](doc/compat-dcomposite.md) |
| 10 | core/nodes/meshgroup/package.d | 611 | core/nodes/mesh_group.cpp | 651 | [compat-meshgroup](doc/compat-meshgroup.md) |
| 11 | core/nodes/deformer/base.d | 16 | core/nodes/deformer_base.hpp | 33 | [compat-deformable](doc/compat-deformable.md) |
| 12 | core/nodes/deformer/grid.d | 782 | core/nodes/grid_deformer.cpp | 657 | [compat-grid_deformer](doc/compat-grid_deformer.md) |
| 13 | core/nodes/deformer/path.d | 1334 | core/nodes/path_deformer.cpp | 1411 | [compat-path_deformer](doc/compat-path_deformer.md) |
| 14 | core/nodes/drivers/package.d | 58 | core/nodes/driver.hpp | 39 | [compat-driver](doc/compat-driver.md) |
| 15 | core/nodes/drivers/simplephysics.d | 1003 | core/nodes/simple_physics_driver.cpp | 874 | [compat-simple_physics_driver](doc/compat-simple_physics_driver.md) |
| nijilive (total) | 9,453 | nicxlive (total) | 8,538 |  |

### Render layer (D → C++)
| # | nijilive (D) | Lines | nicxlive (C++) | Lines | Compat note |
| - | --- | ---:| --- | ---:| --- |
| RQ1 | core/render/commands.d | 172 | core/render/commands.cpp | 115 | [compat-commands](doc/compat-commands.md) |
| RQ2 | core/render/command_emitter.d | 25 | core/render/command_emitter.cpp | 249 | [compat-command_emitter](doc/compat-command_emitter.md) |
| RQ3 | core/render/graph_builder.d | 266 | core/render/graph_builder.cpp | 235 | [compat-render-graph](doc/compat-render-graph.md) |
| RQ4 | core/render/scheduler.d | 98 | core/render/scheduler.cpp | 53 | [compat-scheduler](doc/compat-scheduler.md) |
| RQ5 | core/render/shared_deform_buffer.d | 240 | core/render/shared_deform_buffer.cpp | 185 | [compat-queue](doc/compat-queue.md) |
| RQ6 | core/render/immediate.d | 42 | core/render/immediate.cpp | 28 | [compat-queue](doc/compat-queue.md) |
| RQ7 | core/render/passes.d | 39 | core/render/render_pass.hpp | 20 | [compat-queue](doc/compat-queue.md) |
| RQ8 | core/render/profiler.d | 119 | core/render/profiler.cpp | 108 | [compat-queue](doc/compat-queue.md) |
| RQ9 | core/render/backends/queue/package.d | 457 | core/render/backend_queue.cpp | 201 | [compat-queue](doc/compat-queue.md) |
| RQ10 | core/render/backends/opengl/* | 3,619 | (not yet ported) | 0 | (WIP) |
| RQ11 | core/render/backends/directx12/* | 2,985 | (not yet ported) | 0 | (WIP) |
| RT1 | core/texture.d | 571 | core/texture.cpp | 210 | [compat-texture](doc/compat-texture.md) |

### fmt layer (D → C++)
| # | nijilive (D) | Lines | nicxlive (C++) | Lines | Compat note |
| - | --- | ---:| --- | ---:| --- |
| F1 | fmt/package.d | 339 | fmt/fmt.hpp | 217 | [compat-fmt](doc/compat-fmt.md) |
| F2 | fmt/io.d | 69 | fmt/io.hpp | 59 | [compat-io](doc/compat-io.md) |
| F3 | fmt/serialize.d | 99 | fmt/serialize.hpp | 94 | [compat-serialize](doc/compat-serialize.md) |
| F4 | fmt/binfmt.d | 33 | fmt/binfmt.hpp | 33 | [compat-binfmt](doc/compat-binfmt.md) |

### Unity integration (D → C++)
| # | nijilive (D) | Lines | nicxlive (C++) | Lines | Compat note |
| - | --- | ---:| --- | ---:| --- |
| U1 | integration/unity.d | 1018 | core/unity_native.cpp + core/unity_native.hpp | 1242 | [compat-unity_native](doc/compat-unity_native.md) |

### Math / Common (D → C++)
| # | nijilive (D) | Lines | nicxlive (C++) | Lines | Compat note |
| - | --- | ---:| --- | ---:| --- |
| M1 | math/veca.d | 673 | core/math/veca.hpp | 1065 | [compat-vec2array](doc/compat-vec2array.md) |
| M2 | math/transform.d | 175 | core/math/transform.hpp | 210 | [compat-transform](doc/compat-transform.md) |
| M3 | math/camera.d | 75 | core/math/camera.hpp + core/math/camera.cpp | 59 | [compat-runtime_state](doc/compat-runtime_state.md) |
| M4 | math/triangle.d | 165 | core/math/triangle.hpp + core/math/triangle.cpp | 282 | [compat-triangle](doc/compat-triangle.md) |
| C1 | core/nodes/common.d | 290 | core/nodes/common.hpp | 132 | [compat-common](doc/compat-common.md) |

See `doc/compat-*.md` for detailed compatibility notes and remaining gaps.
