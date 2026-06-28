#include <framework/internal/widgets.hpp>

#include <dwrite.h>
#include <windows.h>

#include <algorithm>
#include <cstring>
#include <string>

namespace farcal {
namespace {

struct input_state {
  std::uint64_t Id{};
  std::size_t SelectionStart{};
  std::size_t SelectionEnd{};
};

input_state active_input{};

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

float estimated_text_width(std::string_view value, const Style &theme,
                           float scale) {
  return static_cast<float>(value.size()) * theme.FontSize * scale * 0.50F;
}

float input_text_width(std::string_view value, const Style &theme,
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
      return estimated_text_width(value, theme, scale);
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
    return estimated_text_width(value, theme, scale);
  }

  format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  const std::wstring text = widen(value);
  IDWriteTextLayout *layout{};
  result = factory->CreateTextLayout(
      text.c_str(), static_cast<UINT32>(text.size()), format, 100000.0F,
      theme.FontSize * scale * 2.0F, &layout);
  release_if_set(format);

  if (FAILED(result)) {
    return estimated_text_width(value, theme, scale);
  }

  DWRITE_TEXT_METRICS metrics{};
  result = layout->GetMetrics(&metrics);
  release_if_set(layout);

  if (FAILED(result)) {
    return estimated_text_width(value, theme, scale);
  }

  return metrics.widthIncludingTrailingWhitespace;
}

bool has_selection() {
  return active_input.SelectionStart != active_input.SelectionEnd;
}

void clear_selection() {
  active_input.SelectionStart = 0;
  active_input.SelectionEnd = 0;
}

void select_all(std::size_t length) {
  active_input.SelectionStart = 0;
  active_input.SelectionEnd = length;
}

bool erase_selection(char *buffer) {
  if (!has_selection()) {
    return false;
  }

  const std::size_t start =
      (std::min)(active_input.SelectionStart, active_input.SelectionEnd);
  const std::size_t end =
      (std::max)(active_input.SelectionStart, active_input.SelectionEnd);
  const std::size_t length = std::strlen(buffer);
  if (start >= end || end > length) {
    clear_selection();
    return false;
  }

  std::memmove(buffer + start, buffer + end, length - end + 1);
  clear_selection();
  return true;
}

bool erase_previous_word(char *buffer) {
  const std::size_t length = std::strlen(buffer);
  if (length == 0) {
    return false;
  }

  std::size_t end = length;
  while (end > 0 && buffer[end - 1] == ' ') {
    --end;
  }

  std::size_t start = end;
  while (start > 0 && buffer[start - 1] != ' ') {
    --start;
  }

  if (start == end) {
    start = 0;
  }

  std::memmove(buffer + start, buffer + end, length - end + 1);
  clear_selection();
  return true;
}

std::string selected_text(const char *buffer) {
  if (!has_selection()) {
    return {};
  }

  const std::size_t start =
      (std::min)(active_input.SelectionStart, active_input.SelectionEnd);
  const std::size_t end =
      (std::max)(active_input.SelectionStart, active_input.SelectionEnd);
  const std::size_t length = std::strlen(buffer);
  if (start >= end || end > length) {
    return {};
  }

  return {buffer + start, end - start};
}

void copy_clipboard(std::string_view value) {
  if (value.empty() || OpenClipboard(nullptr) == 0) {
    return;
  }

  EmptyClipboard();

  HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, value.size() + 1);
  if (memory == nullptr) {
    CloseClipboard();
    return;
  }

  void *target = GlobalLock(memory);
  if (target == nullptr) {
    GlobalFree(memory);
    CloseClipboard();
    return;
  }

  std::memcpy(target, value.data(), value.size());
  static_cast<char *>(target)[value.size()] = '\0';
  GlobalUnlock(memory);

  if (SetClipboardData(CF_TEXT, memory) == nullptr) {
    GlobalFree(memory);
  }

  CloseClipboard();
}

std::string paste_clipboard() {
  if (OpenClipboard(nullptr) == 0) {
    return {};
  }

  HANDLE handle = GetClipboardData(CF_TEXT);
  if (handle == nullptr) {
    CloseClipboard();
    return {};
  }

  const char *source = static_cast<const char *>(GlobalLock(handle));
  if (source == nullptr) {
    CloseClipboard();
    return {};
  }

  std::string result = source;
  GlobalUnlock(handle);
  CloseClipboard();
  return result;
}

