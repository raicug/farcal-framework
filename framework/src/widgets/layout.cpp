#include <framework/internal/widgets.hpp>

#include <algorithm>
#include <stdexcept>

namespace farcal {

void Separator() {
  widget_internal::ensure_layout();
  const Style &theme = CurrentStyle();
  const float Width = widget_internal::layout.ContentWidth > 0.0F
                          ? widget_internal::layout.ContentWidth
                          : theme.ItemWidth * theme.FrameScale;
  widget_internal::add_hline(widget_internal::layout.Cursor.X,
                             widget_internal::layout.Cursor.X + Width,
                             widget_internal::layout.Cursor.Y +
                                 4.0F * theme.FrameScale,
                             {theme.WindowBorder.R, theme.WindowBorder.G,
                              theme.WindowBorder.B, 0.65F});
  widget_internal::layout.Cursor.Y += theme.ItemSpacingY * theme.FrameScale;
}

void Spacing() {
  widget_internal::ensure_layout();
  widget_internal::layout.Cursor.Y +=
      CurrentStyle().SectionSpacingY * CurrentStyle().FrameScale;
}

void SameLine(float Spacing) {
  widget_internal::ensure_layout();
  if (!widget_internal::layout.HasLastItem) {
    return;
  }

  const Style &theme = CurrentStyle();
  const float resolved_spacing =
      Spacing >= 0.0F ? Spacing : theme.ItemSpacingY * theme.FrameScale;
  widget_internal::layout.Cursor = {widget_internal::layout.LastItem.Max.X +
                                        resolved_spacing,
                                    widget_internal::layout.LastItem.Min.Y};
  widget_internal::layout.LineBottom = widget_internal::layout.LastItem.Max.Y;
}

void BeginGroup() {
  widget_internal::ensure_layout();
  widget_internal::group_stack.push_back({
      .Parent = widget_internal::layout,
  });
  widget_internal::layout.HasLastItem = false;
  widget_internal::layout.LineBottom = widget_internal::layout.Cursor.Y;
}

void EndGroup() {
  widget_internal::ensure_layout();
  if (widget_internal::group_stack.empty()) {
    throw std::logic_error("farcal Group stack is empty");
  }

  const widget_internal::layout_state group_layout = widget_internal::layout;
  widget_internal::layout = widget_internal::group_stack.back().Parent;
  widget_internal::group_stack.pop_back();

  const Rect bounds{
      widget_internal::layout.Cursor,
      {
          widget_internal::layout.ContentWidth > 0.0F
              ? widget_internal::layout.Cursor.X +
                    widget_internal::layout.ContentWidth
              : std::max(widget_internal::layout.Cursor.X,
                         group_layout.LastItem.Max.X),
          std::max(widget_internal::layout.Cursor.Y, group_layout.LineBottom),
      },
  };

  widget_internal::layout.Cursor.Y =
      bounds.Max.Y + CurrentStyle().ItemSpacingY * CurrentStyle().FrameScale;
  widget_internal::layout.LineBottom = bounds.Max.Y;
  widget_internal::layout.LastItem = bounds;
  widget_internal::layout.HasLastItem = true;
  widget_internal::set_last_item(
      CurrentId("Group"),
      widget_internal::contains(widget_internal::active_clip(bounds),
                                Input().MousePosition),
      false, widget_internal::focused_item == CurrentId("Group"));
}

void Indent(float Width) {
  widget_internal::ensure_layout();
  const float resolved_width =
      Width > 0.0F ? Width : 18.0F * CurrentStyle().FrameScale;
  widget_internal::layout.IndentWidth += resolved_width;
  widget_internal::layout.Cursor.X =
      widget_internal::layout.StartX + widget_internal::layout.IndentWidth;
}

void Unindent(float Width) {
  widget_internal::ensure_layout();
  const float resolved_width =
      Width > 0.0F ? Width : 18.0F * CurrentStyle().FrameScale;
  widget_internal::layout.IndentWidth =
      std::max(0.0F, widget_internal::layout.IndentWidth - resolved_width);
  widget_internal::layout.Cursor.X =
      widget_internal::layout.StartX + widget_internal::layout.IndentWidth;
}

void Dummy(Vec2 size) {
  widget_internal::ensure_layout();
  const Rect bounds = widget_internal::next_rect(size.X, size.Y);
  widget_internal::set_last_item(
      CurrentId("Dummy"),
      widget_internal::contains(widget_internal::active_clip(bounds),
                                Input().MousePosition),
      false, widget_internal::focused_item == CurrentId("Dummy"));
}

void SetNextItemWidth(float Width) {
  widget_internal::ensure_layout();
  widget_internal::layout.NextItemWidth = std::max(0.0F, Width);
}

bool IsItemHovered() {
  widget_internal::ensure_layout();
  return widget_internal::last_hovered;
}

bool IsItemActive() {
  widget_internal::ensure_layout();
  return widget_internal::last_active;
}

bool IsItemFocused() {
  widget_internal::ensure_layout();
  return widget_internal::last_focused;
}

} // namespace farcal
