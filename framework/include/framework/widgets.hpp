#pragma once

// clang-format off
#include <framework/draw.hpp>

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
void same_line(float spacing = -1.0F);
void begin_group();
void end_group();
void indent(float width = 0.0F);
void unindent(float width = 0.0F);
void dummy(vec2 size);
void set_next_item_width(float width);
bool is_item_hovered();
bool is_item_active();
bool is_item_focused();
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
