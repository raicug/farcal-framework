#include <framework/framework.hpp>

#include <d3d11.h>
#include <windows.h>

#include <cstdio>
#include <iterator>

namespace {

constexpr wchar_t kWindowClassName[] = L"UiFrameworkDx11WindowTest";

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

template <typename T>
void release_if_set(T*& object)
{
    if (object != nullptr) {
        object->Release();
        object = nullptr;
    }
}

bool failed(HRESULT result, const char* operation)
{
    if (SUCCEEDED(result)) {
        return false;
    }

    std::fprintf(stderr, "%s failed: 0x%08lX\n", operation, static_cast<unsigned long>(result));
    return true;
}

}

int main()
{
    const HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSEXW window_class {};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    window_class.lpszClassName = kWindowClassName;

    if (RegisterClassExW(&window_class) == 0) {
        std::fprintf(stderr, "RegisterClassExW failed: %lu\n", GetLastError());
        return 1;
    }

    RECT window_rect {0, 0, 1280, 720};
    AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);

    char title[128] {};
    std::snprintf(title, sizeof(title), "DX11 Window Test - Farcal %s", farcal::version());

    HWND window = CreateWindowExW(
        0,
        kWindowClassName,
        L"DX11 Window Test",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        window_rect.right - window_rect.left,
        window_rect.bottom - window_rect.top,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (window == nullptr) {
        std::fprintf(stderr, "CreateWindowExW failed: %lu\n", GetLastError());
        return 1;
    }

    SetWindowTextA(window, title);

    DXGI_SWAP_CHAIN_DESC swap_chain_desc {};
    swap_chain_desc.BufferCount = 2;
    swap_chain_desc.BufferDesc.Width = 1280;
    swap_chain_desc.BufferDesc.Height = 720;
    swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.OutputWindow = window;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.Windowed = TRUE;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL feature_level {};
    constexpr D3D_FEATURE_LEVEL feature_levels[] {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* device_context = nullptr;
    IDXGISwapChain* swap_chain = nullptr;

    HRESULT result = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        feature_levels,
        static_cast<UINT>(std::size(feature_levels)),
        D3D11_SDK_VERSION,
        &swap_chain_desc,
        &swap_chain,
        &device,
        &feature_level,
        &device_context);

    if (failed(result, "D3D11CreateDeviceAndSwapChain")) {
        return 1;
    }

    ID3D11Texture2D* back_buffer = nullptr;
    result = swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back_buffer));
    if (failed(result, "IDXGISwapChain::GetBuffer")) {
        release_if_set(swap_chain);
        release_if_set(device_context);
        release_if_set(device);
        return 1;
    }

    ID3D11RenderTargetView* render_target = nullptr;
    result = device->CreateRenderTargetView(back_buffer, nullptr, &render_target);
    release_if_set(back_buffer);

    if (failed(result, "ID3D11Device::CreateRenderTargetView")) {
        release_if_set(swap_chain);
        release_if_set(device_context);
        release_if_set(device);
        return 1;
    }

    ShowWindow(window, SW_SHOWDEFAULT);
    UpdateWindow(window);

    bool running = true;
    while (running) {
        MSG message {};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                running = false;
                break;
            }

            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        if (!running) {
            break;
        }

        constexpr float clear_color[] {0.09F, 0.12F, 0.16F, 1.0F};
        device_context->OMSetRenderTargets(1, &render_target, nullptr);
        device_context->ClearRenderTargetView(render_target, clear_color);
        swap_chain->Present(1, 0);
    }

    release_if_set(render_target);
    release_if_set(swap_chain);
    release_if_set(device_context);
    release_if_set(device);

    return 0;
}
