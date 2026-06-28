#include <framework/internal/widgets.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace farcal {
namespace {

void tooltip_filled_rect(Rect bounds, Color tint) {
  ForegroundRenderer().Commands.push_back({
      .Type = DrawCommandType::FilledRect,
      .Bounds = bounds,
      .Clip = {},
      .Tint = tint,
      .AntiAliasing = CurrentStyle().AntiAliasing,
  });
}

void tooltip_line(Vec2 start, Vec2 end, Color tint, float thickness) {
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

void tooltip_outline(Rect bounds, Color tint) {
  const float left = bounds.Min.X + 0.5F;
  const float top = bounds.Min.Y + 0.5F;
  const float right = bounds.Max.X - 0.5F;
  const float bottom = bounds.Max.Y - 0.5F;

  tooltip_line({left, top}, {right, top}, tint, 1.0F);
  tooltip_line({right, top}, {right, bottom}, tint, 1.0F);
  tooltip_line({right, bottom}, {left, bottom}, tint, 1.0F);
  tooltip_line({left, bottom}, {left, top}, tint, 1.0F);
}

void tooltip_text(Rect bounds, std::string_view value, Color tint) {
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

std::vector<std::string> wrap_text(std::string_view value, float max_width) {
  std::vector<std::string> lines;
  std::string line;
  std::size_t offset = 0;

  while (offset < value.size()) {
    while (offset < value.size() && value[offset] == ' ') {
      ++offset;
    }

    const std::size_t start = offset;
    while (offset < value.size() && value[offset] != ' ') {
      ++offset;
    }

    std::string_view word{value.data() + start, offset - start};
    if (word.empty()) {
      continue;
    }

    const std::string candidate =
        line.empty() ? std::string(word) : line + " " + std::string(word);
    if (!line.empty() && widget_internal::text_width(candidate) > max_width) {
      lines.push_back(line);
      line = std::string(word);
    } else {
      line = candidate;
    }
  }

  if (!line.empty()) {
    lines.push_back(line);
  }

  if (lines.empty()) {
    lines.emplace_back(value);
  }

  return lines;
}

} // namespace

void Tooltip(std::string_view value) {
  if (value.empty()) {
    return;
  }

  const Style &theme = CurrentStyle();
  const float scale = theme.FrameScale;
  const float max_text_width = 250.0F * scale;
  const float padding_x = 8.0F * scale;
  const float padding_y = 6.0F * scale;
  const float line_height = (theme.FontSize + 4.0F) * scale;
  const std::vector<std::string> lines = wrap_text(value, max_text_width);
  float text_width = 0.0F;
  for (const std::string &line : lines) {
    text_width =
        std::max(text_width,
                 std::min(widget_internal::text_width(line), max_text_width));
  }

  const float width = text_width + padding_x * 2.0F;
  const float height =
      padding_y * 2.0F + line_height * static_cast<float>(lines.size());
  const Vec2 mouse = Input().MousePosition;
  const Rect bounds{
      {mouse.X + 14.0F * scale, mouse.Y + 16.0F * scale},
      {mouse.X + 14.0F * scale + width, mouse.Y + 16.0F * scale + height},
  };
  const Rect shadow{
      {bounds.Min.X + 2.0F * scale, bounds.Min.Y + 2.0F * scale},
      {bounds.Max.X + 2.0F * scale, bounds.Max.Y + 2.0F * scale},
  };

  tooltip_filled_rect(shadow, Color::Rgba(0, 0, 0, 90));
  tooltip_filled_rect(bounds,
                      Color::RgbaF(theme.WindowPanel.R, theme.WindowPanel.G,
                                   theme.WindowPanel.B, 0.98F));
  tooltip_filled_rect({{bounds.Min.X, bounds.Min.Y},
                       {bounds.Min.X + 2.0F * scale, bounds.Max.Y}},
                      theme.Accent);
  tooltip_line({bounds.Min.X + 1.0F, bounds.Min.Y + 1.5F},
               {bounds.Max.X - 1.0F, bounds.Min.Y + 1.5F},
               Color::RgbaF(1.0F, 1.0F, 1.0F, 0.07F), 1.0F);
  tooltip_outline(bounds,
                  Color::RgbaF(theme.WindowBorder.R, theme.WindowBorder.G,
                               theme.WindowBorder.B, 0.94F));

  for (std::size_t index = 0; index < lines.size(); ++index) {
    const float line_y = bounds.Min.Y + padding_y +
                         static_cast<float>(index) * line_height - 1.0F * scale;
    tooltip_text({{bounds.Min.X + padding_x, line_y},
                  {bounds.Max.X - padding_x, line_y + line_height}},
                 lines[index], theme.Text);
  }
}

void SetTooltip(std::string_view value) {
  if (IsItemHovered()) {
    Tooltip(value);
  }
}

} // namespace farcal
