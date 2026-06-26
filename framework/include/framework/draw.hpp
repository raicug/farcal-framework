#pragma once

// clang-format off
#include <string>
#include <string_view>
#include <vector>
// clang-format on

namespace farcal {

struct Vec2 {
    float X {};
    float Y {};
};

struct Color {
    float R {};
    float G {};
    float B {};
    float A {1.0F};
};

struct Rect {
    Vec2 Min {};
    Vec2 Max {};
};

enum class DrawCommandType {
    FilledRect,
    Rect,
    Line,
    Text,
};

struct DrawCommand {
    DrawCommandType Type {};
    Rect Bounds {};
    Rect Clip {};
    Vec2 Start {};
    Vec2 End {};
    Color Tint {};
    float Thickness {1.0F};
    float AntiAliasing {1.0F};
    float FontSize {16.0F};
    std::string Text {};
    std::wstring FontFamily {};
};

struct DrawData {
    std::vector<DrawCommand> Commands {};
};

}
