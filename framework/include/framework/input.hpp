#pragma once

// clang-format off
#include <framework/draw.hpp>

#include <array>
// clang-format on

namespace farcal {

struct input_state {
    vec2 mouse_position {};
    vec2 mouse_delta {};
    float mouse_wheel {};
    std::array<bool, 5> mouse_down {};
    std::array<bool, 5> mouse_pressed {};
    std::array<bool, 5> mouse_released {};
    std::array<bool, 256> key_down {};
    std::array<bool, 256> key_pressed {};
    std::array<bool, 256> key_released {};
    bool shift {};
    bool control {};
    bool alt {};
};

}
