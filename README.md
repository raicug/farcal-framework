# Farcal

Farcal is an early-stage C++20 UI framework project.

The repository currently contains the framework core, a Win32 window layer, a DirectX 11 backend, and a DX11 smoke test that exercises the first immediate-mode API.

This project is not production-ready yet. The public framework API is intentionally small while the core rendering and UI layers are being built.

## Current status

- C++20 static library target: `farcal::framework`
- Public include root: `framework/include`
- Immediate-mode frame API: `farcal::frame(...)`
- Basic widgets: `farcal::text(...)`, `farcal::button(...)`
- Text hierarchy: `farcal::title_text(...)`, `farcal::section_text(...)`, `farcal::text_secondary(...)`
- Primary actions: `farcal::primary_button(...)`
- ImGui-style UI windows: `farcal::window_panel(...)`
- Style stack: `farcal::push_style_color(...)`, `farcal::push_style_var(...)`
- Draw layers: `farcal::background_renderer()`, `farcal::main_renderer()`, `farcal::foreground_renderer()`
- ID stack: `farcal::push_id(...)`, `farcal::pop_id()`, `farcal::current_id(...)`
- Win32 window wrapper with WndProc hook support
- Draggable UI panels
- Mouse-wheel scrolling inside UI panels
- Clipped draw commands for scrollable content
- DX11 backend with dynamic indexed buffers, scissor batches, resize handling, and cached DirectWrite text formats
- CMake-based build with Ninja presets
- Windows DX11 smoke test executable: `dx11-window-test`

## Repository layout

```text
.
|-- CMakeLists.txt
|-- CMakePresets.json
|-- framework/
|   |-- CMakeLists.txt
|   |-- include/framework/
|   |-- src/
|   |-- src/backends/
|   `-- src/win32/
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

## Basic API shape

```cpp
farcal::create_context();

farcal::window window({
    .title = L"Farcal",
    .width = 1280,
    .height = 720,
});

farcal::dx11_renderer renderer(window);

while (window.poll_events()) {
    farcal::begin_frame(window.consume_input());

    farcal::background_renderer().commands.push_back({
        .type = farcal::draw_command_type::filled_rect,
        .bounds = {{0.0F, 0.0F}, {1280.0F, 720.0F}},
        .tint = {0.055F, 0.055F, 0.064F, 1.0F},
    });

    farcal::frame([&] {
        farcal::window_panel("Farcal", [&] {
            farcal::title_text("Viewport Tools");
            farcal::text_secondary("Immediate-mode controls for engine tooling.");
            farcal::separator();

            farcal::section_text("Scene");
            farcal::primary_button("Select Entity", [&] {
                window.cancel_next_poll();
            });
        });
    });

    farcal::end_frame();
    renderer.render(farcal::draw());
    renderer.present();
}

farcal::destroy_context();
```

The Win32 wrapper also supports a message hook:

```cpp
window.set_wnd_proc_hook([](HWND handle, UINT message, WPARAM wparam, LPARAM lparam) -> std::optional<LRESULT> {
    return std::nullopt;
});
```

Calling `window.cancel_next_poll()` makes the next `window.poll_events()` return `false`, which lets app code break out of a `while (window.poll_events())` loop from inside a widget callback.

The default text face is Inter through DirectWrite. If Inter is not installed on the system, DirectWrite will choose its normal fallback for that family.

## Development notes

- Keep public headers under `framework/include/framework/`.
- Keep platform backends under `framework/src/backends/`.
- Keep platform window code under `framework/src/win32/`.
- Add executable smoke tests under `tests/<test-name>/`.
- Prefer small, focused changes while the core API is still taking shape.
- Do not document planned APIs as if they already exist.

## Roadmap

The next useful milestones are:

- Add a framework-owned font atlas for non-DirectWrite backends.
- Add layout helpers.
- Add checkbox and slider widgets.
- Add resize handling in the DX11 backend.
- Add focused tests or examples for each public feature as it lands.
