#pragma once

// clang-format off
#include <framework/input.hpp>
// clang-format on

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// clang-format off
#include <windows.h>

#include <functional>
#include <optional>
#include <string>
// clang-format on

namespace farcal {

struct window_desc {
    std::wstring title {L"Farcal"};
    int width {1280};
    int height {720};
};

using wnd_proc_hook = std::function<std::optional<LRESULT>(HWND, UINT, WPARAM, LPARAM)>;

class window {
public:
    explicit window(const window_desc& desc = {});
    window(const window&) = delete;
    window& operator=(const window&) = delete;
    window(window&& other) noexcept;
    window& operator=(window&& other) noexcept;
    ~window();

    bool poll_events();
    void cancel_next_poll();
    input_state consume_input();
    void set_wnd_proc_hook(wnd_proc_hook hook);
    HWND native_handle() const;
    int width() const;
    int height() const;
    bool close_requested() const;

    static LRESULT CALLBACK wnd_proc(HWND handle, UINT message, WPARAM wparam, LPARAM lparam);

private:
    HWND handle_ {};
    int width_ {};
    int height_ {};
    bool close_requested_ {};
    bool cancel_next_poll_ {};
    input_state input_ {};
    wnd_proc_hook hook_ {};

    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam);
    void release();
};

}
