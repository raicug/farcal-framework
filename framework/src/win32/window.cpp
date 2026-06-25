// clang-format off
#include <framework/window.hpp>

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <windowsx.h>
// clang-format on

namespace farcal {
namespace {

constexpr wchar_t class_name[] = L"FarcalWindow";
bool class_registered = false;

void register_window_class()
{
    if (class_registered) {
        return;
    }

    WNDCLASSEXW window_class {};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = window::wnd_proc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    window_class.lpszClassName = class_name;

    if (RegisterClassExW(&window_class) == 0) {
        throw std::runtime_error("RegisterClassExW failed");
    }

    class_registered = true;
}

}

window::window(const window_desc& desc)
    : width_(desc.width)
    , height_(desc.height)
{
    register_window_class();

    RECT window_rect {0, 0, width_, height_};
    AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);

    handle_ = CreateWindowExW(
        0,
        class_name,
        desc.title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        window_rect.right - window_rect.left,
        window_rect.bottom - window_rect.top,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        this);

    if (handle_ == nullptr) {
        throw std::runtime_error("CreateWindowExW failed");
    }

    ShowWindow(handle_, SW_SHOWDEFAULT);
    UpdateWindow(handle_);
}

window::window(window&& other) noexcept
{
    *this = std::move(other);
}

window& window::operator=(window&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    release();

    handle_ = other.handle_;
    width_ = other.width_;
    height_ = other.height_;
    close_requested_ = other.close_requested_;
    cancel_next_poll_ = other.cancel_next_poll_;
    input_ = other.input_;
    hook_ = std::move(other.hook_);

    if (handle_ != nullptr) {
        SetWindowLongPtrW(handle_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }

    other.handle_ = nullptr;
    return *this;
}

window::~window()
{
    release();
}

bool window::poll_events()
{
    if (cancel_next_poll_) {
        cancel_next_poll_ = false;
        return false;
    }

    MSG message {};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            close_requested_ = true;
            break;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return !close_requested_;
}

void window::cancel_next_poll()
{
    cancel_next_poll_ = true;
}

input_state window::consume_input()
{
    input_state result = input_;
    input_.mouse_delta = {};
    input_.mouse_wheel = 0.0F;
    input_.mouse_pressed.fill(false);
    input_.mouse_released.fill(false);
    input_.key_pressed.fill(false);
    input_.key_released.fill(false);
    return result;
}

void window::set_wnd_proc_hook(wnd_proc_hook hook)
{
    hook_ = std::move(hook);
}

HWND window::native_handle() const
{
    return handle_;
}

int window::width() const
{
    return width_;
}

int window::height() const
{
    return height_;
}

bool window::close_requested() const
{
    return close_requested_;
}

LRESULT CALLBACK window::wnd_proc(HWND handle, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        auto* self = static_cast<window*>(create->lpCreateParams);
        SetWindowLongPtrW(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->handle_ = handle;
    }

    auto* self = reinterpret_cast<window*>(GetWindowLongPtrW(handle, GWLP_USERDATA));
    if (self != nullptr) {
        if (self->hook_) {
            if (std::optional<LRESULT> result = self->hook_(handle, message, wparam, lparam)) {
                return *result;
            }
        }

        return self->handle_message(message, wparam, lparam);
    }

    return DefWindowProcW(handle, message, wparam, lparam);
}

LRESULT window::handle_message(UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CLOSE:
        close_requested_ = true;
        DestroyWindow(handle_);
        return 0;

    case WM_DESTROY:
        close_requested_ = true;
        PostQuitMessage(0);
        return 0;

    case WM_NCDESTROY:
        SetWindowLongPtrW(handle_, GWLP_USERDATA, 0);
        handle_ = nullptr;
        return 0;

    case WM_SIZE:
        width_ = LOWORD(lparam);
        height_ = HIWORD(lparam);
        return 0;

    case WM_MOUSEMOVE:
    {
        const vec2 previous = input_.mouse_position;
        input_.mouse_position = {static_cast<float>(GET_X_LPARAM(lparam)), static_cast<float>(GET_Y_LPARAM(lparam))};
        input_.mouse_delta = {input_.mouse_delta.x + input_.mouse_position.x - previous.x, input_.mouse_delta.y + input_.mouse_position.y - previous.y};
        return 0;
    }

    case WM_MOUSEWHEEL:
        input_.mouse_wheel += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / static_cast<float>(WHEEL_DELTA);
        return 0;

    case WM_LBUTTONDOWN:
        input_.mouse_down[0] = true;
        input_.mouse_pressed[0] = true;
        SetCapture(handle_);
        return 0;

    case WM_LBUTTONUP:
        input_.mouse_down[0] = false;
        input_.mouse_released[0] = true;
        ReleaseCapture();
        return 0;

    case WM_RBUTTONDOWN:
        input_.mouse_down[1] = true;
        input_.mouse_pressed[1] = true;
        SetCapture(handle_);
        return 0;

    case WM_RBUTTONUP:
        input_.mouse_down[1] = false;
        input_.mouse_released[1] = true;
        ReleaseCapture();
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wparam < input_.key_down.size()) {
            const std::size_t key = static_cast<std::size_t>(wparam);
            input_.key_pressed[key] = !input_.key_down[key];
            input_.key_down[key] = true;
        }
        input_.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        input_.control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        input_.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        return 0;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (wparam < input_.key_down.size()) {
            const std::size_t key = static_cast<std::size_t>(wparam);
            input_.key_down[key] = false;
            input_.key_released[key] = true;
        }
        input_.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        input_.control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        input_.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        return 0;

    default:
        return DefWindowProcW(handle_, message, wparam, lparam);
    }
}

void window::release()
{
    if (handle_ != nullptr) {
        DestroyWindow(handle_);
        handle_ = nullptr;
    }
}

}
