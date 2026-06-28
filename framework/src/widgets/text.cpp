#include <framework/internal/widgets.hpp>

namespace farcal {

void Text(std::string_view value)
{
    widget_internal::ensure_layout();
    const Style& theme = CurrentStyle();
    const float Height = theme.FontSize * theme.FrameScale * 1.45F;
    const Rect bounds = widget_internal::next_rect(widget_internal::text_bounds_width(value), Height);
    widget_internal::add_text(bounds, value, theme.Text);
    widget_internal::set_last_item(CurrentId(value), widget_internal::contains(widget_internal::active_clip(bounds), Input().MousePosition), false, widget_internal::focused_item == CurrentId(value));
}

void TextSecondary(std::string_view value)
{
    widget_internal::ensure_layout();
    const Style& theme = CurrentStyle();
    const float Height = theme.FontSize * theme.FrameScale * 1.45F;
    const Rect bounds = widget_internal::next_rect(widget_internal::text_bounds_width(value), Height);
    widget_internal::add_text(bounds, value, theme.TextSecondary);
    widget_internal::set_last_item(CurrentId(value), widget_internal::contains(widget_internal::active_clip(bounds), Input().MousePosition), false, widget_internal::focused_item == CurrentId(value));
}

void TitleText(std::string_view value)
{
    widget_internal::ensure_layout();
    PushStyleVar(StyleVar::FontSize, 18.0F);
    const Style& title_theme = CurrentStyle();
    const float Height = title_theme.FontSize * title_theme.FrameScale * 1.35F;
    const Rect bounds = widget_internal::next_rect(widget_internal::text_bounds_width(value), Height);
    widget_internal::add_text(bounds, value, title_theme.Text);
    PopStyleVar();
    widget_internal::set_last_item(CurrentId(value), widget_internal::contains(widget_internal::active_clip(bounds), Input().MousePosition), false, widget_internal::focused_item == CurrentId(value));
}

void SectionText(std::string_view value)
{
    widget_internal::ensure_layout();
    const Style& theme = CurrentStyle();
    widget_internal::layout.Cursor.Y += 2.0F * theme.FrameScale;
    const Rect bounds = widget_internal::next_rect(widget_internal::text_width(value), 20.0F * theme.FrameScale);
    widget_internal::add_text(bounds, value, theme.TextSecondary);
    widget_internal::add_hline(widget_internal::layout.Cursor.X, widget_internal::layout.Cursor.X + widget_internal::layout.ContentWidth, bounds.Max.Y, {theme.WindowBorder.R, theme.WindowBorder.G, theme.WindowBorder.B, 0.48F});
    widget_internal::layout.Cursor.Y += 2.0F * theme.FrameScale;
    widget_internal::set_last_item(CurrentId(value), widget_internal::contains(widget_internal::active_clip(bounds), Input().MousePosition), false, widget_internal::focused_item == CurrentId(value));
}

}
