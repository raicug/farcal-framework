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
    float content_width {};
    rect clip {};
    bool has_clip {};
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

layout_state layout;
std::vector<layout_state> layout_stack;
std::vector<window_layout_state> window_stack;
std::unordered_map<std::string, window_state> windows;
std::uint64_t layout_frame = static_cast<std::uint64_t>(-1);

void ensure_layout()
{
    if (layout_frame == frame_index()) {
        return;
    }

    layout = {};
    layout_stack.clear();
    window_stack.clear();
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
    rect bounds {
        layout.cursor,
        {layout.cursor.x + width, layout.cursor.y + height},
    };
    layout.cursor.y += height + current_style().item_spacing_y * current_style().frame_scale;
    return bounds;
}

void advance_layout(float height)
{
    layout.cursor.y += height + current_style().item_spacing_y * current_style().frame_scale;
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
    });
}

void add_hline(float x0, float x1, float y, color tint)
{
    add_filled_rect({{x0, y}, {x1, y + 1.0F}}, tint);
}

void add_vline(float x, float y0, float y1, color tint)
{
    add_filled_rect({{x, y0}, {x + 1.0F, y1}}, tint);
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
    add_hline(bounds.min.x + radius, bounds.max.x - radius, bounds.min.y, tint);
    add_hline(bounds.min.x + radius, bounds.max.x - radius, bounds.max.y - 1.0F, tint);
    add_vline(bounds.min.x, bounds.min.y + radius, bounds.max.y - radius, tint);
    add_vline(bounds.max.x - 1.0F, bounds.min.y + radius, bounds.max.y - radius, tint);
    add_filled_rect({{bounds.min.x + 1.0F, bounds.min.y + 1.0F}, {bounds.min.x + radius, bounds.min.y + 2.0F}}, tint);
    add_filled_rect({{bounds.max.x - radius, bounds.min.y + 1.0F}, {bounds.max.x - 1.0F, bounds.min.y + 2.0F}}, tint);
    add_filled_rect({{bounds.min.x + 1.0F, bounds.max.y - 2.0F}, {bounds.min.x + radius, bounds.max.y - 1.0F}}, tint);
    add_filled_rect({{bounds.max.x - radius, bounds.max.y - 2.0F}, {bounds.max.x - 1.0F, bounds.max.y - 1.0F}}, tint);
}

bool button_impl(std::string_view label, color normal, color hovered_color, color active_color, color text_color)
{
    ensure_layout();
    const style& theme = current_style();
    const float scale = theme.frame_scale;
    const float height = 30.0F * scale;
    const float width = layout.content_width > 0.0F ? std::min(theme.item_width * scale, layout.content_width) : theme.item_width * scale;
    const rect bounds = next_rect(width, height);
    const input_state& io = input();
    const bool hovered = contains(active_clip(bounds), io.mouse_position);
    const bool clicked = hovered && io.mouse_pressed[0];
    const color fill = clicked ? active_color : hovered ? hovered_color : normal;
    const color top_edge = hovered ? color {theme.accent.r, theme.accent.g, theme.accent.b, 0.28F} : color {1.0F, 1.0F, 1.0F, 0.025F};

    add_soft_rect(bounds, 4.0F * scale, fill);
    add_hline(bounds.min.x + 4.0F * scale, bounds.max.x - 4.0F * scale, bounds.min.y + 1.0F, top_edge);
    add_soft_outline(bounds, 4.0F * scale, hovered ? color {theme.accent.r, theme.accent.g, theme.accent.b, 0.36F} : theme.button_border);
    add_text({{bounds.min.x + theme.frame_padding_x * scale, bounds.min.y + 6.0F * scale}, bounds.max}, label, text_color);

    return clicked;
}

}

void text(std::string_view value)
{
    ensure_layout();
    const style& theme = current_style();
    const float height = theme.font_size * theme.frame_scale * 1.45F;
    const rect bounds = next_rect(text_bounds_width(value), height);
    add_text(bounds, value, theme.text);
}

void text_secondary(std::string_view value)
{
    ensure_layout();
    const style& theme = current_style();
    const float height = theme.font_size * theme.frame_scale * 1.45F;
    const rect bounds = next_rect(text_bounds_width(value), height);
    add_text(bounds, value, theme.text_secondary);
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
    layout.content_width = scroll_clip.max.x - scroll_clip.min.x - 18.0F * scale;
    layout.clip = scroll_clip;
    layout.has_clip = true;

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
