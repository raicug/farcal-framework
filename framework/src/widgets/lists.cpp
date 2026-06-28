#include <framework/internal/widgets.hpp>

#include <algorithm>
#include <stdexcept>

namespace farcal {

bool BeginList(std::string_view label, Vec2 size)
{
    widget_internal::ensure_layout();

    const Style& theme = CurrentStyle();
    const float scale = theme.FrameScale;
    const float width = size.X > 0.0F ? size.X : (widget_internal::layout.ContentWidth > 0.0F ? widget_internal::layout.ContentWidth : theme.ItemWidth * scale);
    const float height = size.Y > 0.0F ? size.Y : 156.0F * scale;
    const Rect bounds = widget_internal::next_rect(width, height);
    const std::uint64_t id = CurrentId(label);
    const bool hovered = widget_internal::contains(widget_internal::active_clip(bounds), Input().MousePosition);
    const Rect inner {
        {bounds.Min.X + 6.0F * scale, bounds.Min.Y + 6.0F * scale},
        {bounds.Max.X - 6.0F * scale, bounds.Max.Y - 6.0F * scale},
    };

    widget_internal::add_filled_rect(bounds, Color::RgbaF(theme.WindowPanel.R, theme.WindowPanel.G, theme.WindowPanel.B, 0.88F));
    widget_internal::add_hline(bounds.Min.X + 1.0F, bounds.Max.X - 1.0F, bounds.Min.Y + 1.0F, hovered ? Color::RgbaF(1.0F, 1.0F, 1.0F, 0.10F) : Color::RgbaF(1.0F, 1.0F, 1.0F, 0.04F));
    widget_internal::add_soft_outline(bounds, 3.0F * scale, hovered ? Color::RgbaF(theme.WindowBorder.R, theme.WindowBorder.G, theme.WindowBorder.B, 0.92F) : theme.ButtonBorder);

    widget_internal::list_stack.push_back({
        .Parent = widget_internal::layout,
        .Bounds = bounds,
        .Id = id,
        .Hovered = hovered,
    });

    widget_internal::layout.Cursor = inner.Min;
    widget_internal::layout.StartX = inner.Min.X;
    widget_internal::layout.ContentWidth = std::max(0.0F, inner.Max.X - inner.Min.X);
    widget_internal::layout.Clip = widget_internal::active_clip(inner);
    widget_internal::layout.HasClip = true;
    widget_internal::layout.HasLastItem = false;
    widget_internal::layout.LineBottom = inner.Min.Y;
    widget_internal::layout.NextItemWidth = 0.0F;
    widget_internal::layout.IndentWidth = 0.0F;
    widget_internal::layout.ItemSpacingY = 0.0F;

    return true;
}

void EndList()
{
    widget_internal::ensure_layout();
    if (widget_internal::list_stack.empty()) {
        throw std::logic_error("farcal List stack is empty");
    }

    const widget_internal::list_state state = widget_internal::list_stack.back();
    widget_internal::list_stack.pop_back();

    widget_internal::layout = state.Parent;
    widget_internal::layout.LastItem = state.Bounds;
    widget_internal::layout.HasLastItem = true;
    widget_internal::layout.LineBottom = state.Bounds.Max.Y;
    widget_internal::set_last_item(state.Id, state.Hovered, false, widget_internal::focused_item == state.Id);
}

bool ListItem(std::string_view label, bool selected)
{
    widget_internal::ensure_layout();

    const Style& theme = CurrentStyle();
    const float scale = theme.FrameScale;
    const float height = 26.0F * scale;
    const Rect bounds = widget_internal::next_rect(widget_internal::layout.ContentWidth > 0.0F ? widget_internal::layout.ContentWidth : theme.ItemWidth * scale, height);
    const std::uint64_t id = CurrentId(label);
    const widget_internal::item_result item = widget_internal::item_behavior(id, bounds);
    const bool activated = item.Clicked || widget_internal::keyboard_toggle(id);

    const Color fill = item.Active
        ? theme.ButtonActive
        : selected ? Color::RgbaF(theme.Selection.R, theme.Selection.G, theme.Selection.B, item.Hovered ? 0.68F : 0.48F)
                   : item.Hovered ? Color::RgbaF(theme.ButtonHovered.R, theme.ButtonHovered.G, theme.ButtonHovered.B, 0.88F)
                                  : Color::RgbaF(theme.Button.R, theme.Button.G, theme.Button.B, 0.0F);
    const Color text_color = selected || item.Hovered || item.Active ? theme.Text : theme.TextSecondary;

    if (selected || item.Hovered || item.Active) {
        widget_internal::add_filled_rect(bounds, fill);
    }

    if (item.Focused) {
        widget_internal::add_soft_outline(bounds, 0.0F, theme.Accent);
    }

    widget_internal::add_text({{bounds.Min.X + 8.0F * scale, bounds.Min.Y}, bounds.Max}, label, text_color);

    return activated;
}

}
