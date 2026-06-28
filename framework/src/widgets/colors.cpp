#include <framework/internal/widgets.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>

namespace farcal {
namespace {

std::uint64_t open_picker{};

struct picker_window_state {
  Vec2 Position{};
  Vec2 DragOffset{};
  bool Initialized{};
  bool Dragging{};
};

std::unordered_map<std::uint64_t, picker_window_state> picker_windows{};

int to_byte(float value) {
  return std::clamp(static_cast<int>(std::lround(value * 255.0F)), 0, 255);
}

float to_float(int value) {
  return static_cast<float>(std::clamp(value, 0, 255)) / 255.0F;
}

Color hsv_to_rgb(float hue, float saturation, float value, float alpha = 1.0F) {
  hue = std::fmod(std::max(0.0F, hue), 1.0F);
  saturation = std::clamp(saturation, 0.0F, 1.0F);
  value = std::clamp(value, 0.0F, 1.0F);

  const float scaled = hue * 6.0F;
  const int sector = static_cast<int>(std::floor(scaled));
  const float fraction = scaled - static_cast<float>(sector);
  const float p = value * (1.0F - saturation);
  const float q = value * (1.0F - saturation * fraction);
  const float t = value * (1.0F - saturation * (1.0F - fraction));

  switch (sector % 6) {
  case 0:
    return {value, t, p, alpha};
  case 1:
    return {q, value, p, alpha};
  case 2:
    return {p, value, t, alpha};
  case 3:
    return {p, q, value, alpha};
  case 4:
    return {t, p, value, alpha};
  default:
    return {value, p, q, alpha};
  }
}

void rgb_to_hsv(Color color, float &hue, float &saturation, float &value) {
  const float maximum = std::max(color.R, std::max(color.G, color.B));
  const float minimum = std::min(color.R, std::min(color.G, color.B));
  const float chroma = maximum - minimum;

  value = maximum;
  saturation = maximum <= 0.0F ? 0.0F : chroma / maximum;

  if (chroma <= 0.0F) {
    hue = 0.0F;
  } else if (maximum == color.R) {
    hue = std::fmod((color.G - color.B) / chroma, 6.0F) / 6.0F;
  } else if (maximum == color.G) {
    hue = ((color.B - color.R) / chroma + 2.0F) / 6.0F;
  } else {
    hue = ((color.R - color.G) / chroma + 4.0F) / 6.0F;
  }

  if (hue < 0.0F) {
    hue += 1.0F;
  }
}

Color channel_tint(int channel, const Color &color) {
  switch (channel) {
  case 0:
    return Color::Rgba(255, 84, 92, to_byte(color.A));
  case 1:
    return Color::Rgba(82, 210, 115, to_byte(color.A));
  case 2:
    return Color::Rgba(92, 139, 255, to_byte(color.A));
  case 3:
    return Color::Rgba(184, 192, 204, to_byte(color.A));
  default:
    return color;
  }
}

void menu_filled_rect(Rect bounds, Color tint) {
  ForegroundRenderer().Commands.push_back({
      .Type = DrawCommandType::FilledRect,
      .Bounds = bounds,
      .Clip = {},
      .Tint = tint,
      .AntiAliasing = CurrentStyle().AntiAliasing,
  });
}

void menu_line(Vec2 start, Vec2 end, Color tint, float thickness) {
  const Rect bounds{
      {std::min(start.X, end.X), std::min(start.Y, end.Y)},
      {std::max(start.X, end.X), std::max(start.Y, end.Y)},
  };
  const Rect hit_bounds{
      {bounds.Min.X - thickness, bounds.Min.Y - thickness},
      {bounds.Max.X + thickness, bounds.Max.Y + thickness},
  };

  ForegroundRenderer().Commands.push_back({
      .Type = DrawCommandType::Line,
      .Clip = {},
      .Start = start,
      .End = end,
      .Tint = tint,
      .Thickness = thickness,
      .AntiAliasing = CurrentStyle().AntiAliasing,
  });
}

void menu_hline(float x0, float x1, float y, Color tint) {
  menu_line({x0, y + 0.5F}, {x1, y + 0.5F}, tint, 1.0F);
}

void menu_rect(Rect bounds, Color tint, float thickness = 1.0F) {
  menu_line({bounds.Min.X, bounds.Min.Y}, {bounds.Max.X, bounds.Min.Y}, tint,
            thickness);
  menu_line({bounds.Max.X, bounds.Min.Y}, {bounds.Max.X, bounds.Max.Y}, tint,
            thickness);
  menu_line({bounds.Max.X, bounds.Max.Y}, {bounds.Min.X, bounds.Max.Y}, tint,
            thickness);
  menu_line({bounds.Min.X, bounds.Max.Y}, {bounds.Min.X, bounds.Min.Y}, tint,
            thickness);
}

void menu_soft_rect(Rect bounds, float radius, Color tint) {
  menu_filled_rect({{bounds.Min.X + radius, bounds.Min.Y},
                    {bounds.Max.X - radius, bounds.Max.Y}},
                   tint);
  menu_filled_rect({{bounds.Min.X, bounds.Min.Y + radius},
                    {bounds.Max.X, bounds.Max.Y - radius}},
                   tint);
  menu_filled_rect({{bounds.Min.X + 1.0F, bounds.Min.Y + 1.0F},
                    {bounds.Min.X + radius, bounds.Min.Y + radius}},
                   tint);
  menu_filled_rect({{bounds.Max.X - radius, bounds.Min.Y + 1.0F},
                    {bounds.Max.X - 1.0F, bounds.Min.Y + radius}},
                   tint);
  menu_filled_rect({{bounds.Min.X + 1.0F, bounds.Max.Y - radius},
                    {bounds.Min.X + radius, bounds.Max.Y - 1.0F}},
                   tint);
  menu_filled_rect({{bounds.Max.X - radius, bounds.Max.Y - radius},
                    {bounds.Max.X - 1.0F, bounds.Max.Y - 1.0F}},
                   tint);
}

void menu_soft_outline(Rect bounds, float, Color tint) {
  const float left = bounds.Min.X + 0.5F;
  const float top = bounds.Min.Y + 0.5F;
  const float right = bounds.Max.X - 0.5F;
  const float bottom = bounds.Max.Y - 0.5F;

  menu_line({left, top}, {right, top}, tint, 1.0F);
  menu_line({right, top}, {right, bottom}, tint, 1.0F);
  menu_line({right, bottom}, {left, bottom}, tint, 1.0F);
  menu_line({left, bottom}, {left, top}, tint, 1.0F);
}

void menu_text(Rect bounds, std::string_view value, Color tint) {
  const Style &theme = CurrentStyle();
  ForegroundRenderer().Commands.push_back({
      .Type = DrawCommandType::Text,
      .Bounds = bounds,
      .Clip = {},
      .Tint = tint,
      .FontSize = theme.FontSize * theme.FrameScale,
      .Text = std::string(value),
      .FontFamily = theme.FontFamily,
  });
}

widget_internal::item_result menu_behavior(std::uint64_t id, Rect bounds) {
  const InputState &io = Input();
  if (!io.MouseDown[0] && !io.MousePressed[0]) {
    widget_internal::active_item = 0;
  }

  const bool hovered = widget_internal::contains(bounds, io.MousePosition);
  if (hovered && io.MousePressed[0]) {
    widget_internal::active_item = id;
    widget_internal::focused_item = id;
  }

  const bool active = widget_internal::active_item == id;
  const bool focused = widget_internal::focused_item == id;
  const bool clicked = hovered && io.MousePressed[0];
  widget_internal::set_last_item(id, hovered, active, focused);

  return {
      .Hovered = hovered,
      .Active = active,
      .Focused = focused,
      .Clicked = clicked,
  };
}

bool channel_box(std::string_view label, int *value, int channel, Rect bounds,
                 const Color &color, bool menu = false) {
  const Style &theme = CurrentStyle();
  const InputState &io = Input();
  const float scale = theme.FrameScale;
  const std::uint64_t id = CurrentId(label);
  const widget_internal::item_result item =
      menu ? menu_behavior(id, bounds)
           : widget_internal::item_behavior(id, bounds);
  bool changed = false;

  if ((item.Active && io.MouseDown[0]) || item.Clicked) {
    const int delta =
        item.Clicked ? 0 : static_cast<int>(std::lround(io.MouseDelta.X));
    if (delta != 0) {
      const int next = std::clamp(*value + delta, 0, 255);
      changed = next != *value;
      *value = next;
    }
  }

  if (item.Focused) {
    if (io.KeyPressed[37] || io.KeyRepeated[37]) {
      const int next = std::max(0, *value - 1);
      changed = next != *value || changed;
      *value = next;
    }
    if (io.KeyPressed[39] || io.KeyRepeated[39]) {
      const int next = std::min(255, *value + 1);
      changed = next != *value || changed;
      *value = next;
    }
  }

  const Color base = item.Active    ? theme.ButtonActive
                     : item.Hovered ? theme.ButtonHovered
                                    : theme.Button;
  const Color accent = channel_tint(channel, color);
  char text[16]{};
  std::snprintf(text, sizeof(text), "%c %d", static_cast<char>(label[0]),
                *value);

  if (menu) {
    menu_filled_rect(bounds, base);
    menu_filled_rect({{bounds.Min.X, bounds.Min.Y},
                      {bounds.Min.X + 2.0F * scale, bounds.Max.Y}},
                     accent);
    menu_hline(bounds.Min.X + 1.0F, bounds.Max.X - 1.0F, bounds.Min.Y + 1.0F,
               Color::RgbaF(1.0F, 1.0F, 1.0F, item.Hovered ? 0.12F : 0.04F));
    menu_soft_outline(bounds, 0.0F,
                      item.Focused ? Color::RgbaF(theme.WindowBorder.R,
                                                  theme.WindowBorder.G,
                                                  theme.WindowBorder.B, 0.98F)
                                   : theme.ButtonBorder);
    menu_text({{bounds.Min.X + 7.0F * scale, bounds.Min.Y},
               {bounds.Max.X - 4.0F * scale, bounds.Max.Y}},
              text, item.Active ? theme.Text : theme.TextSecondary);
  } else {
    widget_internal::add_filled_rect(bounds, base);
    widget_internal::add_filled_rect(
        {{bounds.Min.X, bounds.Min.Y},
         {bounds.Min.X + 2.0F * scale, bounds.Max.Y}},
        accent);
    widget_internal::add_hline(
        bounds.Min.X + 1.0F, bounds.Max.X - 1.0F, bounds.Min.Y + 1.0F,
        Color::RgbaF(1.0F, 1.0F, 1.0F, item.Hovered ? 0.12F : 0.04F));
    widget_internal::add_soft_outline(
        bounds, 0.0F,
        item.Focused ? Color::RgbaF(theme.WindowBorder.R, theme.WindowBorder.G,
                                    theme.WindowBorder.B, 0.98F)
                     : theme.ButtonBorder);
    widget_internal::add_text({{bounds.Min.X + 7.0F * scale, bounds.Min.Y},
                               {bounds.Max.X - 4.0F * scale, bounds.Max.Y}},
                              text,
                              item.Active ? theme.Text : theme.TextSecondary);
  }

  return changed;
}

void draw_checker(Rect bounds, float cell, Color a, Color b,
                  bool menu = false) {
  int row = 0;
  for (float y = bounds.Min.Y; y < bounds.Max.Y; y += cell, ++row) {
    int column = 0;
    for (float x = bounds.Min.X; x < bounds.Max.X; x += cell, ++column) {
      const Rect tile{
          {x, y},
          {std::min(x + cell, bounds.Max.X), std::min(y + cell, bounds.Max.Y)},
      };
      if (menu) {
        menu_filled_rect(tile, ((row + column) % 2) == 0 ? a : b);
      } else {
        widget_internal::add_filled_rect(tile,
                                         ((row + column) % 2) == 0 ? a : b);
      }
    }
  }
}

void draw_sv_square(Rect bounds, float hue, bool menu = false) {
  constexpr int steps = 32;
  const float width = bounds.Max.X - bounds.Min.X;
  const float height = bounds.Max.Y - bounds.Min.Y;

  for (int y = 0; y < steps; ++y) {
    for (int x = 0; x < steps; ++x) {
      const float saturation =
          static_cast<float>(x) / static_cast<float>(steps - 1);
      const float value =
          1.0F - static_cast<float>(y) / static_cast<float>(steps - 1);
      const Rect cell{
          {bounds.Min.X +
               width * static_cast<float>(x) / static_cast<float>(steps),
           bounds.Min.Y +
               height * static_cast<float>(y) / static_cast<float>(steps)},
          {bounds.Min.X +
               width * static_cast<float>(x + 1) / static_cast<float>(steps),
           bounds.Min.Y +
               height * static_cast<float>(y + 1) / static_cast<float>(steps)},
      };
      if (menu) {
        menu_filled_rect(cell, hsv_to_rgb(hue, saturation, value));
      } else {
        widget_internal::add_filled_rect(cell,
                                         hsv_to_rgb(hue, saturation, value));
      }
    }
  }
}

void draw_hue_bar(Rect bounds, bool menu = false) {
  constexpr int steps = 48;
  const float height = bounds.Max.Y - bounds.Min.Y;

  for (int index = 0; index < steps; ++index) {
    const float hue0 = static_cast<float>(index) / static_cast<float>(steps);
    const Rect cell{
        {bounds.Min.X, bounds.Min.Y + height * static_cast<float>(index) /
                                          static_cast<float>(steps)},
        {bounds.Max.X, bounds.Min.Y + height * static_cast<float>(index + 1) /
                                          static_cast<float>(steps)},
    };
    if (menu) {
      menu_filled_rect(cell, hsv_to_rgb(hue0, 1.0F, 1.0F));
    } else {
      widget_internal::add_filled_rect(cell, hsv_to_rgb(hue0, 1.0F, 1.0F));
    }
  }
}

void draw_alpha_bar(Rect bounds, Color color, bool menu = false) {
  constexpr int steps = 48;
  const float height = bounds.Max.Y - bounds.Min.Y;

  draw_checker(bounds, 6.0F * CurrentStyle().FrameScale, Color::Rgb(74, 82, 92),
               Color::Rgb(118, 128, 142), menu);

  for (int index = 0; index < steps; ++index) {
    const float alpha =
        1.0F - static_cast<float>(index) / static_cast<float>(steps - 1);
    const Rect cell{
        {bounds.Min.X, bounds.Min.Y + height * static_cast<float>(index) /
                                          static_cast<float>(steps)},
        {bounds.Max.X, bounds.Min.Y + height * static_cast<float>(index + 1) /
                                          static_cast<float>(steps)},
    };
    if (menu) {
      menu_filled_rect(cell, {color.R, color.G, color.B, alpha});
    } else {
      widget_internal::add_filled_rect(cell,
                                       {color.R, color.G, color.B, alpha});
    }
  }
}

bool picker_widget(std::string_view label, Color *color, Rect row_bounds) {
  const Style &theme = CurrentStyle();
  const InputState &io = Input();
  const float scale = theme.FrameScale;
  const std::uint64_t picker_id = CurrentId(label);
  const float width = 360.0F * scale;
  const float padding = 8.0F * scale;
  const float title_height = 30.0F * scale;
  const float square_size = 174.0F * scale;
  const float bar_width = 16.0F * scale;
  const float preview_width = 52.0F * scale;
  const float field_height = 28.0F * scale;
  const float popup_height =
      title_height + padding * 3.0F + square_size + field_height;
  picker_window_state &state = picker_windows[picker_id];

  if (!state.Initialized) {
    state.Position = {row_bounds.Min.X, row_bounds.Max.Y + 8.0F * scale};
    state.Initialized = true;
  }

  const Rect popup{
      state.Position,
      {state.Position.X + width, state.Position.Y + popup_height},
  };
  const Rect title_rect{
      popup.Min,
      {popup.Max.X, popup.Min.Y + title_height},
  };
  const Rect sv_rect{
      {popup.Min.X + padding, title_rect.Max.Y + padding},
      {popup.Min.X + padding + square_size,
       title_rect.Max.Y + padding + square_size},
  };
  const Rect hue_rect{
      {sv_rect.Max.X + 6.0F * scale, sv_rect.Min.Y},
      {sv_rect.Max.X + 6.0F * scale + bar_width, sv_rect.Max.Y},
  };
  const Rect alpha_rect{
      {hue_rect.Max.X + 6.0F * scale, sv_rect.Min.Y},
      {hue_rect.Max.X + 6.0F * scale + bar_width, sv_rect.Max.Y},
  };
  const Rect preview_rect{
      {alpha_rect.Max.X + 8.0F * scale, sv_rect.Min.Y},
      {alpha_rect.Max.X + 8.0F * scale + preview_width,
       sv_rect.Min.Y + preview_width},
  };

  const std::uint64_t title_id = CurrentId("picker-drag");
  const widget_internal::item_result title_item =
      menu_behavior(title_id, title_rect);
  if (title_item.Clicked) {
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
  }

  if (io.MousePressed[0] &&
      !widget_internal::contains(popup, io.MousePosition) &&
      !widget_internal::contains(row_bounds, io.MousePosition)) {
    open_picker = 0;
  }

  float hue{};
  float saturation{};
  float value{};
  rgb_to_hsv(*color, hue, saturation, value);
  bool changed = false;

  menu_soft_rect(popup, 5.0F * scale,
                 Color::RgbaF(theme.WindowPanel.R, theme.WindowPanel.G,
                              theme.WindowPanel.B, 0.98F));
  menu_hline(popup.Min.X + 5.0F * scale, popup.Max.X - 5.0F * scale,
             popup.Min.Y + 1.0F, Color::RgbaF(1.0F, 1.0F, 1.0F, 0.06F));
  menu_soft_outline(popup, 5.0F * scale,
                    Color::RgbaF(theme.WindowBorder.R, theme.WindowBorder.G,
                                 theme.WindowBorder.B, 0.92F));
  menu_soft_rect(title_rect, 5.0F * scale,
                 title_item.Active ? theme.WindowTitleActive
                 : title_item.Hovered
                     ? Color::RgbaF(theme.WindowTitleActive.R,
                                    theme.WindowTitleActive.G,
                                    theme.WindowTitleActive.B, 0.82F)
                     : theme.WindowTitle);
  menu_filled_rect(
      {{title_rect.Min.X, title_rect.Max.Y - 5.0F * scale}, title_rect.Max},
      title_item.Active ? theme.WindowTitleActive
      : title_item.Hovered
          ? Color::RgbaF(theme.WindowTitleActive.R, theme.WindowTitleActive.G,
                         theme.WindowTitleActive.B, 0.82F)
          : theme.WindowTitle);
  menu_hline(title_rect.Min.X, title_rect.Max.X, title_rect.Max.Y,
             Color::RgbaF(theme.WindowBorder.R, theme.WindowBorder.G,
                          theme.WindowBorder.B, 0.72F));
  menu_text({{title_rect.Min.X + 10.0F * scale, title_rect.Min.Y},
             {title_rect.Max.X - 10.0F * scale, title_rect.Max.Y}},
            label, theme.Text);

  draw_sv_square(sv_rect, hue, true);
  menu_soft_outline(sv_rect, 0.0F,
                    Color::RgbaF(theme.WindowBorder.R, theme.WindowBorder.G,
                                 theme.WindowBorder.B, 0.92F));

  PushId(label);
  const widget_internal::item_result sv_item =
      menu_behavior(CurrentId("sv"), sv_rect);
  if ((sv_item.Active && io.MouseDown[0]) || sv_item.Clicked) {
    saturation = std::clamp((io.MousePosition.X - sv_rect.Min.X) /
                                (sv_rect.Max.X - sv_rect.Min.X),
                            0.0F, 1.0F);
    value = std::clamp(1.0F - (io.MousePosition.Y - sv_rect.Min.Y) /
                                  (sv_rect.Max.Y - sv_rect.Min.Y),
                       0.0F, 1.0F);
    changed = true;
  }

  const float marker_x =
      sv_rect.Min.X + saturation * (sv_rect.Max.X - sv_rect.Min.X);
  const float marker_y =
      sv_rect.Min.Y + (1.0F - value) * (sv_rect.Max.Y - sv_rect.Min.Y);
  menu_rect({{marker_x - 4.0F * scale, marker_y - 4.0F * scale},
             {marker_x + 4.0F * scale, marker_y + 4.0F * scale}},
            Color::Rgb(255, 255, 255));
  menu_rect({{marker_x - 5.0F * scale, marker_y - 5.0F * scale},
             {marker_x + 5.0F * scale, marker_y + 5.0F * scale}},
            Color::Rgba(0, 0, 0, 150));

  draw_hue_bar(hue_rect, true);
  menu_soft_outline(hue_rect, 0.0F,
                    Color::RgbaF(theme.WindowBorder.R, theme.WindowBorder.G,
                                 theme.WindowBorder.B, 0.92F));
  const widget_internal::item_result hue_item =
      menu_behavior(CurrentId("hue"), hue_rect);
  if ((hue_item.Active && io.MouseDown[0]) || hue_item.Clicked) {
    hue = std::clamp((io.MousePosition.Y - hue_rect.Min.Y) /
                         (hue_rect.Max.Y - hue_rect.Min.Y),
                     0.0F, 1.0F);
    changed = true;
  }
  const float hue_y = hue_rect.Min.Y + hue * (hue_rect.Max.Y - hue_rect.Min.Y);
  menu_hline(hue_rect.Min.X - 3.0F * scale, hue_rect.Max.X + 3.0F * scale,
             hue_y, Color::Rgb(255, 255, 255));

  draw_alpha_bar(alpha_rect, hsv_to_rgb(hue, saturation, value, 1.0F), true);
  menu_soft_outline(alpha_rect, 0.0F,
                    Color::RgbaF(theme.WindowBorder.R, theme.WindowBorder.G,
                                 theme.WindowBorder.B, 0.92F));
  const widget_internal::item_result alpha_item =
      menu_behavior(CurrentId("alpha"), alpha_rect);
  if ((alpha_item.Active && io.MouseDown[0]) || alpha_item.Clicked) {
    color->A = std::clamp(1.0F - (io.MousePosition.Y - alpha_rect.Min.Y) /
                                     (alpha_rect.Max.Y - alpha_rect.Min.Y),
                          0.0F, 1.0F);
    changed = true;
  }
  const float alpha_y =
      alpha_rect.Min.Y +
      (1.0F - color->A) * (alpha_rect.Max.Y - alpha_rect.Min.Y);
  menu_hline(alpha_rect.Min.X - 3.0F * scale, alpha_rect.Max.X + 3.0F * scale,
             alpha_y, Color::Rgb(255, 255, 255));

  const Color edited = hsv_to_rgb(hue, saturation, value, color->A);
  draw_checker(preview_rect, 7.0F * scale, Color::Rgb(74, 82, 92),
               Color::Rgb(118, 128, 142), true);
  menu_filled_rect({{preview_rect.Min.X + 1.0F, preview_rect.Min.Y + 1.0F},
                    {preview_rect.Max.X - 1.0F, preview_rect.Max.Y - 1.0F}},
                   edited);
  menu_soft_outline(preview_rect, 3.0F * scale,
                    Color::RgbaF(theme.WindowBorder.R, theme.WindowBorder.G,
                                 theme.WindowBorder.B, 0.92F));
  menu_text({{preview_rect.Min.X, preview_rect.Max.Y + 5.0F * scale},
             {popup.Max.X - padding, preview_rect.Max.Y + 26.0F * scale}},
            "Current", theme.TextSecondary);

  int channels[4]{to_byte(edited.R), to_byte(edited.G), to_byte(edited.B),
                  to_byte(edited.A)};
  const char *names[4]{"R", "G", "B", "A"};
  const float field_y = sv_rect.Max.Y + padding;
  const float field_gap = 3.0F * scale;
  const float field_width =
      ((popup.Max.X - popup.Min.X) - padding * 2.0F - field_gap * 3.0F) / 4.0F;

  for (int index = 0; index < 4; ++index) {
    PushId(names[index]);
    const float x = popup.Min.X + padding +
                    static_cast<float>(index) * (field_width + field_gap);
    const Rect field{{x, field_y}, {x + field_width, field_y + field_height}};
    changed = channel_box(names[index], &channels[index], index, field, edited,
                          true) ||
              changed;
    PopId();
  }
  PopId();

  if (changed) {
    color->R = to_float(channels[0]);
    color->G = to_float(channels[1]);
    color->B = to_float(channels[2]);
    color->A = to_float(channels[3]);

    if (sv_item.Active || sv_item.Clicked || hue_item.Active ||
        hue_item.Clicked) {
      const Color from_hsv = hsv_to_rgb(hue, saturation, value, color->A);
      color->R = from_hsv.R;
      color->G = from_hsv.G;
      color->B = from_hsv.B;
    }
  }

  return changed;
}

} // namespace

bool ColorEdit(std::string_view label, Color *value) {
  widget_internal::ensure_layout();
  if (value == nullptr) {
    return false;
  }

  const Style &theme = CurrentStyle();
  const float scale = theme.FrameScale;
  const float width = widget_internal::layout.ContentWidth > 0.0F
                          ? widget_internal::layout.ContentWidth
                          : theme.ItemWidth * scale;
  const float height = 32.0F * scale;
  const Rect bounds = widget_internal::next_rect(width, height);
  const std::uint64_t id = CurrentId(label);
  const bool hovered = widget_internal::contains(
      widget_internal::active_clip(bounds), Input().MousePosition);

  const int red = to_byte(value->R);
  const int green = to_byte(value->G);
  const int blue = to_byte(value->B);

  const float swatch_size = 24.0F * scale;
  const float gap = 5.0F * scale;
  const float swatch_x = bounds.Max.X - swatch_size;
  const Rect swatch{
      {swatch_x, bounds.Min.Y + (height - swatch_size) * 0.5F},
      {bounds.Max.X, bounds.Min.Y + (height + swatch_size) * 0.5F},
  };
  char value_text[64]{};
  std::snprintf(value_text, sizeof(value_text), "%d, %d, %d, %d", red, green,
                blue, to_byte(value->A));
  const float value_width = widget_internal::text_width(value_text);
  bool changed = false;

  widget_internal::add_text(
      {bounds.Min,
       {std::max(bounds.Min.X, swatch.Min.X - value_width - 10.0F * scale),
        bounds.Max.Y}},
      label, hovered ? theme.Text : theme.TextSecondary);
  widget_internal::add_text({{swatch.Min.X - value_width - gap, bounds.Min.Y},
                             {swatch.Min.X - gap, bounds.Max.Y}},
                            value_text, theme.TextSecondary);

  PushId(label);
  const std::uint64_t swatch_id = CurrentId("swatch");
  const widget_internal::item_result swatch_item =
      widget_internal::item_behavior(swatch_id, swatch);
  if (swatch_item.Clicked || widget_internal::keyboard_toggle(swatch_id)) {
    open_picker = open_picker == id ? 0 : id;
  }
  PopId();

  draw_checker(swatch, 6.0F * scale, Color::Rgb(74, 82, 92),
               Color::Rgb(118, 128, 142));
  widget_internal::add_filled_rect({{swatch.Min.X + 1.0F, swatch.Min.Y + 1.0F},
                                    {swatch.Max.X - 1.0F, swatch.Max.Y - 1.0F}},
                                   *value);
  widget_internal::add_hline(
      swatch.Min.X + 1.0F, swatch.Max.X - 1.0F, swatch.Min.Y + 1.0F,
      Color::RgbaF(1.0F, 1.0F, 1.0F, swatch_item.Hovered ? 0.22F : 0.14F));
  widget_internal::add_soft_outline(
      swatch, 3.0F * scale,
      swatch_item.Focused
          ? Color::RgbaF(theme.WindowBorder.R, theme.WindowBorder.G,
                         theme.WindowBorder.B, 0.98F)
          : Color::RgbaF(theme.WindowBorder.R, theme.WindowBorder.G,
                         theme.WindowBorder.B, 0.86F));

  if (open_picker == id) {
    changed = picker_widget(label, value, bounds) || changed;
  }

  widget_internal::set_last_item(id, hovered, false, swatch_item.Focused);
  return changed;
}

} // namespace farcal
