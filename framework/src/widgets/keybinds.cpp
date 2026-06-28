#include <framework/internal/widgets.hpp>

#include <dwrite.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <string_view>
#include <windows.h>

namespace farcal {
namespace {

constexpr int no_key = 0;
constexpr int mouse_base = 256;
std::uint64_t capturing_keybind{};
std::uint64_t open_keybind_menu{};

template <typename Value> void release_if_set(Value *&value) {
  if (value != nullptr) {
    value->Release();
    value = nullptr;
  }
}

std::wstring widen(std::string_view value) {
  std::wstring result;
  result.reserve(value.size());

  for (char character : value) {
    result.push_back(
        static_cast<wchar_t>(static_cast<unsigned char>(character)));
  }

  return result;
}

bool extended_key(int value) {
  switch (value) {
  case VK_INSERT:
  case VK_DELETE:
  case VK_HOME:
  case VK_END:
  case VK_PRIOR:
  case VK_NEXT:
  case VK_LEFT:
  case VK_RIGHT:
  case VK_UP:
  case VK_DOWN:
  case VK_NUMLOCK:
  case VK_DIVIDE:
  case VK_LWIN:
  case VK_RWIN:
  case VK_APPS:
  case VK_RCONTROL:
  case VK_RMENU:
    return true;
  default:
    return false;
  }
}

float raw_text_width(std::string_view value, const Style &theme, float scale) {
  return static_cast<float>(value.size()) * theme.FontSize * scale * 0.54F;
}

float measured_text_width(std::string_view value, const Style &theme,
                          float scale) {
  if (value.empty()) {
    return 0.0F;
  }

  static IDWriteFactory *factory{};
  if (factory == nullptr) {
    const HRESULT result = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown **>(&factory));
    if (FAILED(result)) {
      return raw_text_width(value, theme, scale);
    }
  }

  IDWriteTextFormat *format{};
  const std::wstring family =
      theme.FontFamily.empty() ? L"Inter" : theme.FontFamily;
  HRESULT result = factory->CreateTextFormat(
      family.c_str(), nullptr, DWRITE_FONT_WEIGHT_NORMAL,
      DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
      theme.FontSize * scale, L"", &format);

  if (FAILED(result)) {
    return raw_text_width(value, theme, scale);
  }

  format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  const std::wstring text = widen(value);
  IDWriteTextLayout *layout{};
  result = factory->CreateTextLayout(
      text.c_str(), static_cast<UINT32>(text.size()), format, 100000.0F,
      theme.FontSize * scale * 2.0F, &layout);
  release_if_set(format);

  if (FAILED(result)) {
    return raw_text_width(value, theme, scale);
  }

  DWRITE_TEXT_METRICS metrics{};
  result = layout->GetMetrics(&metrics);
  release_if_set(layout);

  if (FAILED(result)) {
    return raw_text_width(value, theme, scale);
  }

