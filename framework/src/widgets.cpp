#include <framework/internal/widgets.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace farcal::widget_internal {

layout_state layout;
std::vector<layout_state> layout_stack;
std::vector<window_layout_state> window_stack;
std::vector<group_state> group_stack;
std::vector<list_state> list_stack;
std::unordered_map<std::string, window_state> windows;
std::uint64_t layout_frame = static_cast<std::uint64_t>(-1);
std::uint64_t active_item{};
std::uint64_t focused_item{};
std::uint64_t LastItem{};
std::uint64_t open_dropdown{};
bool last_hovered{};
bool last_active{};
bool last_focused{};

void ensure_layout() {
  if (layout_frame == FrameIndex()) {
    return;
  }

  layout = {};
  layout_stack.clear();
  window_stack.clear();
  group_stack.clear();
  list_stack.clear();
  LastItem = 0;
  last_hovered = false;
  last_active = false;
  last_focused = false;
  layout_frame = FrameIndex();
}

DrawData &mutable_draw() { return MainRenderer(); }

bool contains(Rect bounds, Vec2 point) {
  return point.X >= bounds.Min.X && point.Y >= bounds.Min.Y &&
         point.X <= bounds.Max.X && point.Y <= bounds.Max.Y;
}

Rect intersect(Rect a, Rect b) {
  return {
      {std::max(a.Min.X, b.Min.X), std::max(a.Min.Y, b.Min.Y)},
      {std::min(a.Max.X, b.Max.X), std::min(a.Max.Y, b.Max.Y)},
  };
}

bool has_area(Rect value) {
  return value.Max.X > value.Min.X && value.Max.Y > value.Min.Y;
}

Rect active_clip(Rect bounds) {
  if (!layout.HasClip) {
    return bounds;
  }

  return intersect(bounds, layout.Clip);
}

Rect command_clip(Rect) {
  if (!layout.HasClip) {
    return {};
  }

  return layout.Clip;
}

bool is_visible(Rect bounds) {
  return !layout.HasClip || has_area(intersect(bounds, layout.Clip));
}

float text_width(std::string_view value) {
  const Style &theme = CurrentStyle();
  return std::max(24.0F, static_cast<float>(value.size()) * theme.FontSize *
                             theme.FrameScale * 0.54F);
}

float text_bounds_width(std::string_view value) {
  if (layout.ContentWidth > 0.0F) {
    return layout.ContentWidth;
  }

  return text_width(value);
}

