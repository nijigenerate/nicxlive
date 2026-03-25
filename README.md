# nic☒live

`nic☒live` is a straight C++ port of the D-based `nijilive` library. It aims to mirror the original node graph and rendering behavior as closely as possible, while documenting compatibility gaps.

## Live Demo
- WASM/WebGL sample: <https://pub-681295a74302457bbc68d86467905d4f.r2.dev/nijikan/index.html>
<img width="1609" height="1361" alt="image" src="https://github.com/user-attachments/assets/af62e206-d50b-4919-830f-a9a5a152d69f" />

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
