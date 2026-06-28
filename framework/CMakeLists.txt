add_library(framework STATIC
    src/backends/dx9.cpp
    src/backends/dx10.cpp
    src/backends/dx11.cpp
    src/context.cpp
    src/draw.cpp
    src/input.cpp
    src/main.cpp
    src/widgets.cpp
    src/widgets/children.cpp
    src/widgets/colors.cpp
    src/widgets/controls.cpp
    src/widgets/input.cpp
    src/widgets/keybinds.cpp
    src/widgets/layout.cpp
    src/widgets/lists.cpp
    src/widgets/tabs.cpp
    src/widgets/text.cpp
    src/widgets/tooltips.cpp
    src/widgets/windows.cpp
    src/win32/window.cpp
)

add_library(farcal::framework ALIAS framework)

target_compile_features(framework PUBLIC cxx_std_20)

target_include_directories(framework
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(framework
    PUBLIC
        user32
        gdi32
        psapi
        winmm
        d3d9
        d3d10
        d3d10_1
        d3d11
        dxgi
        dxguid
        d3dcompiler
        d2d1
        dwrite
)
