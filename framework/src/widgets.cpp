// clang-format off
#include <framework/widgets.hpp>

#include <framework/context.hpp>

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
// clang-format on

namespace farcal {
namespace {

struct layout_state {
    vec2 cursor {24.0F, 24.0F};
    float start_x {24.0F};
    float content_width {};
    rect clip {};
    bool has_clip {};
    rect last_item {};
    bool has_last_item {};
    float line_bottom {};
    float next_item_width {};
    float indent_width {};
};

struct window_state {
    vec2 position {};
    vec2 drag_offset {};
    float content_height {};
    float scroll_y {};
    bool initialized {};
    bool dragging {};
};

struct window_layout_state {
    std::string title {};
    float content_start_y {};
    float content_padding_y {};
    float visible_height {};
    float max_scroll {};
    bool hovered {};
    rect scroll_track {};
};

struct group_state {
    layout_state parent {};
};

struct item_result {
    bool hovered {};
    bool active {};
    bool focused {};
    bool clicked {};
};

layout_state layout;
std::vector<layout_state> layout_stack;
std::vector<window_layout_state> window_stack;
std::vector<group_state> group_stack;
std::unordered_map<std::string, window_state> windows;
std::uint64_t layout_frame = static_cast<std::uint64_t>(-1);
std::uint64_t active_item {};
std::uint64_t focused_item {};
std::uint64_t last_item {};
bool last_hovered {};
bool last_active {};
bool last_focused {};

void ensure_layout()
{
    if (layout_frame == frame_index()) {
        return;
    }

    layout = {};
    layout_stack.clear();
    window_stack.clear();
    group_stack.clear();
    last_item = 0;
    last_hovered = false;
    last_active = false;
    last_focused = false;
    layout_frame = frame_index();
}

draw_data& mutable_draw()
{
    return main_renderer();
}

bool contains(rect bounds, vec2 point)
{
    return point.x >= bounds.min.x && point.y >= bounds.min.y && point.x <= bounds.max.x && point.y <= bounds.max.y;
}

rect intersect(rect a, rect b)
{
    return {
        {std::max(a.min.x, b.min.x), std::max(a.min.y, b.min.y)},
        {std::min(a.max.x, b.max.x), std::min(a.max.y, b.max.y)},
    };
}

rect active_clip(rect bounds)
{
    if (!layout.has_clip) {
        return bounds;
    }

    return intersect(bounds, layout.clip);
}

rect command_clip(rect bounds)
{
    if (!layout.has_clip) {
        return bounds;
    }

    return layout.clip;
}

float text_width(std::string_view value)
{
    const style& theme = current_style();
    return std::max(24.0F, static_cast<float>(value.size()) * theme.font_size * theme.frame_scale * 0.54F);
}

float text_bounds_width(std::string_view value)
{
    if (layout.content_width > 0.0F) {
        return layout.content_width;
    }

    return text_width(value);
}

rect next_rect(float width, float height)
{
    if (layout.next_item_width > 0.0F) {
        width = layout.next_item_width;
        layout.next_item_width = 0.0F;
    }

    const float spacing = current_style().item_spacing_y * current_style().frame_scale;
    rect bounds {
        layout.cursor,
        {layout.cursor.x + width, layout.cursor.y + height},
    };
    const float bottom = std::max(layout.line_bottom, bounds.max.y);
    layout.cursor = {layout.start_x + layout.indent_width, bottom + spacing};
    layout.line_bottom = bottom;
    layout.last_item = bounds;
    layout.has_last_item = true;
    return bounds;
}

void advance_layout(float height)
{
    const float spacing = current_style().item_spacing_y * current_style().frame_scale;
    layout.cursor.y += height + spacing;
    layout.line_bottom = layout.cursor.y - spacing;
}

void set_last_item(std::uint64_t id, bool hovered, bool active, bool focused)
{
    last_item = id;
    last_hovered = hovered;
    last_active = active;
    last_focused = focused;
}

item_result item_behavior(std::uint64_t id, rect bounds)
{
    const input_state& io = input();
    if (!io.mouse_down[0] && !io.mouse_pressed[0]) {
        active_item = 0;
    }

    const bool hovered = contains(active_clip(bounds), io.mouse_position);
    if (hovered && io.mouse_pressed[0]) {
        active_item = id;
        focused_item = id;
    }

    const bool active = active_item == id;
    const bool focused = focused_item == id;
    const bool clicked = hovered && io.mouse_pressed[0];
    set_last_item(id, hovered, active, focused);

    return {
        .hovered = hovered,
        .active = active,
        .focused = focused,
        .clicked = clicked,
    };
}

bool keyboard_toggle(std::uint64_t id)
{
    const input_state& io = input();
    return focused_item == id && (io.key_pressed[13] || io.key_pressed[32]);
}

void add_text(rect bounds, std::string_view value, color tint)
{
    const style& theme = current_style();
    mutable_draw().commands.push_back({
        .type = draw_command_type::text,
        .bounds = bounds,
        .clip = command_clip(bounds),
        .tint = tint,
        .font_size = theme.font_size * theme.frame_scale,
        .text = std::string(value),
        .font_family = theme.font_family,
    });
}

void add_filled_rect(rect bounds, color tint)
{
    mutable_draw().commands.push_back({
        .type = draw_command_type::filled_rect,
        .bounds = bounds,
        .clip = command_clip(bounds),
        .tint = tint,
        .anti_aliasing = current_style().anti_aliasing,
    });
}

void add_rect(rect bounds, color tint, float thickness = 1.0F)
{
    mutable_draw().commands.push_back({
        .type = draw_command_type::rect,
        .bounds = bounds,
        .clip = command_clip(bounds),
        .tint = tint,
        .thickness = thickness,
        .anti_aliasing = current_style().anti_aliasing,
    });
}

void add_line(vec2 start, vec2 end, color tint, float thickness)
{
    const rect bounds {
        {std::min(start.x, end.x), std::min(start.y, end.y)},
        {std::max(start.x, end.x), std::max(start.y, end.y)},
    };

    mutable_draw().commands.push_back({
        .type = draw_command_type::line,
        .clip = command_clip(bounds),
        .start = start,
        .end = end,
        .tint = tint,
        .thickness = thickness,
        .anti_aliasing = current_style().anti_aliasing,
    });
}

void add_hline(float x0, float x1, float y, color tint)
{
    add_line({x0, y + 0.5F}, {x1, y + 0.5F}, tint, 1.0F);
}

void add_vline(float x, float y0, float y1, color tint)
{
    add_line({x + 0.5F, y0}, {x + 0.5F, y1}, tint, 1.0F);
}

void add_soft_rect(rect bounds, float radius, color tint)
{
    add_filled_rect({{bounds.min.x + radius, bounds.min.y}, {bounds.max.x - radius, bounds.max.y}}, tint);
    add_filled_rect({{bounds.min.x, bounds.min.y + radius}, {bounds.max.x, bounds.max.y - radius}}, tint);
    add_filled_rect({{bounds.min.x + 1.0F, bounds.min.y + 1.0F}, {bounds.min.x + radius, bounds.min.y + radius}}, tint);
    add_filled_rect({{bounds.max.x - radius, bounds.min.y + 1.0F}, {bounds.max.x - 1.0F, bounds.min.y + radius}}, tint);
    add_filled_rect({{bounds.min.x + 1.0F, bounds.max.y - radius}, {bounds.min.x + radius, bounds.max.y - 1.0F}}, tint);
    add_filled_rect({{bounds.max.x - radius, bounds.max.y - radius}, {bounds.max.x - 1.0F, bounds.max.y - 1.0F}}, tint);
}

void add_soft_outline(rect bounds, float radius, color tint)
{
    (void)radius;

    const float left = bounds.min.x + 0.5F;
    const float top = bounds.min.y + 0.5F;
    const float right = bounds.max.x - 0.5F;
    const float bottom = bounds.max.y - 0.5F;

    add_line({left, top}, {right, top}, tint, 1.0F);
    add_line({right, top}, {right, bottom}, tint, 1.0F);
    add_line({right, bottom}, {left, bottom}, tint, 1.0F);
    add_line({left, bottom}, {left, top}, tint, 1.0F);
}

bool button_impl(std::string_view label, color normal, color hovered_color, color active_color, color text_color)
{
    ensure_layout();
    const style& theme = current_style();
    const float scale = theme.frame_scale;
    const float height = 30.0F * scale;
    const float width = layout.content_width > 0.0F ? std::min(theme.item_width * scale, layout.content_width) : theme.item_width * scale;
    const rect bounds = next_rect(width, height);
    const item_result item = item_behavior(current_id(label), bounds);
    const color fill = item.active ? active_color : item.hovered ? hovered_color : normal;
    const color top_edge = item.hovered ? color {theme.accent.r, theme.accent.g, theme.accent.b, 0.28F} : color {1.0F, 1.0F, 1.0F, 0.025F};

    add_soft_rect(bounds, 4.0F * scale, fill);
    add_hline(bounds.min.x + 4.0F * scale, bounds.max.x - 4.0F * scale, bounds.min.y + 1.0F, top_edge);
    add_soft_outline(bounds, 4.0F * scale, item.focused ? theme.accent : item.hovered ? color {theme.accent.r, theme.accent.g, theme.accent.b, 0.36F} : theme.button_border);
    add_text({{bounds.min.x + theme.frame_padding_x * scale, bounds.min.y + 6.0F * scale}, bounds.max}, label, text_color);

    return item.clicked;
}

}

void text(std::string_view value)
{
    ensure_layout();
    const style& theme = current_style();
    const float height = theme.font_size * theme.frame_scale * 1.45F;
    const rect bounds = next_rect(text_bounds_width(value), height);
    add_text(bounds, value, theme.text);
    set_last_item(current_id(value), contains(active_clip(bounds), input().mouse_position), false, focused_item == current_id(value));
}

void text_secondary(std::string_view value)
{
    ensure_layout();
    const style& theme = current_style();
    const float height = theme.font_size * theme.frame_scale * 1.45F;
    const rect bounds = next_rect(text_bounds_width(value), height);
    add_text(bounds, value, theme.text_secondary);
    set_last_item(current_id(value), contains(active_clip(bounds), input().mouse_position), false, focused_item == current_id(value));
}

void title_text(std::string_view value)
{
    ensure_layout();
    const style& theme = current_style();
    push_style_var(style_var::font_size, 18.0F);
    const style& title_theme = current_style();
    const float height = title_theme.font_size * title_theme.frame_scale * 1.35F;
    const rect bounds = next_rect(text_bounds_width(value), height);
    add_text(bounds, value, title_theme.text);
    pop_style_var();
    set_last_item(current_id(value), contains(active_clip(bounds), input().mouse_position), false, focused_item == current_id(value));
}

void section_text(std::string_view value)
{
    ensure_layout();
    const style& theme = current_style();
    layout.cursor.y += 2.0F * theme.frame_scale;
    const rect bounds = next_rect(text_width(value), 20.0F * theme.frame_scale);
    add_text(bounds, value, theme.text_secondary);
    add_hline(layout.cursor.x, layout.cursor.x + layout.content_width, bounds.max.y, {theme.window_border.r, theme.window_border.g, theme.window_border.b, 0.48F});
    layout.cursor.y += 2.0F * theme.frame_scale;
    set_last_item(current_id(value), contains(active_clip(bounds), input().mouse_position), false, focused_item == current_id(value));
}

void separator()
{
    ensure_layout();
    const style& theme = current_style();
    const float width = layout.content_width > 0.0F ? layout.content_width : theme.item_width * theme.frame_scale;
    add_hline(layout.cursor.x, layout.cursor.x + width, layout.cursor.y + 4.0F * theme.frame_scale, {theme.window_border.r, theme.window_border.g, theme.window_border.b, 0.65F});
    layout.cursor.y += theme.item_spacing_y * theme.frame_scale;
}

void spacing()
{
    ensure_layout();
    layout.cursor.y += current_style().section_spacing_y * current_style().frame_scale;
}

void same_line(float spacing)
{
    ensure_layout();
    if (!layout.has_last_item) {
        return;
    }

    const style& theme = current_style();
    const float resolved_spacing = spacing >= 0.0F ? spacing : theme.item_spacing_y * theme.frame_scale;
    layout.cursor = {layout.last_item.max.x + resolved_spacing, layout.last_item.min.y};
    layout.line_bottom = layout.last_item.max.y;
}

void begin_group()
{
    ensure_layout();
    group_stack.push_back({
        .parent = layout,
    });
    layout.has_last_item = false;
    layout.line_bottom = layout.cursor.y;
}

void end_group()
{
    ensure_layout();
    if (group_stack.empty()) {
        throw std::logic_error("farcal group stack is empty");
    }

    const layout_state group_layout = layout;
    layout = group_stack.back().parent;
    group_stack.pop_back();

    const rect bounds {
        layout.cursor,
        {
            layout.content_width > 0.0F ? layout.cursor.x + layout.content_width : std::max(layout.cursor.x, group_layout.last_item.max.x),
            std::max(layout.cursor.y, group_layout.line_bottom),
        },
    };

    layout.cursor.y = bounds.max.y + current_style().item_spacing_y * current_style().frame_scale;
    layout.line_bottom = bounds.max.y;
    layout.last_item = bounds;
    layout.has_last_item = true;
    set_last_item(current_id("group"), contains(active_clip(bounds), input().mouse_position), false, focused_item == current_id("group"));
}

void indent(float width)
{
    ensure_layout();
    const float resolved_width = width > 0.0F ? width : 18.0F * current_style().frame_scale;
    layout.indent_width += resolved_width;
    layout.cursor.x = layout.start_x + layout.indent_width;
}

void unindent(float width)
{
    ensure_layout();
    const float resolved_width = width > 0.0F ? width : 18.0F * current_style().frame_scale;
    layout.indent_width = std::max(0.0F, layout.indent_width - resolved_width);
    layout.cursor.x = layout.start_x + layout.indent_width;
}

void dummy(vec2 size)
{
    ensure_layout();
    const rect bounds = next_rect(size.x, size.y);
    set_last_item(current_id("dummy"), contains(active_clip(bounds), input().mouse_position), false, focused_item == current_id("dummy"));
}

void set_next_item_width(float width)
{
    ensure_layout();
    layout.next_item_width = std::max(0.0F, width);
}

bool is_item_hovered()
{
    ensure_layout();
    return last_hovered;
}

bool is_item_active()
{
    ensure_layout();
    return last_active;
}

bool is_item_focused()
{
    ensure_layout();
    return last_focused;
}

bool button(std::string_view label)
{
    const style& theme = current_style();
    return button_impl(label, theme.button, theme.button_hovered, theme.button_active, theme.text);
}

bool primary_button(std::string_view label)
{
    const style& theme = current_style();
    return button_impl(label, theme.button_primary, theme.button_primary_hovered, theme.button_primary_active, {1.0F, 1.0F, 1.0F, 1.0F});
}

bool checkbox(std::string_view label, bool* value)
{
    ensure_layout();
    if (value == nullptr) {
        return false;
    }

    const style& theme = current_style();
    const float scale = theme.frame_scale;
    const float height = 24.0F * scale;
    const float box_size = 16.0F * scale;
    const float gap = 10.0F * scale;
    const float label_width = text_width(label);
    const float width = layout.content_width > 0.0F ? layout.content_width : box_size + gap + label_width;
    const rect bounds = next_rect(width, height);
    const std::uint64_t id = current_id(label);
    const item_result item = item_behavior(id, bounds);
    const bool changed = item.clicked || keyboard_toggle(id);

    if (changed) {
        *value = !*value;
    }

    const float box_y = bounds.min.y + (height - box_size) * 0.5F;
    const rect box {{bounds.min.x, box_y}, {bounds.min.x + box_size, box_y + box_size}};
    const color unchecked_fill = item.active ? theme.button_active : item.hovered ? theme.button_hovered : theme.button;
    const color checked_fill = item.active ? theme.button_primary_active : item.hovered ? theme.button_primary_hovered : theme.button_primary;
    const color fill = *value ? checked_fill : unchecked_fill;
    const color border = item.focused ? theme.accent : *value ? color {theme.accent.r, theme.accent.g, theme.accent.b, 0.82F} : item.hovered ? color {theme.accent.r, theme.accent.g, theme.accent.b, 0.34F} : theme.button_border;

    add_filled_rect({{box.min.x + 1.0F, box.min.y + 1.0F}, {box.max.x - 1.0F, box.max.y - 1.0F}}, fill);
    add_hline(box.min.x + 1.0F, box.max.x - 1.0F, box.min.y + 1.0F, item.hovered ? color {1.0F, 1.0F, 1.0F, 0.12F} : color {1.0F, 1.0F, 1.0F, 0.05F});
    add_soft_outline(box, 0.0F, border);

    if (*value) {
        const color mark {1.0F, 1.0F, 1.0F, 0.96F};
        add_line({box.min.x + 4.4F * scale, box.min.y + 8.3F * scale}, {box.min.x + 7.0F * scale, box.min.y + 10.8F * scale}, mark, 1.35F * scale);
        add_line({box.min.x + 7.0F * scale, box.min.y + 10.8F * scale}, {box.min.x + 12.0F * scale, box.min.y + 5.7F * scale}, mark, 1.35F * scale);
    }

    const color text_color = *value ? theme.text : theme.text_secondary;
    add_text({{box.max.x + gap, bounds.min.y + 2.0F * scale}, bounds.max}, label, text_color);

    return changed;
}

bool begin_window(std::string_view title)
{
    ensure_layout();
    const style& theme = current_style();
    const float scale = theme.frame_scale;
    const float width = theme.window_width * scale;
    const float title_height = theme.window_title_height * scale;
    window_state& state = windows[std::string(title)];
    const float height = theme.window_height * scale;

    if (!state.initialized) {
        state.position = layout.cursor;
        state.initialized = true;
    }

    const input_state& io = input();
    rect bounds {
        state.position,
        {state.position.x + width, state.position.y + height},
    };
    const rect title_bounds {bounds.min, {bounds.max.x, bounds.min.y + title_height}};
    const bool hovered = contains(bounds, io.mouse_position);
    const bool title_hovered = contains(title_bounds, io.mouse_position);

    if (title_hovered && io.mouse_pressed[0]) {
        state.dragging = true;
        state.drag_offset = {
            io.mouse_position.x - state.position.x,
            io.mouse_position.y - state.position.y,
        };
    }

    if (!io.mouse_down[0]) {
        state.dragging = false;
    }

    if (state.dragging) {
        state.position = {
            io.mouse_position.x - state.drag_offset.x,
            io.mouse_position.y - state.drag_offset.y,
        };

        bounds = {
            state.position,
            {state.position.x + width, state.position.y + height},
        };
    }

    advance_layout(height);

    const rect current_title_bounds {bounds.min, {bounds.max.x, bounds.min.y + title_height}};
    const rect content_bounds {{bounds.min.x + 1.0F, bounds.min.y + title_height}, {bounds.max.x - 1.0F, bounds.max.y - 1.0F}};
    const rect scroll_clip {{bounds.min.x + 18.0F * scale, bounds.min.y + title_height + 16.0F * scale}, {bounds.max.x - 18.0F * scale, bounds.max.y - 16.0F * scale}};
    const rect shadow_1 {{bounds.min.x + 3.0F * scale, bounds.min.y + 4.0F * scale}, {bounds.max.x + 3.0F * scale, bounds.max.y + 4.0F * scale}};
    const rect shadow_2 {{bounds.min.x + 1.0F * scale, bounds.min.y + 2.0F * scale}, {bounds.max.x + 1.0F * scale, bounds.max.y + 2.0F * scale}};

    add_soft_rect(shadow_1, 4.0F * scale, {0.0F, 0.0F, 0.0F, 0.18F});
    add_soft_rect(shadow_2, 4.0F * scale, {0.0F, 0.0F, 0.0F, 0.14F});
    add_soft_rect(bounds, 4.0F * scale, theme.window_background);
    add_filled_rect(content_bounds, theme.window_background);
    add_soft_rect(current_title_bounds, 4.0F * scale, hovered ? theme.window_title_active : theme.window_title);
    add_filled_rect({{current_title_bounds.min.x, current_title_bounds.max.y - 4.0F * scale}, current_title_bounds.max}, hovered ? theme.window_title_active : theme.window_title);
    add_hline(bounds.min.x, bounds.max.x, current_title_bounds.max.y, {theme.window_border.r, theme.window_border.g, theme.window_border.b, 0.70F});
    add_soft_outline(bounds, 4.0F * scale, theme.window_border);
    add_text({{current_title_bounds.min.x + 10.0F * scale, current_title_bounds.min.y + 10.0F * scale}, current_title_bounds.max}, title, theme.text);

    const float visible_height = scroll_clip.max.y - scroll_clip.min.y;
    const float max_scroll = std::max(0.0F, state.content_height - visible_height);
    if (hovered && io.mouse_wheel != 0.0F) {
        state.scroll_y = std::clamp(state.scroll_y - io.mouse_wheel * 44.0F * scale, 0.0F, max_scroll);
    }
    state.scroll_y = std::clamp(state.scroll_y, 0.0F, max_scroll);

    const vec2 content_cursor {
        scroll_clip.min.x,
        scroll_clip.min.y - state.scroll_y,
    };

    layout_stack.push_back(layout);
    window_stack.push_back({
        .title = std::string(title),
        .content_start_y = content_cursor.y,
        .content_padding_y = 16.0F * scale,
        .visible_height = visible_height,
        .max_scroll = max_scroll,
        .hovered = hovered,
        .scroll_track = {{scroll_clip.max.x - 7.0F * scale, scroll_clip.min.y}, {scroll_clip.max.x - 3.0F * scale, scroll_clip.max.y}},
    });
    layout.cursor = content_cursor;
    layout.start_x = content_cursor.x;
    layout.content_width = scroll_clip.max.x - scroll_clip.min.x - 18.0F * scale;
    layout.clip = scroll_clip;
    layout.has_clip = true;
    layout.has_last_item = false;
    layout.line_bottom = content_cursor.y;
    layout.next_item_width = 0.0F;
    layout.indent_width = 0.0F;

    return true;
}

void end_window()
{
    if (layout_stack.empty() || window_stack.empty()) {
        throw std::logic_error("farcal window stack is empty");
    }

    const window_layout_state window_layout = window_stack.back();
    window_stack.pop_back();

    window_state& state = windows[window_layout.title];
    state.content_height = std::max(0.0F, layout.cursor.y - window_layout.content_start_y + window_layout.content_padding_y);

    if (window_layout.max_scroll > 0.0F) {
        const style& theme = current_style();
        const rect track = window_layout.scroll_track;
        const float track_height = track.max.y - track.min.y;
        const float thumb_height = std::max(28.0F * theme.frame_scale, track_height * std::min(1.0F, window_layout.visible_height / state.content_height));
        const float thumb_y = track.min.y + (track_height - thumb_height) * (state.scroll_y / window_layout.max_scroll);
        const rect thumb {{track.min.x, thumb_y}, {track.max.x, thumb_y + thumb_height}};

        add_soft_rect(track, 2.0F * theme.frame_scale, {0.0F, 0.0F, 0.0F, 0.20F});
        add_soft_rect(thumb, 2.0F * theme.frame_scale, {theme.text_muted.r, theme.text_muted.g, theme.text_muted.b, window_layout.hovered ? 0.70F : 0.42F});
    }

    layout = layout_stack.back();
    layout_stack.pop_back();
}

}
