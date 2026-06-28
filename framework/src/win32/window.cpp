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

void register_Window_class()
{
    if (class_registered) {
        return;
    }

    WNDCLASSEXW Window_class {};
    Window_class.cbSize = sizeof(Window_class);
    Window_class.style = CS_HREDRAW | CS_VREDRAW;
    Window_class.lpfnWndProc = Window::WndProc;
    Window_class.hInstance = GetModuleHandleW(nullptr);
    Window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    Window_class.lpszClassName = class_name;

    if (RegisterClassExW(&Window_class) == 0) {
        throw std::runtime_error("RegisterClassExW failed");
    }

    class_registered = true;
}

}

Window::Window(const WindowDesc& desc)
    : Width_(desc.Width)
    , Height_(desc.Height)
{
    register_Window_class();

    RECT Window_rect {0, 0, Width_, Height_};
    AdjustWindowRect(&Window_rect, WS_OVERLAPPEDWINDOW, FALSE);

    Handle_ = CreateWindowExW(
        0,
        class_name,
        desc.Title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        Window_rect.right - Window_rect.left,
        Window_rect.bottom - Window_rect.top,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        this);

    if (Handle_ == nullptr) {
        throw std::runtime_error("CreateWindowExW failed");
    }

    ShowWindow(Handle_, SW_SHOWDEFAULT);
    UpdateWindow(Handle_);
}

Window::Window(Window&& other) noexcept
{
    *this = std::move(other);
}

Window& Window::operator=(Window&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    Release();

    Handle_ = other.Handle_;
    Width_ = other.Width_;
    Height_ = other.Height_;
    CloseRequested_ = other.CloseRequested_;
    CancelNextPoll_ = other.CancelNextPoll_;
    Input_ = other.Input_;
    Hook_ = std::move(other.Hook_);

    if (Handle_ != nullptr) {
        SetWindowLongPtrW(Handle_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }

    other.Handle_ = nullptr;
    return *this;
}

Window::~Window()
{
    Release();
}

bool Window::PollEvents()
{
    if (CancelNextPoll_) {
        CancelNextPoll_ = false;
        return false;
    }

    MSG message {};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            CloseRequested_ = true;
            break;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return !CloseRequested_;
}

void Window::CancelNextPoll()
{
    CancelNextPoll_ = true;
}

InputState Window::ConsumeInput()
{
    InputState result = Input_;
    Input_.MouseDelta = {};
    Input_.MouseWheel = 0.0F;
    Input_.MousePressed.fill(false);
    Input_.MouseReleased.fill(false);
    Input_.KeyPressed.fill(false);
    Input_.KeyRepeated.fill(false);
    Input_.KeyReleased.fill(false);
    Input_.TextInput.clear();
    return result;
}

void Window::SetWndProcHook(WndProcHook hook)
{
    Hook_ = std::move(hook);
}

HWND Window::NativeHandle() const
{
    return Handle_;
}

int Window::Width() const
{
    return Width_;
}

int Window::Height() const
{
    return Height_;
}

bool Window::CloseRequested() const
{
    return CloseRequested_;
}

LRESULT CALLBACK Window::WndProc(HWND handle, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        auto* self = static_cast<Window*>(create->lpCreateParams);
        SetWindowLongPtrW(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->Handle_ = handle;
    }

    auto* self = reinterpret_cast<Window*>(GetWindowLongPtrW(handle, GWLP_USERDATA));
    if (self != nullptr) {
        if (self->Hook_) {
            if (std::optional<LRESULT> result = self->Hook_(handle, message, wparam, lparam)) {
                return *result;
            }
        }

        return self->HandleMessage(message, wparam, lparam);
    }

    return DefWindowProcW(handle, message, wparam, lparam);
}

LRESULT Window::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CLOSE:
        CloseRequested_ = true;
        DestroyWindow(Handle_);
        return 0;

    case WM_DESTROY:
        CloseRequested_ = true;
        PostQuitMessage(0);
        return 0;

    case WM_NCDESTROY:
        SetWindowLongPtrW(Handle_, GWLP_USERDATA, 0);
        Handle_ = nullptr;
        return 0;

    case WM_SIZE:
        Width_ = LOWORD(lparam);
        Height_ = HIWORD(lparam);
        return 0;

    case WM_MOUSEMOVE:
    {
        const Vec2 previous = Input_.MousePosition;
        Input_.MousePosition = {static_cast<float>(GET_X_LPARAM(lparam)), static_cast<float>(GET_Y_LPARAM(lparam))};
        Input_.MouseDelta = {Input_.MouseDelta.X + Input_.MousePosition.X - previous.X, Input_.MouseDelta.Y + Input_.MousePosition.Y - previous.Y};
        return 0;
    }

    case WM_MOUSEWHEEL:
        Input_.MouseWheel += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / static_cast<float>(WHEEL_DELTA);
        return 0;

    case WM_LBUTTONDOWN:
        Input_.MouseDown[0] = true;
        Input_.MousePressed[0] = true;
        SetCapture(Handle_);
        return 0;

    case WM_LBUTTONUP:
        Input_.MouseDown[0] = false;
        Input_.MouseReleased[0] = true;
        ReleaseCapture();
        return 0;

    case WM_RBUTTONDOWN:
        Input_.MouseDown[1] = true;
        Input_.MousePressed[1] = true;
        SetCapture(Handle_);
        return 0;

    case WM_RBUTTONUP:
        Input_.MouseDown[1] = false;
        Input_.MouseReleased[1] = true;
        ReleaseCapture();
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wparam < Input_.KeyDown.size()) {
            const std::size_t key = static_cast<std::size_t>(wparam);
            const bool was_down = Input_.KeyDown[key];
            Input_.KeyPressed[key] = !was_down;
            Input_.KeyRepeated[key] = was_down;
            Input_.KeyDown[key] = true;
        }
        Input_.Shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        Input_.Control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        Input_.Alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        return 0;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (wparam < Input_.KeyDown.size()) {
            const std::size_t key = static_cast<std::size_t>(wparam);
            Input_.KeyDown[key] = false;
            Input_.KeyReleased[key] = true;
        }
        Input_.Shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        Input_.Control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        Input_.Alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        return 0;

    case WM_CHAR:
        if (wparam >= 32 && wparam != 127) {
            Input_.TextInput.push_back(static_cast<char>(wparam));
        }
        return 0;

    default:
        return DefWindowProcW(Handle_, message, wparam, lparam);
    }
}

void Window::Release()
{
    if (Handle_ != nullptr) {
        DestroyWindow(Handle_);
        Handle_ = nullptr;
    }
}

}
