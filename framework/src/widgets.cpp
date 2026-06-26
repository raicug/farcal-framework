// clang-format off
#include <framework/widgets.hpp>

#include <framework/context.hpp>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
// clang-format on

namespace farcal {
namespace {

struct layout_state {
    Vec2 Cursor {24.0F, 24.0F};
    float StartX {24.0F};
    float ContentWidth {};
    Rect Clip {};
    bool HasClip {};
    Rect LastItem {};
    bool HasLastItem {};
    float LineBottom {};
    float NextItemWidth {};
    float IndentWidth {};
};

struct window_state {
    Vec2 Position {};
    Vec2 DragOffset {};
    float ContentHeight {};
    float ScrollY {};
    bool Initialized {};
    bool Dragging {};
};

struct window_layout_state {
    std::string Title {};
    float ContentStartY {};
    float ContentPaddingY {};
    float VisibleHeight {};
    float MaxScroll {};
    bool Hovered {};
    Rect ScrollTrack {};
};

struct group_state {
    layout_state Parent {};
};

struct item_result {
    bool Hovered {};
    bool Active {};
    bool Focused {};
    bool Clicked {};
};

layout_state layout;
std::vector<layout_state> layout_stack;
std::vector<window_layout_state> window_stack;
std::vector<group_state> group_stack;
std::unordered_map<std::string, window_state> windows;
std::uint64_t layout_frame = static_cast<std::uint64_t>(-1);
std::uint64_t active_item {};
std::uint64_t focused_item {};
std::uint64_t LastItem {};
bool last_hovered {};
bool last_active {};
bool last_focused {};

void ensure_layout()
{
    if (layout_frame == FrameIndex()) {
        return;
    }

    layout = {};
    layout_stack.clear();
    window_stack.clear();
    group_stack.clear();
    LastItem = 0;
    last_hovered = false;
    last_active = false;
    last_focused = false;
    layout_frame = FrameIndex();
}

DrawData& mutable_draw()
{
    return MainRenderer();
}

bool contains(Rect bounds, Vec2 point)
{
    return point.X >= bounds.Min.X && point.Y >= bounds.Min.Y && point.X <= bounds.Max.X && point.Y <= bounds.Max.Y;
}

Rect intersect(Rect a, Rect b)
{
    return {
        {std::max(a.Min.X, b.Min.X), std::max(a.Min.Y, b.Min.Y)},
        {std::min(a.Max.X, b.Max.X), std::min(a.Max.Y, b.Max.Y)},
    };
}

Rect active_clip(Rect bounds)
{
    if (!layout.HasClip) {
        return bounds;
    }

    return intersect(bounds, layout.Clip);
}

Rect command_clip(Rect bounds)
{
    if (!layout.HasClip) {
        return bounds;
    }

    return layout.Clip;
}

float text_width(std::string_view value)
{
    const Style& theme = CurrentStyle();
    return std::max(24.0F, static_cast<float>(value.size()) * theme.FontSize * theme.FrameScale * 0.54F);
}

float text_bounds_width(std::string_view value)
{
    if (layout.ContentWidth > 0.0F) {
        return layout.ContentWidth;
    }

    return text_width(value);
}

Rect next_rect(float Width, float Height)
{
    if (layout.NextItemWidth > 0.0F) {
        Width = layout.NextItemWidth;
        layout.NextItemWidth = 0.0F;
    }

    const float Spacing = CurrentStyle().ItemSpacingY * CurrentStyle().FrameScale;
    Rect bounds {
        layout.Cursor,
        {layout.Cursor.X + Width, layout.Cursor.Y + Height},
    };
    const float bottom = std::max(layout.LineBottom, bounds.Max.Y);
    layout.Cursor = {layout.StartX + layout.IndentWidth, bottom + Spacing};
    layout.LineBottom = bottom;
    layout.LastItem = bounds;
    layout.HasLastItem = true;
    return bounds;
}

void advance_layout(float Height)
{
    const float Spacing = CurrentStyle().ItemSpacingY * CurrentStyle().FrameScale;
    layout.Cursor.Y += Height + Spacing;
    layout.LineBottom = layout.Cursor.Y - Spacing;
}

void set_last_item(std::uint64_t id, bool Hovered, bool Active, bool Focused)
{
    LastItem = id;
    last_hovered = Hovered;
    last_active = Active;
    last_focused = Focused;
}

item_result item_behavior(std::uint64_t id, Rect bounds)
{
    const InputState& io = Input();
    if (!io.MouseDown[0] && !io.MousePressed[0]) {
        active_item = 0;
    }

    const bool Hovered = contains(active_clip(bounds), io.MousePosition);
    if (Hovered && io.MousePressed[0]) {
        active_item = id;
        focused_item = id;
    }

    const bool Active = active_item == id;
    const bool Focused = focused_item == id;
    const bool Clicked = Hovered && io.MousePressed[0];
    set_last_item(id, Hovered, Active, Focused);

    return {
        .Hovered = Hovered,
        .Active = Active,
        .Focused = Focused,
        .Clicked = Clicked,
    };
}

bool keyboard_toggle(std::uint64_t id)
{
    const InputState& io = Input();
    return focused_item == id && (io.KeyPressed[13] || io.KeyPressed[32]);
}

float normalized_value(float value, float minimum, float maximum)
{
    if (maximum <= minimum) {
        return 0.0F;
    }

    return std::clamp((value - minimum) / (maximum - minimum), 0.0F, 1.0F);
}

float value_from_normalized(float value, float minimum, float maximum)
{
    return minimum + (maximum - minimum) * std::clamp(value, 0.0F, 1.0F);
}

bool set_float_value(float* target, float value, float minimum, float maximum)
{
    const float clamped = std::clamp(value, minimum, maximum);
    if (*target == clamped) {
        return false;
    }

    *target = clamped;
    return true;
}

void add_text(Rect bounds, std::string_view value, Color tint)
{
    const Style& theme = CurrentStyle();
    mutable_draw().Commands.push_back({
        .Type = DrawCommandType::Text,
        .Bounds = bounds,
        .Clip = command_clip(bounds),
        .Tint = tint,
        .FontSize = theme.FontSize * theme.FrameScale,
        .Text = std::string(value),
        .FontFamily = theme.FontFamily,
    });
}

void add_filled_rect(Rect bounds, Color tint)
{
    mutable_draw().Commands.push_back({
        .Type = DrawCommandType::FilledRect,
        .Bounds = bounds,
        .Clip = command_clip(bounds),
        .Tint = tint,
        .AntiAliasing = CurrentStyle().AntiAliasing,
    });
}

void add_rect(Rect bounds, Color tint, float thickness = 1.0F)
{
    mutable_draw().Commands.push_back({
        .Type = DrawCommandType::Rect,
        .Bounds = bounds,
        .Clip = command_clip(bounds),
        .Tint = tint,
        .Thickness = thickness,
        .AntiAliasing = CurrentStyle().AntiAliasing,
    });
}

void add_line(Vec2 start, Vec2 end, Color tint, float thickness)
{
    const Rect bounds {
        {std::min(start.X, end.X), std::min(start.Y, end.Y)},
        {std::max(start.X, end.X), std::max(start.Y, end.Y)},
    };

    mutable_draw().Commands.push_back({
        .Type = DrawCommandType::Line,
        .Clip = command_clip(bounds),
        .Start = start,
        .End = end,
        .Tint = tint,
        .Thickness = thickness,
        .AntiAliasing = CurrentStyle().AntiAliasing,
    });
}

void add_hline(float x0, float x1, float y, Color tint)
{
    add_line({x0, y + 0.5F}, {x1, y + 0.5F}, tint, 1.0F);
}

void add_vline(float x, float y0, float y1, Color tint)
{
    add_line({x + 0.5F, y0}, {x + 0.5F, y1}, tint, 1.0F);
}

void add_soft_rect(Rect bounds, float radius, Color tint)
{
    add_filled_rect({{bounds.Min.X + radius, bounds.Min.Y}, {bounds.Max.X - radius, bounds.Max.Y}}, tint);
    add_filled_rect({{bounds.Min.X, bounds.Min.Y + radius}, {bounds.Max.X, bounds.Max.Y - radius}}, tint);
    add_filled_rect({{bounds.Min.X + 1.0F, bounds.Min.Y + 1.0F}, {bounds.Min.X + radius, bounds.Min.Y + radius}}, tint);
    add_filled_rect({{bounds.Max.X - radius, bounds.Min.Y + 1.0F}, {bounds.Max.X - 1.0F, bounds.Min.Y + radius}}, tint);
    add_filled_rect({{bounds.Min.X + 1.0F, bounds.Max.Y - radius}, {bounds.Min.X + radius, bounds.Max.Y - 1.0F}}, tint);
    add_filled_rect({{bounds.Max.X - radius, bounds.Max.Y - radius}, {bounds.Max.X - 1.0F, bounds.Max.Y - 1.0F}}, tint);
}

void add_soft_outline(Rect bounds, float radius, Color tint)
{
    (void)radius;

    const float left = bounds.Min.X + 0.5F;
    const float top = bounds.Min.Y + 0.5F;
    const float right = bounds.Max.X - 0.5F;
    const float bottom = bounds.Max.Y - 0.5F;

    add_line({left, top}, {right, top}, tint, 1.0F);
    add_line({right, top}, {right, bottom}, tint, 1.0F);
    add_line({right, bottom}, {left, bottom}, tint, 1.0F);
    add_line({left, bottom}, {left, top}, tint, 1.0F);
}

bool button_impl(std::string_view label, Color normal, Color hovered_color, Color active_color, Color text_color)
{
    ensure_layout();
    const Style& theme = CurrentStyle();
    const float scale = theme.FrameScale;
    const float Height = 30.0F * scale;
    const float Width = layout.ContentWidth > 0.0F ? std::min(theme.ItemWidth * scale, layout.ContentWidth) : theme.ItemWidth * scale;
    const Rect bounds = next_rect(Width, Height);
    const item_result item = item_behavior(CurrentId(label), bounds);
    const Color fill = item.Active ? active_color : item.Hovered ? hovered_color : normal;
    const Color top_edge = item.Hovered ? Color {theme.Accent.R, theme.Accent.G, theme.Accent.B, 0.28F} : Color {1.0F, 1.0F, 1.0F, 0.025F};

    add_soft_rect(bounds, 4.0F * scale, fill);
    add_hline(bounds.Min.X + 4.0F * scale, bounds.Max.X - 4.0F * scale, bounds.Min.Y + 1.0F, top_edge);
    add_soft_outline(bounds, 4.0F * scale, item.Focused ? theme.Accent : item.Hovered ? Color {theme.Accent.R, theme.Accent.G, theme.Accent.B, 0.36F} : theme.ButtonBorder);
    add_text({{bounds.Min.X + theme.FramePaddingX * scale, bounds.Min.Y + 6.0F * scale}, bounds.Max}, label, text_color);

    return item.Clicked;
}

}

void Text(std::string_view value)
{
    ensure_layout();
    const Style& theme = CurrentStyle();
    const float Height = theme.FontSize * theme.FrameScale * 1.45F;
    const Rect bounds = next_rect(text_bounds_width(value), Height);
    add_text(bounds, value, theme.Text);
    set_last_item(CurrentId(value), contains(active_clip(bounds), Input().MousePosition), false, focused_item == CurrentId(value));
}

void TextSecondary(std::string_view value)
{
    ensure_layout();
    const Style& theme = CurrentStyle();
    const float Height = theme.FontSize * theme.FrameScale * 1.45F;
    const Rect bounds = next_rect(text_bounds_width(value), Height);
    add_text(bounds, value, theme.TextSecondary);
    set_last_item(CurrentId(value), contains(active_clip(bounds), Input().MousePosition), false, focused_item == CurrentId(value));
}

void TitleText(std::string_view value)
{
    ensure_layout();
    const Style& theme = CurrentStyle();
    PushStyleVar(StyleVar::FontSize, 18.0F);
    const Style& title_theme = CurrentStyle();
    const float Height = title_theme.FontSize * title_theme.FrameScale * 1.35F;
    const Rect bounds = next_rect(text_bounds_width(value), Height);
    add_text(bounds, value, title_theme.Text);
    PopStyleVar();
    set_last_item(CurrentId(value), contains(active_clip(bounds), Input().MousePosition), false, focused_item == CurrentId(value));
}

void SectionText(std::string_view value)
{
    ensure_layout();
    const Style& theme = CurrentStyle();
    layout.Cursor.Y += 2.0F * theme.FrameScale;
    const Rect bounds = next_rect(text_width(value), 20.0F * theme.FrameScale);
    add_text(bounds, value, theme.TextSecondary);
    add_hline(layout.Cursor.X, layout.Cursor.X + layout.ContentWidth, bounds.Max.Y, {theme.WindowBorder.R, theme.WindowBorder.G, theme.WindowBorder.B, 0.48F});
    layout.Cursor.Y += 2.0F * theme.FrameScale;
    set_last_item(CurrentId(value), contains(active_clip(bounds), Input().MousePosition), false, focused_item == CurrentId(value));
}

void Separator()
{
    ensure_layout();
    const Style& theme = CurrentStyle();
    const float Width = layout.ContentWidth > 0.0F ? layout.ContentWidth : theme.ItemWidth * theme.FrameScale;
    add_hline(layout.Cursor.X, layout.Cursor.X + Width, layout.Cursor.Y + 4.0F * theme.FrameScale, {theme.WindowBorder.R, theme.WindowBorder.G, theme.WindowBorder.B, 0.65F});
    layout.Cursor.Y += theme.ItemSpacingY * theme.FrameScale;
}

void Spacing()
{
    ensure_layout();
    layout.Cursor.Y += CurrentStyle().SectionSpacingY * CurrentStyle().FrameScale;
}

void SameLine(float Spacing)
{
    ensure_layout();
    if (!layout.HasLastItem) {
        return;
    }

    const Style& theme = CurrentStyle();
    const float resolved_spacing = Spacing >= 0.0F ? Spacing : theme.ItemSpacingY * theme.FrameScale;
    layout.Cursor = {layout.LastItem.Max.X + resolved_spacing, layout.LastItem.Min.Y};
    layout.LineBottom = layout.LastItem.Max.Y;
}

void BeginGroup()
{
    ensure_layout();
    group_stack.push_back({
        .Parent = layout,
    });
    layout.HasLastItem = false;
    layout.LineBottom = layout.Cursor.Y;
}

void EndGroup()
{
    ensure_layout();
    if (group_stack.empty()) {
        throw std::logic_error("farcal Group stack is empty");
    }

    const layout_state group_layout = layout;
    layout = group_stack.back().Parent;
    group_stack.pop_back();

    const Rect bounds {
        layout.Cursor,
        {
            layout.ContentWidth > 0.0F ? layout.Cursor.X + layout.ContentWidth : std::max(layout.Cursor.X, group_layout.LastItem.Max.X),
            std::max(layout.Cursor.Y, group_layout.LineBottom),
        },
    };

    layout.Cursor.Y = bounds.Max.Y + CurrentStyle().ItemSpacingY * CurrentStyle().FrameScale;
    layout.LineBottom = bounds.Max.Y;
    layout.LastItem = bounds;
    layout.HasLastItem = true;
    set_last_item(CurrentId("Group"), contains(active_clip(bounds), Input().MousePosition), false, focused_item == CurrentId("Group"));
}

void Indent(float Width)
{
    ensure_layout();
    const float resolved_width = Width > 0.0F ? Width : 18.0F * CurrentStyle().FrameScale;
    layout.IndentWidth += resolved_width;
    layout.Cursor.X = layout.StartX + layout.IndentWidth;
}

void Unindent(float Width)
{
    ensure_layout();
    const float resolved_width = Width > 0.0F ? Width : 18.0F * CurrentStyle().FrameScale;
    layout.IndentWidth = std::max(0.0F, layout.IndentWidth - resolved_width);
    layout.Cursor.X = layout.StartX + layout.IndentWidth;
}

void Dummy(Vec2 size)
{
    ensure_layout();
    const Rect bounds = next_rect(size.X, size.Y);
    set_last_item(CurrentId("Dummy"), contains(active_clip(bounds), Input().MousePosition), false, focused_item == CurrentId("Dummy"));
}

void SetNextItemWidth(float Width)
{
    ensure_layout();
    layout.NextItemWidth = std::max(0.0F, Width);
}

bool IsItemHovered()
{
    ensure_layout();
    return last_hovered;
}

bool IsItemActive()
{
    ensure_layout();
    return last_active;
}

bool IsItemFocused()
{
    ensure_layout();
    return last_focused;
}

bool Button(std::string_view label)
{
    const Style& theme = CurrentStyle();
    return button_impl(label, theme.Button, theme.ButtonHovered, theme.ButtonActive, theme.Text);
}

bool PrimaryButton(std::string_view label)
{
    const Style& theme = CurrentStyle();
    return button_impl(label, theme.ButtonPrimary, theme.ButtonPrimaryHovered, theme.ButtonPrimaryActive, {1.0F, 1.0F, 1.0F, 1.0F});
}

bool Checkbox(std::string_view label, bool* value)
{
    ensure_layout();
    if (value == nullptr) {
        return false;
    }

    const Style& theme = CurrentStyle();
    const float scale = theme.FrameScale;
    const float Height = 24.0F * scale;
    const float box_size = 16.0F * scale;
    const float gap = 10.0F * scale;
    const float label_width = text_width(label);
    const float Width = layout.ContentWidth > 0.0F ? layout.ContentWidth : box_size + gap + label_width;
    const Rect bounds = next_rect(Width, Height);
    const std::uint64_t id = CurrentId(label);
    const item_result item = item_behavior(id, bounds);
    const bool changed = item.Clicked || keyboard_toggle(id);

    if (changed) {
        *value = !*value;
    }

    const float box_y = bounds.Min.Y + (Height - box_size) * 0.5F;
    const Rect box {{bounds.Min.X, box_y}, {bounds.Min.X + box_size, box_y + box_size}};
    const Color unchecked_fill = item.Active ? theme.ButtonActive : item.Hovered ? theme.ButtonHovered : theme.Button;
    const Color checked_fill = item.Active ? theme.ButtonPrimaryActive : item.Hovered ? theme.ButtonPrimaryHovered : theme.ButtonPrimary;
    const Color fill = *value ? checked_fill : unchecked_fill;
    const Color border = item.Focused ? theme.Accent : *value ? Color {theme.Accent.R, theme.Accent.G, theme.Accent.B, 0.82F} : item.Hovered ? Color {theme.Accent.R, theme.Accent.G, theme.Accent.B, 0.34F} : theme.ButtonBorder;

    add_filled_rect({{box.Min.X + 1.0F, box.Min.Y + 1.0F}, {box.Max.X - 1.0F, box.Max.Y - 1.0F}}, fill);
    add_hline(box.Min.X + 1.0F, box.Max.X - 1.0F, box.Min.Y + 1.0F, item.Hovered ? Color {1.0F, 1.0F, 1.0F, 0.12F} : Color {1.0F, 1.0F, 1.0F, 0.05F});
    add_soft_outline(box, 0.0F, border);

    if (*value) {
        const Color mark {1.0F, 1.0F, 1.0F, 0.96F};
        add_line({box.Min.X + 4.4F * scale, box.Min.Y + 8.3F * scale}, {box.Min.X + 7.0F * scale, box.Min.Y + 10.8F * scale}, mark, 1.35F * scale);
        add_line({box.Min.X + 7.0F * scale, box.Min.Y + 10.8F * scale}, {box.Min.X + 12.0F * scale, box.Min.Y + 5.7F * scale}, mark, 1.35F * scale);
    }

    const Color text_color = *value ? theme.Text : theme.TextSecondary;
    add_text({{box.Max.X + gap, bounds.Min.Y + 2.0F * scale}, bounds.Max}, label, text_color);

    return changed;
}

bool SliderFloat(std::string_view label, float* value, float minimum, float maximum)
{
    ensure_layout();
    if (value == nullptr) {
        return false;
    }

    if (maximum < minimum) {
        std::swap(minimum, maximum);
    }

    const Style& theme = CurrentStyle();
    const InputState& io = Input();
    const float scale = theme.FrameScale;
    const float Height = 42.0F * scale;
    const float Width = layout.ContentWidth > 0.0F ? layout.ContentWidth : theme.ItemWidth * scale;
    const Rect bounds = next_rect(Width, Height);
    const std::uint64_t id = CurrentId(label);
    const item_result item = item_behavior(id, bounds);
    bool changed = false;

    const float track_y = bounds.Min.Y + 30.0F * scale;
    const float track_start = bounds.Min.X;
    const float track_end = bounds.Max.X;
    const float track_width = std::max(1.0F, track_end - track_start);

    if ((item.Active && io.MouseDown[0]) || item.Clicked) {
        const float normalized = (io.MousePosition.X - track_start) / track_width;
        changed = set_float_value(value, value_from_normalized(normalized, minimum, maximum), minimum, maximum);
    }

    if (item.Focused) {
        const float step = (maximum - minimum) / 100.0F;
        if (io.KeyPressed[37]) {
            changed = set_float_value(value, *value - step, minimum, maximum) || changed;
        }
        if (io.KeyPressed[39]) {
            changed = set_float_value(value, *value + step, minimum, maximum) || changed;
        }
    }

    const float normalized = normalized_value(*value, minimum, maximum);
    const float thumb_x = track_start + track_width * normalized;
    const Color track_color = item.Hovered || item.Active ? theme.ButtonHovered : theme.Button;
    const Color progress_color = item.Active ? theme.ButtonPrimaryActive : item.Hovered ? theme.ButtonPrimaryHovered : theme.ButtonPrimary;
    const Color thumb_fill = item.Active ? theme.Selection : theme.WindowPanel;
    const Color thumb_border = item.Focused ? theme.Accent : item.Hovered || item.Active ? Color {theme.Accent.R, theme.Accent.G, theme.Accent.B, 0.70F} : theme.ButtonBorder;

    char value_text[32] {};
    std::snprintf(value_text, sizeof(value_text), "%.2f", static_cast<double>(*value));
    const float value_width = text_width(value_text);

    add_text({bounds.Min, {bounds.Max.X - value_width - 10.0F * scale, bounds.Min.Y + 20.0F * scale}}, label, theme.TextSecondary);
    add_text({{bounds.Max.X - value_width, bounds.Min.Y}, {bounds.Max.X, bounds.Min.Y + 20.0F * scale}}, value_text, item.Active ? theme.Text : theme.TextSecondary);

    add_line({track_start, track_y}, {track_end, track_y}, track_color, 4.0F * scale);
    add_line({track_start, track_y}, {thumb_x, track_y}, progress_color, 4.0F * scale);

    const Rect thumb {{thumb_x - 4.0F * scale, track_y - 8.0F * scale}, {thumb_x + 4.0F * scale, track_y + 8.0F * scale}};
    add_filled_rect({{thumb.Min.X + 1.0F, thumb.Min.Y + 1.0F}, {thumb.Max.X - 1.0F, thumb.Max.Y - 1.0F}}, thumb_fill);
    add_soft_outline(thumb, 0.0F, thumb_border);

    return changed;
}

bool BeginWindow(std::string_view Title)
{
    ensure_layout();
    const Style& theme = CurrentStyle();
    const float scale = theme.FrameScale;
    const float Width = theme.WindowWidth * scale;
    const float title_height = theme.WindowTitleHeight * scale;
    window_state& state = windows[std::string(Title)];
    const float Height = theme.WindowHeight * scale;

    if (!state.Initialized) {
        state.Position = layout.Cursor;
        state.Initialized = true;
    }

    const InputState& io = Input();
    Rect bounds {
        state.Position,
        {state.Position.X + Width, state.Position.Y + Height},
    };
    const Rect title_bounds {bounds.Min, {bounds.Max.X, bounds.Min.Y + title_height}};
    const bool Hovered = contains(bounds, io.MousePosition);
    const bool title_hovered = contains(title_bounds, io.MousePosition);

    if (title_hovered && io.MousePressed[0]) {
        state.Dragging = true;
        state.DragOffset = {
            io.MousePosition.X - state.Position.X,
            io.MousePosition.Y - state.Position.Y,
        };
    }

    if (!io.MouseDown[0]) {
        state.Dragging = false;
    }

    if (state.Dragging) {
        state.Position = {
            io.MousePosition.X - state.DragOffset.X,
            io.MousePosition.Y - state.DragOffset.Y,
        };

        bounds = {
            state.Position,
            {state.Position.X + Width, state.Position.Y + Height},
        };
    }

    advance_layout(Height);

    const Rect current_title_bounds {bounds.Min, {bounds.Max.X, bounds.Min.Y + title_height}};
    const Rect content_bounds {{bounds.Min.X + 1.0F, bounds.Min.Y + title_height}, {bounds.Max.X - 1.0F, bounds.Max.Y - 1.0F}};
    const Rect scroll_clip {{bounds.Min.X + 18.0F * scale, bounds.Min.Y + title_height + 16.0F * scale}, {bounds.Max.X - 18.0F * scale, bounds.Max.Y - 16.0F * scale}};
    const Rect shadow_1 {{bounds.Min.X + 3.0F * scale, bounds.Min.Y + 4.0F * scale}, {bounds.Max.X + 3.0F * scale, bounds.Max.Y + 4.0F * scale}};
    const Rect shadow_2 {{bounds.Min.X + 1.0F * scale, bounds.Min.Y + 2.0F * scale}, {bounds.Max.X + 1.0F * scale, bounds.Max.Y + 2.0F * scale}};

    add_soft_rect(shadow_1, 4.0F * scale, {0.0F, 0.0F, 0.0F, 0.18F});
    add_soft_rect(shadow_2, 4.0F * scale, {0.0F, 0.0F, 0.0F, 0.14F});
    add_soft_rect(bounds, 4.0F * scale, theme.WindowBackground);
    add_filled_rect(content_bounds, theme.WindowBackground);
    add_soft_rect(current_title_bounds, 4.0F * scale, Hovered ? theme.WindowTitleActive : theme.WindowTitle);
    add_filled_rect({{current_title_bounds.Min.X, current_title_bounds.Max.Y - 4.0F * scale}, current_title_bounds.Max}, Hovered ? theme.WindowTitleActive : theme.WindowTitle);
    add_hline(bounds.Min.X, bounds.Max.X, current_title_bounds.Max.Y, {theme.WindowBorder.R, theme.WindowBorder.G, theme.WindowBorder.B, 0.70F});
    add_soft_outline(bounds, 4.0F * scale, theme.WindowBorder);
    add_text({{current_title_bounds.Min.X + 10.0F * scale, current_title_bounds.Min.Y + 10.0F * scale}, current_title_bounds.Max}, Title, theme.Text);

    const float VisibleHeight = scroll_clip.Max.Y - scroll_clip.Min.Y;
    const float MaxScroll = std::max(0.0F, state.ContentHeight - VisibleHeight);
    if (Hovered && io.MouseWheel != 0.0F) {
        state.ScrollY = std::clamp(state.ScrollY - io.MouseWheel * 44.0F * scale, 0.0F, MaxScroll);
    }
    state.ScrollY = std::clamp(state.ScrollY, 0.0F, MaxScroll);

    const Vec2 content_cursor {
        scroll_clip.Min.X,
        scroll_clip.Min.Y - state.ScrollY,
    };

    layout_stack.push_back(layout);
    window_stack.push_back({
        .Title = std::string(Title),
        .ContentStartY = content_cursor.Y,
        .ContentPaddingY = 16.0F * scale,
        .VisibleHeight = VisibleHeight,
        .MaxScroll = MaxScroll,
        .Hovered = Hovered,
        .ScrollTrack = {{scroll_clip.Max.X - 7.0F * scale, scroll_clip.Min.Y}, {scroll_clip.Max.X - 3.0F * scale, scroll_clip.Max.Y}},
    });
    layout.Cursor = content_cursor;
    layout.StartX = content_cursor.X;
    layout.ContentWidth = scroll_clip.Max.X - scroll_clip.Min.X - 18.0F * scale;
    layout.Clip = scroll_clip;
    layout.HasClip = true;
    layout.HasLastItem = false;
    layout.LineBottom = content_cursor.Y;
    layout.NextItemWidth = 0.0F;
    layout.IndentWidth = 0.0F;

    return true;
}

void EndWindow()
{
    if (layout_stack.empty() || window_stack.empty()) {
        throw std::logic_error("farcal Window stack is empty");
    }

    const window_layout_state window_layout = window_stack.back();
    window_stack.pop_back();

    window_state& state = windows[window_layout.Title];
    state.ContentHeight = std::max(0.0F, layout.Cursor.Y - window_layout.ContentStartY + window_layout.ContentPaddingY);

    if (window_layout.MaxScroll > 0.0F) {
        const Style& theme = CurrentStyle();
        const Rect track = window_layout.ScrollTrack;
        const float track_height = track.Max.Y - track.Min.Y;
        const float thumb_height = std::max(28.0F * theme.FrameScale, track_height * std::min(1.0F, window_layout.VisibleHeight / state.ContentHeight));
        const float thumb_y = track.Min.Y + (track_height - thumb_height) * (state.ScrollY / window_layout.MaxScroll);
        const Rect thumb {{track.Min.X, thumb_y}, {track.Max.X, thumb_y + thumb_height}};

        add_soft_rect(track, 2.0F * theme.FrameScale, {0.0F, 0.0F, 0.0F, 0.20F});
        add_soft_rect(thumb, 2.0F * theme.FrameScale, {theme.TextMuted.R, theme.TextMuted.G, theme.TextMuted.B, window_layout.Hovered ? 0.70F : 0.42F});
    }

    layout = layout_stack.back();
    layout_stack.pop_back();
}

}
