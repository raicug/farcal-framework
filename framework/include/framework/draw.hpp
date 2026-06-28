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

    constexpr Color() = default;

    constexpr Color(float red, float green, float blue, float alpha = 1.0F)
        : R(red)
        , G(green)
        , B(blue)
        , A(alpha)
    {
    }

    static constexpr Color Rgb(float red, float green, float blue)
    {
        return {red, green, blue};
    }

    static constexpr Color Rgba(float red, float green, float blue, float alpha)
    {
        return {red, green, blue, alpha};
    }

    static constexpr Color FromBytes(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha = 255)
    {
        constexpr float scale = 1.0F / 255.0F;
        return {
            static_cast<float>(red) * scale,
            static_cast<float>(green) * scale,
            static_cast<float>(blue) * scale,
            static_cast<float>(alpha) * scale,
        };
    }
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
