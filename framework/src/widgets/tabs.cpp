#include <framework/internal/widgets.hpp>

#include <algorithm>
#include <string>

namespace farcal {

bool BeginTabs(std::string_view label, int *selected, const char *const *tabs,
               int tab_count) {
  widget_internal::ensure_layout();
  if (selected == nullptr || tabs == nullptr || tab_count <= 0) {
    return false;
  }

  *selected = std::clamp(*selected, 0, tab_count - 1);

  const Style &theme = CurrentStyle();
  const InputState &io = Input();
  const float scale = theme.FrameScale;
  const float height = 38.0F * scale;
  const float width = widget_internal::layout.ContentWidth > 0.0F
                          ? widget_internal::layout.ContentWidth
                          : theme.ItemWidth * scale;
  const Rect bounds = widget_internal::next_rect(width, height);
  const float padding = 4.0F * scale;
  const float gap = 3.0F * scale;
  const Rect track{{bounds.Min.X, bounds.Min.Y},
                   {bounds.Max.X, bounds.Max.Y - 2.0F * scale}};
  const float available_width = (track.Max.X - track.Min.X) - padding * 2.0F -
                                gap * static_cast<float>(tab_count - 1);
  const float tab_width = available_width / static_cast<float>(tab_count);
  bool hovered = false;
  bool active = false;
  bool focused = false;

  widget_internal::add_soft_rect(
      track, 5.0F * scale,
      Color::RgbaF(theme.Button.R, theme.Button.G, theme.Button.B, 0.52F));
  widget_internal::add_hline(track.Min.X + 5.0F * scale,
                             track.Max.X - 5.0F * scale, track.Min.Y + 1.0F,
                             Color::RgbaF(1.0F, 1.0F, 1.0F, 0.045F));
  widget_internal::add_soft_outline(track, 5.0F * scale,
                                    Color::RgbaF(theme.WindowBorder.R,
                                                 theme.WindowBorder.G,
                                                 theme.WindowBorder.B, 0.62F));

  for (int index = 0; index < tab_count; ++index) {
    const float x0 =
        track.Min.X + padding + static_cast<float>(index) * (tab_width + gap);
    const float x1 =
        index == tab_count - 1 ? track.Max.X - padding : x0 + tab_width;
    const Rect tab_bounds{{x0, track.Min.Y + padding},
                          {x1, track.Max.Y - padding}};
    const std::string id_label = std::string(label) + "##" + tabs[index];
    const std::uint64_t id = CurrentId(id_label);
    const Rect hit_bounds{{x0 - gap * 0.5F, track.Min.Y},
                          {x1 + gap * 0.5F, track.Max.Y}};
    const widget_internal::item_result item =
        widget_internal::item_behavior(id, hit_bounds);
    const bool selected_tab = *selected == index;

    hovered = hovered || item.Hovered;
    active = active || item.Active;
    focused = focused || item.Focused;

    if (item.Clicked || widget_internal::keyboard_toggle(id)) {
      *selected = index;
    }

    const Color fill =
        selected_tab
            ? Color::RgbaF(theme.WindowTitleActive.R, theme.WindowTitleActive.G,
                           theme.WindowTitleActive.B, 0.98F)
        : item.Active ? Color::RgbaF(theme.ButtonActive.R, theme.ButtonActive.G,
                                     theme.ButtonActive.B, 0.82F)
        : item.Hovered
            ? Color::RgbaF(theme.ButtonHovered.R, theme.ButtonHovered.G,
                           theme.ButtonHovered.B, 0.56F)
            : Color::RgbaF(theme.Button.R, theme.Button.G, theme.Button.B,
                           0.0F);

    if (selected_tab || item.Hovered || item.Active) {
      widget_internal::add_soft_rect(tab_bounds, 4.0F * scale, fill);
    }

    if (selected_tab) {
      widget_internal::add_hline(
          tab_bounds.Min.X + 5.0F * scale, tab_bounds.Max.X - 5.0F * scale,
          tab_bounds.Min.Y + 1.0F, Color::RgbaF(1.0F, 1.0F, 1.0F, 0.075F));
      widget_internal::add_soft_outline(
          tab_bounds, 4.0F * scale,
          Color::RgbaF(theme.WindowBorder.R, theme.WindowBorder.G,
                       theme.WindowBorder.B, 0.82F));
      widget_internal::add_filled_rect(
          {{tab_bounds.Min.X + 8.0F * scale, tab_bounds.Max.Y - 2.0F * scale},
           {tab_bounds.Max.X - 8.0F * scale, tab_bounds.Max.Y - 1.0F * scale}},
          Color::RgbaF(theme.TextSecondary.R, theme.TextSecondary.G,
                       theme.TextSecondary.B, 0.72F));
    }

    if (item.Focused) {
      widget_internal::add_soft_outline(
          tab_bounds, 4.0F * scale,
          Color::RgbaF(theme.WindowBorder.R, theme.WindowBorder.G,
                       theme.WindowBorder.B, 0.95F));
    }

    const float label_width = widget_internal::text_width(tabs[index]);
    const float label_x = std::max(
        tab_bounds.Min.X + 6.0F * scale,
        tab_bounds.Min.X +
            ((tab_bounds.Max.X - tab_bounds.Min.X) - label_width) * 0.5F);
    widget_internal::add_text(
        {{label_x, tab_bounds.Min.Y},
         {tab_bounds.Max.X - 6.0F * scale, tab_bounds.Max.Y}},
        tabs[index],
        selected_tab ? theme.Text
        : item.Hovered
            ? Color::RgbaF(theme.Text.R, theme.Text.G, theme.Text.B, 0.88F)
            : theme.TextSecondary);
  }

  widget_internal::layout.Cursor.Y += 4.0F * scale;
  widget_internal::set_last_item(
      CurrentId(label),
      hovered || widget_internal::contains(widget_internal::active_clip(bounds),
                                           io.MousePosition),
      active, focused);
  return true;
}

void EndTabs() {
  widget_internal::ensure_layout();
  widget_internal::layout.Cursor.Y +=
      CurrentStyle().ItemSpacingY * CurrentStyle().FrameScale;
}

} // namespace farcal
