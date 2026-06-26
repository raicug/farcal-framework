#pragma once

// clang-format off
#include <framework/draw.hpp>

#include <concepts>
#include <string_view>
// clang-format on

namespace farcal {

void Text(std::string_view value);
void TextSecondary(std::string_view value);
void TitleText(std::string_view value);
void SectionText(std::string_view value);
void Separator();
void Spacing();
void SameLine(float Spacing = -1.0F);
void BeginGroup();
void EndGroup();
void Indent(float Width = 0.0F);
void Unindent(float Width = 0.0F);
void Dummy(Vec2 size);
void SetNextItemWidth(float Width);
bool IsItemHovered();
bool IsItemActive();
bool IsItemFocused();
bool Button(std::string_view label);
bool PrimaryButton(std::string_view label);
bool Checkbox(std::string_view label, bool* value);
bool SliderFloat(std::string_view label, float* value, float minimum, float maximum);
bool BeginWindow(std::string_view Title);
void EndWindow();

template <std::invocable Callback>
bool Button(std::string_view label, Callback&& callback)
{
    const bool clicked = Button(label);
    if (clicked) {
        callback();
    }
    return clicked;
}

template <std::invocable Callback>
bool PrimaryButton(std::string_view label, Callback&& callback)
{
    const bool clicked = PrimaryButton(label);
    if (clicked) {
        callback();
    }
    return clicked;
}

template <std::invocable Callback>
void WindowPanel(std::string_view Title, Callback&& callback)
{
    if (BeginWindow(Title)) {
        callback();
    }

    EndWindow();
}

template <std::invocable Callback>
void Group(Callback&& callback)
{
    BeginGroup();
    callback();
    EndGroup();
}

}
