# nic☒live

`nic☒live` is a straight C++ port of the D-based `nijilive` library. It aims to mirror the original node graph and rendering behavior as closely as possible, while documenting compatibility gaps.

## Live Demo
- WASM/WebGL sample: <https://pub-681295a74302457bbc68d86467905d4f.r2.dev/nijikan/index.html>
<img width="2560" height="1392" alt="image" src="https://github.com/user-attachments/assets/fa9c87ed-ff36-4beb-b0dd-18a320433a5d" />


## Goals
- [x] Implement full compatibility with `nijilive` in C++ (node graph + render behavior)
- [x] Serve as a drop-in native library for the Unity plugin (replace D-based plugin)
- [x] Enable WASM builds for web environments
- [ ] Replace D-based development to avoid platform/library constraints (graphics backends, toolchains, etc.)

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
See `doc/compat-*.md` for detailed compatibility notes and remaining gaps.
## Render resource lifetime

Resource retirement is internal. Normal `inUpdate()` processing and DLL model
unload/load-failure handling drain pending releases. Detaching the renderer with
`inSetRenderBackend(nullptr)` also drains before releasing the old backend, while
its graphics context is still current. Applications do not call a
shared-buffer collection API. Model mutation and rendering remain serialized.

Drawable destruction unregisters CPU vertex/UV/deform arrays and queues the index
handle with a weak reference to its backend. GPU calls are deferred to the rendering
thread. `RenderBackend::disposeDrawableBuffers(id)` delivers individual releases;
the backend owns its allocation and compaction policy without a collection hook.
Dirty arrays are uploaded even when empty, so deleting the last model also clears
backend copies. The last CPU array removal frees atlas bookkeeping capacity.

`nicxlive_render_resource_lifetime_test` verifies deferred backend notifications,
surviving Drawable data, repeated creation/deletion, empty-array notification and
that pending retirements do not keep the backend alive.
