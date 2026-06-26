#pragma once

// clang-format off
#include <framework/draw.hpp>

#include <string>
// clang-format on

namespace farcal {

enum class StyleColor {
    Text,
    TextSecondary,
    TextMuted,
    Button,
    ButtonHovered,
    ButtonActive,
    ButtonPrimary,
    ButtonPrimaryHovered,
    ButtonPrimaryActive,
    ButtonBorder,
    WindowBackground,
    WindowPanel,
    WindowBorder,
    WindowTitle,
    WindowTitleActive,
    Accent,
    Selection,
};

enum class StyleVar {
    FontSize,
    FrameScale,
    FramePaddingX,
    FramePaddingY,
    ItemSpacingY,
    ItemWidth,
    WindowWidth,
    WindowHeight,
    WindowTitleHeight,
    SectionSpacingY,
    AntiAliasing,
};

struct Style {
    Color Text {0.925F, 0.937F, 0.957F, 1.0F};
    Color TextSecondary {0.700F, 0.710F, 0.745F, 1.0F};
    Color TextMuted {0.430F, 0.440F, 0.480F, 1.0F};
    Color Button {0.090F, 0.092F, 0.105F, 1.0F};
    Color ButtonHovered {0.125F, 0.128F, 0.150F, 1.0F};
    Color ButtonActive {0.160F, 0.165F, 0.200F, 1.0F};
    Color ButtonPrimary {0.205F, 0.325F, 0.720F, 1.0F};
    Color ButtonPrimaryHovered {0.250F, 0.380F, 0.820F, 1.0F};
    Color ButtonPrimaryActive {0.160F, 0.255F, 0.600F, 1.0F};
    Color ButtonBorder {0.180F, 0.185F, 0.210F, 1.0F};
    Color WindowBackground {0.067F, 0.067F, 0.075F, 1.0F};
    Color WindowPanel {0.067F, 0.067F, 0.075F, 1.0F};
    Color WindowBorder {0.185F, 0.190F, 0.225F, 1.0F};
    Color WindowTitle {0.078F, 0.075F, 0.094F, 1.0F};
    Color WindowTitleActive {0.095F, 0.092F, 0.115F, 1.0F};
    Color Accent {0.360F, 0.545F, 1.0F, 1.0F};
    Color Selection {0.250F, 0.360F, 0.760F, 1.0F};
    float FontSize {14.0F};
    float FrameScale {1.0F};
    float FramePaddingX {10.0F};
    float FramePaddingY {6.0F};
    float ItemSpacingY {8.0F};
    float ItemWidth {220.0F};
    float WindowWidth {571.0F};
    float WindowHeight {412.0F};
    float WindowTitleHeight {40.0F};
    float SectionSpacingY {20.0F};
    float AntiAliasing {1.0F};
    std::wstring FontFamily {L"Inter"};
};

const Style& CurrentStyle();
void SetStyle(const Style& value);
void PushStyleColor(StyleColor variable, Color value);
void PopStyleColor();
void PushStyleVar(StyleVar variable, float value);
void PopStyleVar();

}
