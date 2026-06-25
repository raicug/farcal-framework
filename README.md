# Farcal

Farcal is an early-stage C++20 UI framework project.

The repository currently contains the framework library skeleton plus a Win32/DirectX 11 smoke test that opens a window, creates a D3D11 device and swap chain, and clears the back buffer each frame.

This project is not production-ready yet. The public framework API is intentionally small while the core rendering and UI layers are being built.

## Current status

- C++20 static library target: `farcal::framework`
- Public include root: `framework/include`
- Current public API: `farcal::version()`
- CMake-based build with Ninja presets
- Windows DX11 smoke test executable: `dx11-window-test`

## Repository layout

```text
.
|-- CMakeLists.txt
|-- CMakePresets.json
|-- framework/
|   |-- CMakeLists.txt
|   |-- include/framework/framework.hpp
|   `-- src/main.cpp
`-- tests/
    |-- CMakeLists.txt
    `-- dx11-window-test/
        |-- CMakeLists.txt
        `-- main.cpp
```

## Requirements

- CMake 3.24 or newer
- A C++20 compiler
- Ninja, when using the provided presets
- Windows SDK
- DirectX 11 system libraries (`d3d11`, `dxgi`, `dxguid`)

The included DX11 test is Windows-only.

## Building

Configure and build the debug preset:

```sh
cmake --preset ninja-debug
cmake --build --preset debug
```

Configure and build the release preset:

```sh
cmake --preset ninja-release
cmake --build --preset release
```

Build outputs are written under:

```text
bin/<channel>/<config>/
```

For the provided Ninja presets, this means:

```text
bin/ninja/debug/
bin/ninja/release/
```

## Running the DX11 smoke test

After a debug build:

```sh
bin/ninja/debug/dx11-window-test.exe
```

After a release build:

```sh
bin/ninja/release/dx11-window-test.exe
```

The test opens a 1280x720 Win32 window and clears it with a DirectX 11 render target. It is useful as a quick check that the framework target links correctly and that the local Windows/DX11 toolchain is working.

## Using the library from CMake

When this repository is added as a CMake subdirectory, link against the alias target:

```cmake
add_subdirectory(path/to/farcal)

target_link_libraries(your_target
    PRIVATE
        farcal::framework
)
```

Then include the public header:

```cpp
#include <framework/framework.hpp>

const char* version = farcal::version();
```

## Development notes

- Keep public headers under `framework/include/framework/`.
- Keep implementation files under `framework/src/`.
- Add executable smoke tests under `tests/<test-name>/`.
- Prefer small, focused changes while the core API is still taking shape.
- Do not document planned APIs as if they already exist.

## Roadmap

The next useful milestones are:

- Define the core application/window abstraction.
- Add renderer-owned DX11 objects behind framework APIs.
- Add input/event handling.
- Add a minimal immediate-mode or retained-mode UI surface.
- Add focused tests or examples for each public feature as it lands.