Rect next_rect(float Width, float Height) {
  if (layout.NextItemWidth > 0.0F) {
    Width = layout.NextItemWidth;
    layout.NextItemWidth = 0.0F;
  }

  const Style &theme = CurrentStyle();
  const float Spacing =
      (layout.ItemSpacingY >= 0.0F ? layout.ItemSpacingY : theme.ItemSpacingY) *
      theme.FrameScale;
  Rect bounds{
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

void advance_layout(float Height) {
  const float Spacing = CurrentStyle().ItemSpacingY * CurrentStyle().FrameScale;
  layout.Cursor.Y += Height + Spacing;
  layout.LineBottom = layout.Cursor.Y - Spacing;
}

void set_last_item(std::uint64_t id, bool Hovered, bool Active, bool Focused) {
  LastItem = id;
  last_hovered = Hovered;
  last_active = Active;
  last_focused = Focused;
}

item_result item_behavior(std::uint64_t id, Rect bounds) {
  const InputState &io = Input();
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

bool keyboard_toggle(std::uint64_t id) {
  const InputState &io = Input();
  return focused_item == id && (io.KeyPressed[13] || io.KeyPressed[32]);
}

double normalized_value(double value, double minimum, double maximum) {
  if (maximum <= minimum) {
    return 0.0;
  }

  return std::clamp((value - minimum) / (maximum - minimum), 0.0, 1.0);
}

double value_from_normalized(double value, double minimum, double maximum) {
  return minimum + (maximum - minimum) * std::clamp(value, 0.0, 1.0);
}

bool set_scalar_value(double *target, double value, double minimum,
                      double maximum, bool integral) {
  double clamped = std::clamp(value, minimum, maximum);
  if (integral) {
    clamped = std::round(clamped);
  }

  if (*target == clamped) {
    return false;
  }

  *target = clamped;
  return true;
}

void format_slider_value(char *buffer, std::size_t buffer_size, double value,
                         bool integral, std::string_view suffix) {
  if (integral) {
    if (suffix.empty()) {
      std::snprintf(buffer, buffer_size, "%.0f", value);
    } else {
      std::snprintf(buffer, buffer_size, "%.0f %.*s", value,
                    static_cast<int>(suffix.size()), suffix.data());
    }
  } else {
    if (suffix.empty()) {
      std::snprintf(buffer, buffer_size, "%.2f", value);
    } else {
      std::snprintf(buffer, buffer_size, "%.2f %.*s", value,
                    static_cast<int>(suffix.size()), suffix.data());
    }
  }
}

void add_text(Rect bounds, std::string_view value, Color tint) {
  if (!is_visible(bounds)) {
    return;
  }

  const Style &theme = CurrentStyle();
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

void add_filled_rect(Rect bounds, Color tint) {
  if (!is_visible(bounds)) {
    return;
  }

  mutable_draw().Commands.push_back({
      .Type = DrawCommandType::FilledRect,
      .Bounds = bounds,
      .Clip = command_clip(bounds),
      .Tint = tint,
      .AntiAliasing = CurrentStyle().AntiAliasing,
  });
}

void add_rect(Rect bounds, Color tint, float thickness) {
  if (!is_visible(bounds)) {
    return;
  }

  mutable_draw().Commands.push_back({
      .Type = DrawCommandType::Rect,
      .Bounds = bounds,
      .Clip = command_clip(bounds),
      .Tint = tint,
      .Thickness = thickness,
      .AntiAliasing = CurrentStyle().AntiAliasing,
  });
}

void add_line(Vec2 start, Vec2 end, Color tint, float thickness) {
  const Rect bounds{
      {std::min(start.X, end.X), std::min(start.Y, end.Y)},
      {std::max(start.X, end.X), std::max(start.Y, end.Y)},
  };
  const Rect hit_bounds{
      {bounds.Min.X - thickness, bounds.Min.Y - thickness},
      {bounds.Max.X + thickness, bounds.Max.Y + thickness},
  };

  if (!is_visible(hit_bounds)) {
    return;
  }

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

void add_hline(float x0, float x1, float y, Color tint) {
  add_line({x0, y + 0.5F}, {x1, y + 0.5F}, tint, 1.0F);
}

void add_vline(float x, float y0, float y1, Color tint) {
  add_line({x + 0.5F, y0}, {x + 0.5F, y1}, tint, 1.0F);
}

void add_soft_rect(Rect bounds, float radius, Color tint) {
  add_filled_rect({{bounds.Min.X + radius, bounds.Min.Y},
                   {bounds.Max.X - radius, bounds.Max.Y}},
                  tint);
  add_filled_rect({{bounds.Min.X, bounds.Min.Y + radius},
                   {bounds.Max.X, bounds.Max.Y - radius}},
                  tint);
  add_filled_rect({{bounds.Min.X + 1.0F, bounds.Min.Y + 1.0F},
                   {bounds.Min.X + radius, bounds.Min.Y + radius}},
                  tint);
  add_filled_rect({{bounds.Max.X - radius, bounds.Min.Y + 1.0F},
                   {bounds.Max.X - 1.0F, bounds.Min.Y + radius}},
                  tint);
  add_filled_rect({{bounds.Min.X + 1.0F, bounds.Max.Y - radius},
                   {bounds.Min.X + radius, bounds.Max.Y - 1.0F}},
                  tint);
  add_filled_rect({{bounds.Max.X - radius, bounds.Max.Y - radius},
                   {bounds.Max.X - 1.0F, bounds.Max.Y - 1.0F}},
                  tint);
}

void add_soft_outline(Rect bounds, float radius, Color tint) {
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

bool button_impl(std::string_view label, Color normal, Color hovered_color,
                 Color active_color, Color text_color) {
  ensure_layout();
  const Style &theme = CurrentStyle();
  const float scale = theme.FrameScale;
  const float Height = 32.0F * scale;
  const float minimum_width =
      text_width(label) + theme.FramePaddingX * scale * 2.0F;
  const float preferred_width =
      std::max(theme.ItemWidth * scale, minimum_width);
  const float Width = layout.ContentWidth > 0.0F
                          ? std::min(preferred_width, layout.ContentWidth)
                          : preferred_width;
  const Rect bounds = next_rect(Width, Height);
  const item_result item = item_behavior(CurrentId(label), bounds);
  const Color fill = item.Active    ? active_color
                     : item.Hovered ? hovered_color
                                    : normal;
  const Color top_edge = item.Hovered ? Color{theme.Accent.R, theme.Accent.G,
                                              theme.Accent.B, 0.28F}
                                      : Color{1.0F, 1.0F, 1.0F, 0.025F};

  add_soft_rect(bounds, 4.0F * scale, fill);
  add_hline(bounds.Min.X + 4.0F * scale, bounds.Max.X - 4.0F * scale,
            bounds.Min.Y + 1.0F, top_edge);
  add_soft_outline(bounds, 4.0F * scale,
                   item.Focused   ? theme.Accent
                   : item.Hovered ? Color{theme.Accent.R, theme.Accent.G,
                                          theme.Accent.B, 0.36F}
                                  : theme.ButtonBorder);
  add_text(
      {{bounds.Min.X + theme.FramePaddingX * scale, bounds.Min.Y}, bounds.Max},
      label, text_color);

  return item.Clicked;
}

} // namespace farcal::widget_internal
