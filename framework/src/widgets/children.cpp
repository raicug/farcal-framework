#include <framework/internal/widgets.hpp>

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace farcal {
namespace {

struct child_frame_state {
  widget_internal::layout_state Parent{};
  Rect Bounds{};
  Rect ScrollTrack{};
  std::uint64_t Id{};
  float ContentStartY{};
  float ContentPaddingY{};
  float VisibleHeight{};
  float MaxScroll{};
  bool Hovered{};
};

std::vector<child_frame_state> child_stack;
std::unordered_map<std::uint64_t, float> child_scroll;
std::unordered_map<std::uint64_t, float> child_content_height;
std::uint64_t child_frame = static_cast<std::uint64_t>(-1);

void ensure_child_frame() {
  if (child_frame == FrameIndex()) {
    return;
  }

  child_stack.clear();
  child_frame = FrameIndex();
}

} // namespace

bool BeginChild(std::string_view label, Vec2 size) {
  widget_internal::ensure_layout();
  ensure_child_frame();

  const Style &theme = CurrentStyle();
  const InputState &io = Input();
  const float scale = theme.FrameScale;
  const float width = size.X > 0.0F
                          ? size.X
                          : (widget_internal::layout.ContentWidth > 0.0F
                                 ? widget_internal::layout.ContentWidth
                                 : theme.ItemWidth * scale);
  const float height = size.Y > 0.0F ? size.Y : 168.0F * scale;
  const Rect bounds = widget_internal::next_rect(width, height);
  const Rect clip_bounds = widget_internal::active_clip(bounds);
  const std::uint64_t id = CurrentId(label);
  const bool hovered = widget_internal::contains(clip_bounds, io.MousePosition);
  const float padding_x = 8.0F * scale;
  const float padding_y = 10.0F * scale;
  const Rect inner{
      {bounds.Min.X + padding_x, bounds.Min.Y + padding_y},
      {bounds.Max.X - padding_x, bounds.Max.Y - padding_y},
  };
  const float visible_height = std::max(1.0F, inner.Max.Y - inner.Min.Y);
  const float content_height = child_content_height[id];
  const float max_scroll = std::max(0.0F, content_height - visible_height);
  float &scroll_y = child_scroll[id];

  if (max_scroll > 0.0F) {
    widget_internal::register_scroll_block(clip_bounds);
  }

  if (hovered && io.MouseWheel != 0.0F) {
    scroll_y =
        std::clamp(scroll_y - io.MouseWheel * 42.0F * scale, 0.0F, max_scroll);
  }
  scroll_y = std::clamp(scroll_y, 0.0F, max_scroll);

  widget_internal::add_filled_rect(
      bounds, Color::RgbaF(theme.WindowPanel.R, theme.WindowPanel.G,
                           theme.WindowPanel.B, 0.72F));
  widget_internal::add_hline(bounds.Min.X + 1.0F, bounds.Max.X - 1.0F,
                             bounds.Min.Y + 1.0F,
                             hovered ? Color::RgbaF(1.0F, 1.0F, 1.0F, 0.10F)
                                     : Color::RgbaF(1.0F, 1.0F, 1.0F, 0.04F));
  widget_internal::add_soft_outline(
      bounds, 3.0F * scale,
      hovered ? Color::RgbaF(theme.WindowBorder.R, theme.WindowBorder.G,
                             theme.WindowBorder.B, 0.92F)
              : theme.ButtonBorder);

  const float scrollbar_width = max_scroll > 0.0F ? 10.0F * scale : 0.0F;
  const Rect content_clip = widget_internal::active_clip(
      {inner.Min, {inner.Max.X - scrollbar_width, inner.Max.Y}});
  const Vec2 content_cursor{inner.Min.X, inner.Min.Y - scroll_y};

  child_stack.push_back({
      .Parent = widget_internal::layout,
      .Bounds = bounds,
      .ScrollTrack = {{inner.Max.X - 5.0F * scale, inner.Min.Y},
                      {inner.Max.X - 2.0F * scale, inner.Max.Y}},
      .Id = id,
      .ContentStartY = content_cursor.Y,
      .ContentPaddingY = padding_y,
      .VisibleHeight = visible_height,
      .MaxScroll = max_scroll,
      .Hovered = hovered,
  });

  widget_internal::layout.Cursor = content_cursor;
  widget_internal::layout.StartX = content_cursor.X;
  widget_internal::layout.ContentWidth =
      std::max(0.0F, content_clip.Max.X - content_clip.Min.X);
  widget_internal::layout.Clip = content_clip;
  widget_internal::layout.HasClip = true;
  widget_internal::layout.HasLastItem = false;
  widget_internal::layout.LineBottom = content_cursor.Y;
  widget_internal::layout.NextItemWidth = 0.0F;
  widget_internal::layout.IndentWidth = 0.0F;
  widget_internal::layout.ItemSpacingY = -1.0F;

  return true;
}

void EndChild() {
  widget_internal::ensure_layout();
  ensure_child_frame();

  if (child_stack.empty()) {
    throw std::logic_error("farcal Child stack is empty");
  }

  const child_frame_state state = child_stack.back();
  child_stack.pop_back();

  const float content_height =
      std::max(0.0F, widget_internal::layout.Cursor.Y - state.ContentStartY +
                         state.ContentPaddingY);
  child_content_height[state.Id] = content_height;

  if (state.MaxScroll > 0.0F) {
    const Style &theme = CurrentStyle();
    const Rect track = state.ScrollTrack;
    const float track_height = track.Max.Y - track.Min.Y;
    const float thumb_height = std::max(
        24.0F * theme.FrameScale,
        track_height * std::min(1.0F, state.VisibleHeight /
                                          std::max(1.0F, content_height)));
    const float scroll_y = child_scroll[state.Id];
    const float thumb_y =
        track.Min.Y + (track_height - thumb_height) *
                          (scroll_y / std::max(1.0F, state.MaxScroll));
    const Rect thumb{{track.Min.X, thumb_y},
                     {track.Max.X, thumb_y + thumb_height}};

    widget_internal::add_soft_rect(track, 2.0F * theme.FrameScale,
                                   Color::Rgba(0, 0, 0, 70));
    widget_internal::add_soft_rect(
        thumb, 2.0F * theme.FrameScale,
        Color::RgbaF(theme.TextMuted.R, theme.TextMuted.G, theme.TextMuted.B,
                     state.Hovered ? 0.68F : 0.42F));
  }

  widget_internal::layout = state.Parent;
  widget_internal::layout.LastItem = state.Bounds;
  widget_internal::layout.HasLastItem = true;
  widget_internal::layout.LineBottom = state.Bounds.Max.Y;
  widget_internal::set_last_item(state.Id, state.Hovered, false,
                                 widget_internal::focused_item == state.Id);
}

} // namespace farcal
