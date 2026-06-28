#include <framework/internal/widgets.hpp>

#include <algorithm>
#include <cstdio>

namespace farcal {

bool Button(std::string_view label) {
  const Style &theme = CurrentStyle();
  return widget_internal::button_impl(label, theme.Button, theme.ButtonHovered,
                                      theme.ButtonActive, theme.Text);
}

bool PrimaryButton(std::string_view label) {
  const Style &theme = CurrentStyle();
  return widget_internal::button_impl(
      label, theme.ButtonPrimary, theme.ButtonPrimaryHovered,
      theme.ButtonPrimaryActive, {1.0F, 1.0F, 1.0F, 1.0F});
}

bool Checkbox(std::string_view label, bool *value) {
  widget_internal::ensure_layout();
  if (value == nullptr) {
    return false;
  }

  const Style &theme = CurrentStyle();
  const float scale = theme.FrameScale;
  const float Height = 24.0F * scale;
  const float box_size = 16.0F * scale;
  const float gap = 10.0F * scale;
  const float label_width = widget_internal::text_width(label);
  const float Width = widget_internal::layout.ContentWidth > 0.0F
                          ? widget_internal::layout.ContentWidth
                          : box_size + gap + label_width;
  const Rect bounds = widget_internal::next_rect(Width, Height);
  const std::uint64_t id = CurrentId(label);
  const widget_internal::item_result item =
      widget_internal::item_behavior(id, bounds);
  const bool changed = item.Clicked || widget_internal::keyboard_toggle(id);

  if (changed) {
    *value = !*value;
  }

  const float box_y = bounds.Min.Y + (Height - box_size) * 0.5F;
  const Rect box{{bounds.Min.X, box_y},
                 {bounds.Min.X + box_size, box_y + box_size}};
  const Color unchecked_fill = item.Active    ? theme.ButtonActive
                               : item.Hovered ? theme.ButtonHovered
                                              : theme.Button;
  const Color checked_fill = item.Active    ? theme.ButtonPrimaryActive
                             : item.Hovered ? theme.ButtonPrimaryHovered
                                            : theme.ButtonPrimary;
  const Color fill = *value ? checked_fill : unchecked_fill;
  const Color border =
      item.Focused ? theme.Accent
      : *value ? Color{theme.Accent.R, theme.Accent.G, theme.Accent.B, 0.82F}
      : item.Hovered
          ? Color{theme.Accent.R, theme.Accent.G, theme.Accent.B, 0.34F}
          : theme.ButtonBorder;

  widget_internal::add_filled_rect({{box.Min.X + 1.0F, box.Min.Y + 1.0F},
                                    {box.Max.X - 1.0F, box.Max.Y - 1.0F}},
                                   fill);
  widget_internal::add_hline(box.Min.X + 1.0F, box.Max.X - 1.0F,
                             box.Min.Y + 1.0F,
                             item.Hovered ? Color{1.0F, 1.0F, 1.0F, 0.12F}
                                          : Color{1.0F, 1.0F, 1.0F, 0.05F});
  widget_internal::add_soft_outline(box, 0.0F, border);

  if (*value) {
    const Color mark{1.0F, 1.0F, 1.0F, 0.96F};
    widget_internal::add_line(
        {box.Min.X + 4.4F * scale, box.Min.Y + 8.3F * scale},
        {box.Min.X + 7.0F * scale, box.Min.Y + 10.8F * scale}, mark,
        1.35F * scale);
    widget_internal::add_line(
        {box.Min.X + 7.0F * scale, box.Min.Y + 10.8F * scale},
        {box.Min.X + 12.0F * scale, box.Min.Y + 5.7F * scale}, mark,
        1.35F * scale);
  }

  const Color text_color = *value ? theme.Text : theme.TextSecondary;
  widget_internal::add_text({{box.Max.X + gap, bounds.Min.Y}, bounds.Max},
                            label, text_color);

  return changed;
}

namespace widget_internal {

bool SliderScalar(std::string_view label, double *value, double minimum,
                  double maximum, bool integral, std::string_view suffix) {
  ensure_layout();
  if (value == nullptr) {
    return false;
  }

  if (maximum < minimum) {
    std::swap(minimum, maximum);
  }

  const Style &theme = CurrentStyle();
  const InputState &io = Input();
  const float scale = theme.FrameScale;
  const float Height = 42.0F * scale;
  const float Width = layout.ContentWidth > 0.0F ? layout.ContentWidth
                                                 : theme.ItemWidth * scale;
  const Rect bounds = next_rect(Width, Height);
  const std::uint64_t id = CurrentId(label);
  const item_result item = item_behavior(id, bounds);
  bool changed = false;

  const float track_y = bounds.Min.Y + 30.0F * scale;
  const float track_start = bounds.Min.X;
  const float track_end = bounds.Max.X;
  const float track_width = std::max(1.0F, track_end - track_start);

  if ((item.Active && io.MouseDown[0]) || item.Clicked) {
    const double normalized =
        static_cast<double>((io.MousePosition.X - track_start) / track_width);
    changed = set_scalar_value(
        value, value_from_normalized(normalized, minimum, maximum), minimum,
        maximum, integral);
  }

  if (item.Focused) {
    const double step =
        integral ? std::max(1.0, std::round((maximum - minimum) / 100.0))
                 : (maximum - minimum) / 100.0;
    if (io.KeyPressed[37]) {
      changed =
          set_scalar_value(value, *value - step, minimum, maximum, integral) ||
          changed;
    }
    if (io.KeyPressed[39]) {
      changed =
          set_scalar_value(value, *value + step, minimum, maximum, integral) ||
          changed;
    }
  }

  const float normalized =
      static_cast<float>(normalized_value(*value, minimum, maximum));
  const float thumb_x = track_start + track_width * normalized;
  const Color track_color =
      item.Hovered || item.Active ? theme.ButtonHovered : theme.Button;
  const Color progress_color = item.Active    ? theme.ButtonPrimaryActive
                               : item.Hovered ? theme.ButtonPrimaryHovered
                                              : theme.ButtonPrimary;
  const Color thumb_fill = item.Active ? theme.Selection : theme.WindowPanel;
  const Color thumb_border =
      item.Focused ? theme.Accent
      : item.Hovered || item.Active
          ? Color{theme.Accent.R, theme.Accent.G, theme.Accent.B, 0.70F}
          : theme.ButtonBorder;

  char value_text[32]{};
  format_slider_value(value_text, sizeof(value_text), *value, integral, suffix);
  const float value_width = text_width(value_text);

  add_text({bounds.Min,
            {bounds.Max.X - value_width - 10.0F * scale,
             bounds.Min.Y + 20.0F * scale}},
           label, theme.TextSecondary);
  add_text({{bounds.Max.X - value_width, bounds.Min.Y},
            {bounds.Max.X, bounds.Min.Y + 20.0F * scale}},
           value_text, item.Active ? theme.Text : theme.TextSecondary);

  add_line({track_start, track_y}, {track_end, track_y}, track_color,
           4.0F * scale);
  add_line({track_start, track_y}, {thumb_x, track_y}, progress_color,
           4.0F * scale);

  const Rect thumb{{thumb_x - 4.0F * scale, track_y - 8.0F * scale},
                   {thumb_x + 4.0F * scale, track_y + 8.0F * scale}};
  add_filled_rect({{thumb.Min.X + 1.0F, thumb.Min.Y + 1.0F},
                   {thumb.Max.X - 1.0F, thumb.Max.Y - 1.0F}},
                  thumb_fill);
  add_soft_outline(thumb, 0.0F, thumb_border);

  return changed;
}

} // namespace widget_internal

bool SliderFloat(std::string_view label, float *value, float minimum,
                 float maximum, std::string_view suffix) {
  if (value == nullptr) {
    return false;
  }

  double scalar = static_cast<double>(*value);
  const bool changed = widget_internal::SliderScalar(
      label, &scalar, static_cast<double>(minimum),
      static_cast<double>(maximum), false, suffix);
  if (changed) {
    *value = static_cast<float>(scalar);
  }

  return changed;
}

} // namespace farcal