  return metrics.widthIncludingTrailingWhitespace;
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

void menu_outline(Rect bounds, Color tint) {
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

std::string_view key_name(int value) {
  if (value == no_key) {
    return "Unbound";
  }

  if (value >= mouse_base) {
    switch (value - mouse_base) {
    case 0:
      return "Mouse 1";
    case 1:
      return "Mouse 2";
    case 2:
      return "Mouse 3";
    case 3:
      return "Mouse 4";
    case 4:
      return "Mouse 5";
    default:
      return "Mouse ?";
    }
  }

  static std::array<char, 64> buffer{};
  const UINT scan_code =
      MapVirtualKeyA(static_cast<UINT>(value), MAPVK_VK_TO_VSC);
  if (scan_code != 0) {
    LONG parameter = static_cast<LONG>(scan_code << 16U);
    if (extended_key(value)) {
      parameter |= 1L << 24;
    }

    const int length = GetKeyNameTextA(parameter, buffer.data(),
                                       static_cast<int>(buffer.size()));
    if (length > 0) {
      return {buffer.data(), static_cast<std::size_t>(length)};
    }
  }

  std::snprintf(buffer.data(), buffer.size(), "Key %d", value);
  return {buffer.data()};
}

int pressed_binding(const InputState &io) {
  for (std::size_t index = 0; index < io.MousePressed.size(); ++index) {
    if (io.MousePressed[index]) {
      return mouse_base + static_cast<int>(index);
    }
  }

  for (std::size_t key = 1; key < io.KeyPressed.size(); ++key) {
    if (io.KeyPressed[key]) {
      return static_cast<int>(key);
    }
  }

  return no_key;
}

bool mode_popup(std::uint64_t id, Rect value_rect, KeybindMode *mode) {
  if (mode == nullptr || open_keybind_menu != id) {
    return false;
  }

  const Style &theme = CurrentStyle();
  const InputState &io = Input();
  const float scale = theme.FrameScale;
  const float option_height = 26.0F * scale;
  const float width = 92.0F * scale;
  const float gap = 3.0F * scale;
  const Rect popup{
      {value_rect.Max.X - width, value_rect.Max.Y + gap},
      {value_rect.Max.X, value_rect.Max.Y + gap + option_height * 2.0F},
  };

  if ((io.MousePressed[0] || io.MousePressed[1]) &&
      !widget_internal::contains(popup, io.MousePosition) &&
      !widget_internal::contains(value_rect, io.MousePosition)) {
    open_keybind_menu = 0;
  }

  menu_filled_rect(popup, Color::RgbaF(theme.WindowPanel.R, theme.WindowPanel.G,
                                       theme.WindowPanel.B, 0.98F));
  menu_hline(popup.Min.X + 1.0F, popup.Max.X - 1.0F, popup.Min.Y + 1.0F,
             Color::RgbaF(1.0F, 1.0F, 1.0F, 0.06F));
  menu_outline(popup, Color::RgbaF(theme.WindowBorder.R, theme.WindowBorder.G,
                                   theme.WindowBorder.B, 0.92F));

  bool changed = false;
  const KeybindMode modes[2]{KeybindMode::Toggle, KeybindMode::Hold};
  const char *labels[2]{"Toggle", "Hold"};

  for (int index = 0; index < 2; ++index) {
    const Rect option{
        {popup.Min.X + 1.0F,
         popup.Min.Y + static_cast<float>(index) * option_height},
        {popup.Max.X - 1.0F,
         popup.Min.Y + static_cast<float>(index + 1) * option_height},
    };
    const widget_internal::item_result item =
        menu_behavior(CurrentId(labels[index]), option);
    const bool selected = *mode == modes[index];

    if (item.Clicked) {
      changed = *mode != modes[index];
      *mode = modes[index];
      open_keybind_menu = 0;
    }

    if (selected || item.Hovered || item.Active) {
      const Color fill =
          item.Active ? theme.ButtonActive
          : item.Hovered
              ? Color::RgbaF(theme.ButtonHovered.R, theme.ButtonHovered.G,
                             theme.ButtonHovered.B, 0.74F)
              : Color::RgbaF(theme.Button.R, theme.Button.G, theme.Button.B,
                             0.42F);
      menu_filled_rect(option, fill);
    }

    if (selected) {
      menu_filled_rect(
          {{option.Min.X + 3.0F * scale, option.Min.Y + 6.0F * scale},
           {option.Min.X + 5.0F * scale, option.Max.Y - 6.0F * scale}},
          theme.Accent);
    }

    menu_text({{option.Min.X + 11.0F * scale, option.Min.Y}, option.Max},
              labels[index],
              selected || item.Hovered ? theme.Text : theme.TextSecondary);
  }

  return changed;
}

} // namespace

bool Keybind(std::string_view label, int *value) {
  return Keybind(label, value, nullptr);
}

bool Keybind(std::string_view label, int *value, KeybindMode *mode) {
  widget_internal::ensure_layout();
  if (value == nullptr) {
    return false;
  }

  const Style &theme = CurrentStyle();
  const InputState &io = Input();
  const float scale = theme.FrameScale;
  const float height = 32.0F * scale;
  const float width = widget_internal::layout.ContentWidth > 0.0F
                          ? widget_internal::layout.ContentWidth
                          : theme.ItemWidth * scale;
  const Rect bounds = widget_internal::next_rect(width, height);
  const std::uint64_t id = CurrentId(label);
  const bool was_capturing = capturing_keybind == id;
  const std::string_view display_text =
      was_capturing ? "..." : key_name(*value);
  const float display_width = measured_text_width(display_text, theme, scale);
  const float value_width =
      std::clamp(display_width + 24.0F * scale, 42.0F * scale, 86.0F * scale);
  const float value_height = 28.0F * scale;
  const float value_y = bounds.Min.Y + (height - value_height) * 0.5F;
  const Rect value_rect{
      {bounds.Max.X - value_width, value_y},
      {bounds.Max.X, value_y + value_height},
  };
  const float gap = 8.0F * scale;
  const Rect label_rect{
      bounds.Min,
      {value_rect.Min.X - gap, bounds.Max.Y},
  };
  const widget_internal::item_result item =
      widget_internal::item_behavior(id, value_rect);
  const bool activated =
      !was_capturing && (item.Clicked || widget_internal::keyboard_toggle(id));
  bool started_capture = false;
  bool changed = false;

  if (mode != nullptr && item.Hovered && io.MousePressed[1]) {
    open_keybind_menu = open_keybind_menu == id ? 0 : id;
    capturing_keybind = 0;
    widget_internal::focused_item = id;
  }

  if (activated) {
    capturing_keybind = id;
    open_keybind_menu = 0;
    widget_internal::focused_item = id;
    started_capture = true;
  }

  const bool capturing = capturing_keybind == id;
  if (capturing && !started_capture) {
    if (io.MousePressed[0] && !item.Hovered) {
      capturing_keybind = 0;
    } else if (io.KeyPressed[27]) {
      changed = *value != no_key;
      *value = no_key;
      capturing_keybind = 0;
    } else {
      const int next = pressed_binding(io);
      if (next != no_key) {
        changed = *value != next;
        *value = next;
        capturing_keybind = 0;
      }
    }
  }

  const bool now_capturing = capturing_keybind == id;
  const std::string_view text = now_capturing ? "..." : key_name(*value);
  const float text_width = measured_text_width(text, theme, scale);

  const Color fill =
      item.Active ? theme.ButtonActive
      : item.Hovered || now_capturing
          ? Color::RgbaF(theme.ButtonHovered.R, theme.ButtonHovered.G,
                         theme.ButtonHovered.B, 0.82F)
          : theme.Button;
  const Color border =
      now_capturing || open_keybind_menu == id
          ? Color::RgbaF(theme.Accent.R, theme.Accent.G, theme.Accent.B, 0.82F)
      : item.Focused
          ? Color::RgbaF(theme.Accent.R, theme.Accent.G, theme.Accent.B, 0.44F)
      : item.Hovered ? Color::RgbaF(theme.WindowBorder.R, theme.WindowBorder.G,
                                    theme.WindowBorder.B, 0.88F)
                     : theme.ButtonBorder;
  const Color value_fill =
      now_capturing ? Color::RgbaF(theme.ButtonPrimary.R, theme.ButtonPrimary.G,
                                   theme.ButtonPrimary.B, 0.40F)
                    : Color::RgbaF(theme.WindowPanel.R, theme.WindowPanel.G,
                                   theme.WindowPanel.B, 0.88F);

  widget_internal::add_text(
      label_rect, label,
      item.Hovered || now_capturing ? theme.Text : theme.TextSecondary);
  widget_internal::add_filled_rect(value_rect, fill);
  widget_internal::add_filled_rect(
      {{value_rect.Min.X + 1.0F, value_rect.Min.Y + 1.0F},
       {value_rect.Max.X - 1.0F, value_rect.Max.Y - 1.0F}},
      value_fill);
  widget_internal::add_hline(
      value_rect.Min.X + 1.0F, value_rect.Max.X - 1.0F, value_rect.Min.Y + 1.0F,
      item.Hovered || now_capturing ? Color::RgbaF(1.0F, 1.0F, 1.0F, 0.13F)
                                    : Color::RgbaF(1.0F, 1.0F, 1.0F, 0.05F));
  widget_internal::add_soft_outline(value_rect, 0.0F, border);

  const float text_x =
      value_rect.Min.X +
      (std::max)(0.0F,
                 (value_rect.Max.X - value_rect.Min.X - text_width) * 0.5F);
  widget_internal::add_text(
      {{text_x, value_rect.Min.Y},
       {text_x + text_width + 1.0F * scale, value_rect.Max.Y}},
      text, now_capturing || *value != no_key ? theme.Text : theme.TextMuted);

  PushId(label);
  changed = mode_popup(id, value_rect, mode) || changed;
  PopId();

  return changed;
}

} // namespace farcal