bool append_text(char *buffer, std::size_t buffer_size,
                 std::string_view value) {
  std::size_t length = std::strlen(buffer);
  bool changed = false;

  for (char character : value) {
    if (character < 32 || character == 127) {
      continue;
    }

    if (length + 1 >= buffer_size) {
      break;
    }

    buffer[length] = character;
    ++length;
    buffer[length] = '\0';
    changed = true;
  }

  return changed;
}

} // namespace

bool Dropdown(std::string_view label, int *selected, const char *const *items,
              int item_count) {
  widget_internal::ensure_layout();
  if (selected == nullptr || items == nullptr || item_count <= 0) {
    return false;
  }

  *selected = std::clamp(*selected, 0, item_count - 1);

  const Style &theme = CurrentStyle();
  const InputState &io = Input();
  const float scale = theme.FrameScale;
  const float height = 32.0F * scale;
  const float width = widget_internal::layout.ContentWidth > 0.0F
                          ? widget_internal::layout.ContentWidth
                          : theme.ItemWidth * scale;
  const std::uint64_t id = CurrentId(label);
  const bool open = widget_internal::open_dropdown == id;
  const int visible_items = (std::min)(item_count, 6);
  const float popup_gap = 2.0F * scale;
  const float option_height = 26.0F * scale;
  const float popup_height =
      open ? popup_gap + static_cast<float>(visible_items) * option_height
           : 0.0F;
  const Rect bounds = widget_internal::next_rect(width, height + popup_height);
  const Rect frame{bounds.Min, {bounds.Max.X, bounds.Min.Y + height}};
  const widget_internal::item_result item =
      widget_internal::item_behavior(id, frame);

  if (open && io.MousePressed[0] &&
      !widget_internal::contains(widget_internal::active_clip(bounds),
                                 io.MousePosition)) {
    widget_internal::open_dropdown = 0;
  }

  if (item.Clicked || widget_internal::keyboard_toggle(id)) {
    widget_internal::open_dropdown = open ? 0 : id;
  }

  bool changed = false;
  const bool now_open = widget_internal::open_dropdown == id;
  const Color fill =
      item.Active ? theme.ButtonActive
      : item.Hovered || now_open
          ? Color::RgbaF(theme.ButtonHovered.R, theme.ButtonHovered.G,
                         theme.ButtonHovered.B, 0.82F)
          : theme.Button;
  const Color border =
      now_open
          ? Color::RgbaF(theme.Accent.R, theme.Accent.G, theme.Accent.B, 0.64F)
      : item.Focused ? theme.Accent
      : item.Hovered ? Color::RgbaF(theme.WindowBorder.R, theme.WindowBorder.G,
                                    theme.WindowBorder.B, 0.92F)
                     : theme.ButtonBorder;

  widget_internal::add_filled_rect(frame, fill);
  widget_internal::add_hline(
      frame.Min.X + 1.0F, frame.Max.X - 1.0F, frame.Min.Y + 1.0F,
      item.Hovered ? Color::RgbaF(1.0F, 1.0F, 1.0F, 0.12F)
                   : Color::RgbaF(1.0F, 1.0F, 1.0F, 0.05F));
  widget_internal::add_soft_outline(frame, 0.0F, border);
  widget_internal::add_text(
      {{frame.Min.X + theme.FramePaddingX * scale, frame.Min.Y},
       {frame.Max.X - 28.0F * scale, frame.Max.Y}},
      items[*selected], theme.Text);

  const float arrow_x = frame.Max.X - 15.0F * scale;
  const float arrow_y = frame.Min.Y + 15.0F * scale;
  const Color arrow = now_open ? theme.Text : theme.TextSecondary;
  widget_internal::add_line({arrow_x - 4.0F * scale, arrow_y - 2.0F * scale},
                            {arrow_x, arrow_y + 2.0F * scale}, arrow,
                            1.0F * scale);
  widget_internal::add_line({arrow_x, arrow_y + 2.0F * scale},
                            {arrow_x + 4.0F * scale, arrow_y - 2.0F * scale},
                            arrow, 1.0F * scale);

  if (open) {
    const Rect popup{{bounds.Min.X, frame.Max.Y + popup_gap}, bounds.Max};
    widget_internal::add_filled_rect(
        popup, Color::RgbaF(theme.WindowPanel.R, theme.WindowPanel.G,
                            theme.WindowPanel.B, 0.98F));
    widget_internal::add_soft_outline(
        popup, 0.0F,
        Color::RgbaF(theme.WindowBorder.R, theme.WindowBorder.G,
                     theme.WindowBorder.B, 0.76F));

    for (int index = 0; index < visible_items; ++index) {
      const Rect option{
          {popup.Min.X + 1.0F,
           popup.Min.Y + static_cast<float>(index) * option_height},
          {popup.Max.X - 1.0F,
           popup.Min.Y + static_cast<float>(index + 1) * option_height},
      };
      const std::uint64_t option_id = CurrentId(items[index]);
      const widget_internal::item_result option_item =
          widget_internal::item_behavior(option_id, option);
      const bool option_selected = *selected == index;

      if (option_item.Clicked || widget_internal::keyboard_toggle(option_id)) {
        *selected = index;
        widget_internal::open_dropdown = 0;
        changed = true;
      }

      if (option_selected || option_item.Hovered || option_item.Active) {
        const Color option_fill =
            option_item.Active ? theme.ButtonActive
            : option_item.Hovered
                ? Color::RgbaF(theme.ButtonHovered.R, theme.ButtonHovered.G,
                               theme.ButtonHovered.B, 0.74F)
                : Color::RgbaF(theme.Button.R, theme.Button.G, theme.Button.B,
                               0.42F);
        widget_internal::add_filled_rect(option, option_fill);
      }

      if (option_selected) {
        widget_internal::add_filled_rect(
            {{option.Min.X + 2.0F * scale, option.Min.Y + 6.0F * scale},
             {option.Min.X + 4.0F * scale, option.Max.Y - 6.0F * scale}},
            theme.Accent);
      }

      widget_internal::add_text({{option.Min.X + theme.FramePaddingX * scale +
                                      (option_selected ? 6.0F * scale : 0.0F),
                                  option.Min.Y},
                                 option.Max},
                                items[index],
                                option_selected || option_item.Hovered
                                    ? theme.Text
                                    : theme.TextSecondary);
    }
  }

  return changed;
}

