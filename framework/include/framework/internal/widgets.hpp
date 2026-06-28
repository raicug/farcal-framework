#pragma once

#include <framework/context.hpp>
#include <framework/widgets.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace farcal::widget_internal {

struct layout_state {
  Vec2 Cursor{24.0F, 24.0F};
  float StartX{24.0F};
  float ContentWidth{};
  Rect Clip{};
  bool HasClip{};
  Rect LastItem{};
  bool HasLastItem{};
  float LineBottom{};
  float NextItemWidth{};
  float IndentWidth{};
  float ItemSpacingY{-1.0F};
};

struct window_state {
  Vec2 Position{};
  Vec2 DragOffset{};
  float ContentHeight{};
  float ScrollY{};
  bool Initialized{};
  bool Dragging{};
};

struct window_layout_state {
  std::string Title{};
  float ContentStartY{};
  float ContentPaddingY{};
  float VisibleHeight{};
  float MaxScroll{};
  bool Hovered{};
  Rect ScrollTrack{};
};

struct group_state {
  layout_state Parent{};
};

struct list_state {
  layout_state Parent{};
  Rect Bounds{};
  std::uint64_t Id{};
  bool Hovered{};
};

struct item_result {
  bool Hovered{};
  bool Active{};
  bool Focused{};
  bool Clicked{};
};

extern layout_state layout;
extern std::vector<layout_state> layout_stack;
extern std::vector<window_layout_state> window_stack;
extern std::vector<group_state> group_stack;
extern std::vector<list_state> list_stack;
extern std::vector<Rect> scroll_block_regions;
extern std::vector<Rect> previous_scroll_block_regions;
extern std::unordered_map<std::string, window_state> windows;
extern std::uint64_t layout_frame;
extern std::uint64_t active_item;
extern std::uint64_t focused_item;
extern std::uint64_t LastItem;
extern std::uint64_t open_dropdown;
extern bool last_hovered;
extern bool last_active;
extern bool last_focused;

void ensure_layout();
DrawData &mutable_draw();
bool contains(Rect bounds, Vec2 point);
Rect intersect(Rect a, Rect b);
bool has_area(Rect value);
Rect active_clip(Rect bounds);
Rect command_clip(Rect bounds);
bool is_visible(Rect bounds);
float text_width(std::string_view value);
float text_bounds_width(std::string_view value);
Rect next_rect(float Width, float Height);
void advance_layout(float Height);
void set_last_item(std::uint64_t id, bool Hovered, bool Active, bool Focused);
void register_scroll_block(Rect bounds);
bool is_scroll_blocked(Vec2 point);
item_result item_behavior(std::uint64_t id, Rect bounds);
bool keyboard_toggle(std::uint64_t id);
double normalized_value(double value, double minimum, double maximum);
double value_from_normalized(double value, double minimum, double maximum);
bool set_scalar_value(double *target, double value, double minimum,
                      double maximum, bool integral);
void add_text(Rect bounds, std::string_view value, Color tint);
void add_filled_rect(Rect bounds, Color tint);
void add_rect(Rect bounds, Color tint, float thickness = 1.0F);
void add_line(Vec2 start, Vec2 end, Color tint, float thickness);
void add_hline(float x0, float x1, float y, Color tint);
void add_vline(float x, float y0, float y1, Color tint);
void add_soft_rect(Rect bounds, float radius, Color tint);
void add_soft_outline(Rect bounds, float radius, Color tint);
bool button_impl(std::string_view label, Color normal, Color hovered_color,
                 Color active_color, Color text_color);
void format_slider_value(char *buffer, std::size_t buffer_size, double value,
                         bool integral, std::string_view suffix);

} // namespace farcal::widget_internal
