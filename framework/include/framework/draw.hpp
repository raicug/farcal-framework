#pragma once

// clang-format off
#include <string>
#include <string_view>
#include <vector>
// clang-format on

namespace farcal {

struct vec2 {
    float x {};
    float y {};
};

struct color {
    float r {};
    float g {};
    float b {};
    float a {1.0F};
};

struct rect {
    vec2 min {};
    vec2 max {};
};

enum class draw_command_type {
    filled_rect,
    rect,
    line,
    text,
};

struct draw_command {
    draw_command_type type {};
    rect bounds {};
    rect clip {};
    vec2 start {};
    vec2 end {};
    color tint {};
    float thickness {1.0F};
    float font_size {16.0F};
    std::string text {};
    std::wstring font_family {};
};

struct draw_data {
    std::vector<draw_command> commands {};
};

}