bool InputText(std::string_view label, char *buffer, std::size_t buffer_size) {
  widget_internal::ensure_layout();
  if (buffer == nullptr || buffer_size == 0) {
    return false;
  }

  buffer[buffer_size - 1] = '\0';

  const Style &theme = CurrentStyle();
  const InputState &io = Input();
  const float scale = theme.FrameScale;
  const float height = 32.0F * scale;
  const float width = widget_internal::layout.ContentWidth > 0.0F
                          ? widget_internal::layout.ContentWidth
                          : theme.ItemWidth * scale;
  const Rect bounds = widget_internal::next_rect(width, height);
  const std::uint64_t id = CurrentId(label);
  const widget_internal::item_result item =
      widget_internal::item_behavior(id, bounds);
  const bool focused = widget_internal::focused_item == id;
  bool changed = false;
  std::size_t length = std::strlen(buffer);

  if (focused && active_input.Id != id) {
    active_input.Id = id;
    clear_selection();
  }

  if (item.Clicked) {
    clear_selection();
  }

  if (focused && io.KeyPressed[27]) {
    widget_internal::focused_item = 0;
    active_input.Id = 0;
    clear_selection();
  }

  const bool backspace_pressed = io.KeyPressed[8] || io.KeyRepeated[8];
  const bool delete_pressed = io.KeyPressed[46] || io.KeyRepeated[46];

  if (focused && io.Control && io.KeyPressed['A']) {
    select_all(length);
  }

  if (focused && io.Control && io.KeyPressed['C']) {
    copy_clipboard(selected_text(buffer));
  }

  if (focused && io.Control && io.KeyPressed['V']) {
    if (has_selection()) {
      changed = erase_selection(buffer) || changed;
    }

    changed = append_text(buffer, buffer_size, paste_clipboard()) || changed;
    length = std::strlen(buffer);
  }

  if (focused && io.Control && (delete_pressed || backspace_pressed)) {
    if (has_selection()) {
      changed = erase_selection(buffer);
    } else if (length > 0) {
      changed = erase_previous_word(buffer);
    }

    length = std::strlen(buffer);
  } else if (focused && delete_pressed && has_selection()) {
    changed = erase_selection(buffer);
    length = std::strlen(buffer);
  }

  if (focused && backspace_pressed) {
    if (has_selection()) {
      changed = erase_selection(buffer);
      length = std::strlen(buffer);
    } else if (length > 0) {
      buffer[length - 1] = '\0';
      --length;
      changed = true;
    }
  }

  if (focused && !io.TextInput.empty()) {
    if (has_selection()) {
      changed = erase_selection(buffer) || changed;
    }

    changed = append_text(buffer, buffer_size, io.TextInput) || changed;
  }

  const Color fill = item.Active               ? theme.ButtonActive
                     : item.Hovered || focused ? theme.ButtonHovered
                                               : theme.Button;
  const Color border =
      focused ? theme.Accent
      : item.Hovered
          ? Color::RgbaF(theme.Accent.R, theme.Accent.G, theme.Accent.B, 0.42F)
          : theme.ButtonBorder;

  widget_internal::add_filled_rect(bounds, fill);
  widget_internal::add_hline(
      bounds.Min.X + 1.0F, bounds.Max.X - 1.0F, bounds.Min.Y + 1.0F,
      item.Hovered ? Color::RgbaF(1.0F, 1.0F, 1.0F, 0.12F)
                   : Color::RgbaF(1.0F, 1.0F, 1.0F, 0.05F));
  widget_internal::add_soft_outline(bounds, 0.0F, border);

  const std::string_view text =
      buffer[0] != '\0' ? std::string_view(buffer) : label;
  const Color text_color = buffer[0] != '\0' ? theme.Text : theme.TextMuted;
  const float text_min_x = bounds.Min.X + theme.FramePaddingX * scale;
  const float text_max_x = bounds.Max.X - theme.FramePaddingX * scale;
  const float text_area_width = (std::max)(0.0F, text_max_x - text_min_x);
  const float typed_width = input_text_width(buffer, theme, scale);
  const float text_scroll =
      focused && buffer[0] != '\0'
          ? (std::max)(0.0F, typed_width - text_area_width)
          : 0.0F;
  const Rect text_bounds{{text_min_x - text_scroll, bounds.Min.Y},
                         {text_max_x, bounds.Max.Y}};

  if (focused && has_selection()) {
    const std::size_t selection_start =
        (std::min)(active_input.SelectionStart, active_input.SelectionEnd);
    const std::size_t selection_end =
        (std::max)(active_input.SelectionStart, active_input.SelectionEnd);
    const float selection_x0 =
        text_min_x +
        input_text_width(std::string_view(buffer, selection_start), theme,
                         scale) -
        text_scroll;
    const float selection_x1 =
        text_min_x +
        input_text_width(std::string_view(buffer, selection_end), theme,
                         scale) -
        text_scroll;
    const Rect selection_bounds{
        {std::clamp(selection_x0, text_min_x, text_max_x),
         bounds.Min.Y + 5.0F * scale},
        {std::clamp(selection_x1, text_min_x, text_max_x),
         bounds.Max.Y - 5.0F * scale},
    };

    if (selection_bounds.Max.X > selection_bounds.Min.X) {
      widget_internal::add_filled_rect(
          selection_bounds, Color::RgbaF(theme.Selection.R, theme.Selection.G,
                                         theme.Selection.B, 0.64F));
    }
  }

  widget_internal::add_text(text_bounds, text, text_color);

  if (focused && !has_selection() && (FrameIndex() / 30U) % 2U == 0U) {
    const float cursor_x = std::clamp(text_min_x + typed_width - text_scroll,
                                      text_min_x, text_max_x);
    widget_internal::add_vline(cursor_x, bounds.Min.Y + 8.0F * scale,
                               bounds.Max.Y - 8.0F * scale, theme.Text);
  }

  return changed;
}

bool IsTextInputFocused() {
  return active_input.Id != 0 &&
         widget_internal::focused_item == active_input.Id;
}

} // namespace farcal
