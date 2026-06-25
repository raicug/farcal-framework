#pragma once

// clang-format off
#include <concepts>
#include <string_view>
// clang-format on

namespace farcal {

void text(std::string_view value);
void text_secondary(std::string_view value);
void title_text(std::string_view value);
void section_text(std::string_view value);
void separator();
void spacing();
bool button(std::string_view label);
bool primary_button(std::string_view label);
bool begin_window(std::string_view title);
void end_window();

template <std::invocable Callback>
bool button(std::string_view label, Callback&& callback)
{
    const bool clicked = button(label);
    if (clicked) {
        callback();
    }
    return clicked;
}

template <std::invocable Callback>
bool primary_button(std::string_view label, Callback&& callback)
{
    const bool clicked = primary_button(label);
    if (clicked) {
        callback();
    }
    return clicked;
}

template <std::invocable Callback>
void window_panel(std::string_view title, Callback&& callback)
{
    if (begin_window(title)) {
        callback();
    }

    end_window();
}

}
