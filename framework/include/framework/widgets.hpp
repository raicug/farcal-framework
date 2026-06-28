#pragma once

// clang-format off
#include <framework/draw.hpp>

#include <cmath>
#include <concepts>
#include <cstddef>
#include <string_view>
#include <type_traits>
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
bool SliderFloat(std::string_view label, float* value, float minimum, float maximum, std::string_view suffix = {});
bool ColorEdit(std::string_view label, Color* value);
bool Dropdown(std::string_view label, int* selected, const char* const* items, int item_count);
bool InputText(std::string_view label, char* buffer, std::size_t buffer_size);
bool IsTextInputFocused();
enum class KeybindMode {
    Toggle,
    Hold,
};
bool Keybind(std::string_view label, int* value);
bool Keybind(std::string_view label, int* value, KeybindMode* mode);
bool BeginTabs(std::string_view label, int* selected, const char* const* tabs, int tab_count);
void EndTabs();
bool BeginList(std::string_view label, Vec2 size = {});
void EndList();
bool ListItem(std::string_view label, bool selected = false);
bool BeginWindow(std::string_view Title);
void EndWindow();

namespace widget_internal {

bool SliderScalar(std::string_view label, double* value, double minimum, double maximum, bool integral, std::string_view suffix);

}

template <typename Value>
concept SliderValue = (std::integral<Value> || std::floating_point<Value>) && !std::same_as<std::remove_cv_t<Value>, bool>;

template <SliderValue Value>
bool Slider(std::string_view label, Value* value, Value minimum, Value maximum, std::string_view suffix = {})
{
    if (value == nullptr) {
        return false;
    }

    double scalar = static_cast<double>(*value);
    const bool changed = widget_internal::SliderScalar(
        label,
        &scalar,
        static_cast<double>(minimum),
        static_cast<double>(maximum),
        std::integral<Value>,
        suffix);

    if (!changed) {
        return false;
    }

    if constexpr (std::integral<Value>) {
        *value = static_cast<Value>(std::llround(scalar));
    } else {
        *value = static_cast<Value>(scalar);
    }

    return true;
}

template <std::size_t Count>
bool Dropdown(std::string_view label, int* selected, const char* const (&items)[Count])
{
    return Dropdown(label, selected, items, static_cast<int>(Count));
}

template <std::size_t Count>
bool BeginTabs(std::string_view label, int* selected, const char* const (&tabs)[Count])
{
    return BeginTabs(label, selected, tabs, static_cast<int>(Count));
}

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

template <std::invocable Callback>
void List(std::string_view label, Callback&& callback, Vec2 size = {})
{
    if (BeginList(label, size)) {
        callback();
    }

    EndList();
}

}
