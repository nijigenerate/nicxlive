# Build Guide

## macOS

Use AppleClang with C++20 support, CMake, Ninja and Boost 1.90 with its JSON
library installed. From this repository on `nijigenerate-compatible`:

```sh
cmake --preset macos-release
cmake --build --preset macos-release
```

The Release shared library is `build/macos-compatible/libnicxlive.dylib`.
The preset disables test executables and keeps fast-math and AVX2 disabled.
It does not change the system compiler or install files globally.

## Windows

This project uses CMake + MSVC and expects Boost headers. The simplest setup on Windows is to install Boost via a classic vcpkg checkout and point CMake at the vcpkg toolchain.

## Prerequisites
- Visual Studio 2022 with Desktop C++ workload (MSVC toolchain + Windows SDK)
- Git
- PowerShell

## One-time setup (vcpkg + Boost headers)
```powershell
# Clone classic vcpkg
git clone https://github.com/microsoft/vcpkg $env:USERPROFILE/vcpkg
& "$env:USERPROFILE/vcpkg/bootstrap-vcpkg.bat"

# Install Boost headers (sufficient for nicxlive)
& "$env:USERPROFILE/vcpkg/vcpkg.exe" install boost-headers:x64-windows
```

## Configure
Use the CMake bundled by vcpkg (installed during the first vcpkg run) and pass the toolchain file so Boost resolves via config packages:
```powershell
& "$env:USERPROFILE/vcpkg/downloads/tools/cmake-3.31.10-windows/cmake-3.31.10-windows-x86_64/bin/cmake.exe" `
  -S nicxlive -B nicxlive/build `
  -DCMAKE_TOOLCHAIN_FILE=C:/Users/siget/vcpkg/scripts/buildsystems/vcpkg.cmake
```
Optional flags:
- `-DCMAKE_BUILD_TYPE=Release` for release builds (single-config generators like Ninja)
- `-DBUILD_WASM=ON` when configuring with the Emscripten toolchain (use `emcmake cmake ...`)

## Build
```powershell
& "$env:USERPROFILE/vcpkg/downloads/tools/cmake-3.31.10-windows/cmake-3.31.10-windows-x86_64/bin/cmake.exe" --build nicxlive/build
```
The static library outputs to `nicxlive/build/<config>/nicxlive.lib` (e.g., Debug/Release).

## Notes
- If you already have a CMake on PATH, you can replace the long CMake path above; still pass the vcpkg toolchain file.
- Warnings like C4819 indicate files containing non-ASCII bytes under code page 932; saving headers as UTF-8 (no BOM) will silence them.
