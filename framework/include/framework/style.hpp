#pragma once

// clang-format off
#include <framework/draw.hpp>

#include <string>
// clang-format on

namespace farcal {

enum class style_color {
    text,
    text_secondary,
    text_muted,
    button,
    button_hovered,
    button_active,
    button_primary,
    button_primary_hovered,
    button_primary_active,
    button_border,
    window_background,
    window_panel,
    window_border,
    window_title,
    window_title_active,
    accent,
    selection,
};

enum class style_var {
    font_size,
    frame_scale,
    frame_padding_x,
    frame_padding_y,
    item_spacing_y,
    item_width,
    window_width,
    window_height,
    window_title_height,
    section_spacing_y,
};

struct style {
    color text {0.925F, 0.937F, 0.957F, 1.0F};
    color text_secondary {0.700F, 0.710F, 0.745F, 1.0F};
    color text_muted {0.430F, 0.440F, 0.480F, 1.0F};
    color button {0.090F, 0.092F, 0.105F, 1.0F};
    color button_hovered {0.125F, 0.128F, 0.150F, 1.0F};
    color button_active {0.160F, 0.165F, 0.200F, 1.0F};
    color button_primary {0.205F, 0.325F, 0.720F, 1.0F};
    color button_primary_hovered {0.250F, 0.380F, 0.820F, 1.0F};
    color button_primary_active {0.160F, 0.255F, 0.600F, 1.0F};
    color button_border {0.180F, 0.185F, 0.210F, 1.0F};
    color window_background {0.067F, 0.067F, 0.075F, 1.0F};
    color window_panel {0.067F, 0.067F, 0.075F, 1.0F};
    color window_border {0.185F, 0.190F, 0.225F, 1.0F};
    color window_title {0.078F, 0.075F, 0.094F, 1.0F};
    color window_title_active {0.095F, 0.092F, 0.115F, 1.0F};
    color accent {0.360F, 0.545F, 1.0F, 1.0F};
    color selection {0.250F, 0.360F, 0.760F, 1.0F};
    float font_size {14.0F};
    float frame_scale {1.0F};
    float frame_padding_x {10.0F};
    float frame_padding_y {6.0F};
    float item_spacing_y {8.0F};
    float item_width {220.0F};
    float window_width {571.0F};
    float window_height {412.0F};
    float window_title_height {40.0F};
    float section_spacing_y {20.0F};
    std::wstring font_family {L"Inter"};
};

const style& current_style();
void set_style(const style& value);
void push_style_color(style_color variable, color value);
void pop_style_color();
void push_style_var(style_var variable, float value);
void pop_style_var();

}
