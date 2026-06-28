# Farcal

Farcal is an early-stage C++20 UI framework project.

The repository currently contains the framework core, a Win32 window layer, DirectX 10 and DirectX 11 backends, and smoke tests that exercise the immediate-mode API.

This project is not production-ready yet. The public framework API is intentionally small while the core rendering and UI layers are being built.

## Current status

- C++20 static library target: `farcal::framework`
- Public include root: `framework/include`
- Immediate-mode frame API: `farcal::Frame(...)`
- Basic widgets: `farcal::Text(...)`, `farcal::Button(...)`, `farcal::Slider<T>(...)`, `farcal::ListItem(...)`
- Text hierarchy: `farcal::TitleText(...)`, `farcal::SectionText(...)`, `farcal::TextSecondary(...)`
- Primary actions: `farcal::PrimaryButton(...)`
- ImGui-style UI windows: `farcal::WindowPanel(...)`
- Style stack: `farcal::PushStyleColor(..., farcal::Color::Rgb(...))`, `farcal::PushStyleVar(...)`
- Draw layers: `farcal::BackgroundRenderer()`, `farcal::MainRenderer()`, `farcal::ForegroundRenderer()`
- ID stack: `farcal::PushId(...)`, `farcal::PopId()`, `farcal::CurrentId(...)`
- Win32 window wrapper with WndProc hook support
- Draggable UI panels
- Mouse-wheel scrolling inside UI panels
- Clipped draw commands for scrollable content
- DX10 and DX11 backends with dynamic indexed buffers, scissor batches, resize handling, and cached DirectWrite text formats
- CMake-based build with Ninja presets
- Windows smoke test executables: `dx10-window-test`, `dx11-window-test`

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
    |-- component_demo.hpp
    |-- CMakeLists.txt
    |-- dx10-window-test/
    |   |-- CMakeLists.txt
    |   `-- main.cpp
    `-- dx11-window-test/
        |-- CMakeLists.txt
        `-- main.cpp
```

## Requirements

- CMake 3.24 or newer
- A C++20 compiler
- Ninja, when using the provided presets
- Windows SDK
- DirectX 10 and 11 system libraries (`d3d10_1`, `d3d11`, `dxgi`, `dxguid`)

The included DirectX tests are Windows-only.

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

## Running the smoke tests

After a debug build:

```sh
bin/ninja/debug/dx11-window-test.exe
bin/ninja/debug/dx10-window-test.exe
```

After a release build:

```sh
bin/ninja/release/dx11-window-test.exe
bin/ninja/release/dx10-window-test.exe
```

Each test opens a 1280x720 Win32 window and renders the same immediate-mode UI through one backend. They are useful as quick checks that the framework target links correctly and that the local Windows/DirectX toolchain is working.

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

const char* Version = farcal::Version();
```

## Basic API shape

```cpp
farcal::CreateContext();

farcal::Window Window({
    .Title = L"Farcal",
    .Width = 1280,
    .Height = 720,
});

farcal::Dx11Renderer Renderer(Window);

while (Window.PollEvents()) {
    farcal::BeginFrame(Window.ConsumeInput());

    farcal::BackgroundRenderer().Commands.push_back({
        .Type = farcal::DrawCommandType::FilledRect,
        .Bounds = {{0.0F, 0.0F}, {1280.0F, 720.0F}},
        .Tint = farcal::Color::Rgb(14, 14, 16),
    });

    farcal::Frame([&] {
        farcal::WindowPanel("Farcal", [&] {
            farcal::TitleText("Viewport Tools");
            farcal::TextSecondary("Immediate-mode controls for engine tooling.");
            farcal::Separator();

            farcal::SectionText("Scene");
            static float Exposure = 1.0F;
            farcal::Slider<float>("Exposure", &Exposure, 0.0F, 5.0F);

            farcal::PushStyleColor(farcal::StyleColor::Text, farcal::Color::Rgb(82, 210, 115));
            farcal::Text("Ready");
            farcal::PopStyleColor();

            static int SelectedItem = 0;
            farcal::List("Entities", [&] {
                if (farcal::ListItem("Camera", SelectedItem == 0)) {
                    SelectedItem = 0;
                }
                if (farcal::ListItem("Light", SelectedItem == 1)) {
                    SelectedItem = 1;
                }
            });

            farcal::PrimaryButton("Select Entity", [&] {
                Window.CancelNextPoll();
            });
        });
    });

    farcal::EndFrame();
    Renderer.Render(farcal::Draw());
    Renderer.Present();
}

farcal::DestroyContext();
```

The Win32 wrapper also supports a message hook:

```cpp
Window.SetWndProcHook([](HWND handle, UINT message, WPARAM wparam, LPARAM lparam) -> std::optional<LRESULT> {
    return std::nullopt;
});
```

Calling `Window.CancelNextPoll()` makes the next `Window.PollEvents()` return `false`, which lets app code break out of a `while (Window.PollEvents())` loop from inside a widget callback.

The default text face is Inter through DirectWrite. If Inter is not installed on the system, DirectWrite will choose its normal fallback for that family.

## Development notes

- Keep public headers under `framework/include/framework/`.
- Keep platform backends under `framework/src/backends/`.
- Keep platform window code under `framework/src/win32/`.
- Add executable smoke tests under `tests/<test-name>/`.
- Prefer small, focused changes while the core API is still taking shape.
- Do not document planned APIs as if they already exist.

## Roadmap

- Add a framework-owned font atlas for non-DirectWrite backends.
- Add layout helpers.
- Add more core widgets.
- Add backend parity checks.
- Add focused tests or examples for each public feature as it lands.
