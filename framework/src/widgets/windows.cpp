#include <framework/internal/widgets.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>

namespace farcal {

bool BeginWindow(std::string_view Title)
{
    widget_internal::ensure_layout();
    const Style& theme = CurrentStyle();
    const float scale = theme.FrameScale;
    const float Width = theme.WindowWidth * scale;
    const float title_height = theme.WindowTitleHeight * scale;
    widget_internal::window_state& state = widget_internal::windows[std::string(Title)];
    const float Height = theme.WindowHeight * scale;

    if (!state.Initialized) {
        state.Position = widget_internal::layout.Cursor;
        state.Initialized = true;
    }

    const InputState& io = Input();
    Rect bounds {
        state.Position,
        {state.Position.X + Width, state.Position.Y + Height},
    };
    const Rect title_bounds {bounds.Min, {bounds.Max.X, bounds.Min.Y + title_height}};
    const bool Hovered = widget_internal::contains(bounds, io.MousePosition);
    const bool title_hovered = widget_internal::contains(title_bounds, io.MousePosition);

    if (title_hovered && io.MousePressed[0]) {
        state.Dragging = true;
        state.DragOffset = {
            io.MousePosition.X - state.Position.X,
            io.MousePosition.Y - state.Position.Y,
        };
    }

    if (!io.MouseDown[0]) {
        state.Dragging = false;
    }

    if (state.Dragging) {
        state.Position = {
            io.MousePosition.X - state.DragOffset.X,
            io.MousePosition.Y - state.DragOffset.Y,
        };

        bounds = {
            state.Position,
            {state.Position.X + Width, state.Position.Y + Height},
        };
    }

    widget_internal::advance_layout(Height);

    const Rect current_title_bounds {bounds.Min, {bounds.Max.X, bounds.Min.Y + title_height}};
    const Rect content_bounds {{bounds.Min.X + 1.0F, bounds.Min.Y + title_height}, {bounds.Max.X - 1.0F, bounds.Max.Y - 1.0F}};
    const Rect scroll_clip {{bounds.Min.X + 18.0F * scale, bounds.Min.Y + title_height + 16.0F * scale}, {bounds.Max.X - 18.0F * scale, bounds.Max.Y - 16.0F * scale}};
    const Rect shadow_1 {{bounds.Min.X + 3.0F * scale, bounds.Min.Y + 4.0F * scale}, {bounds.Max.X + 3.0F * scale, bounds.Max.Y + 4.0F * scale}};
    const Rect shadow_2 {{bounds.Min.X + 1.0F * scale, bounds.Min.Y + 2.0F * scale}, {bounds.Max.X + 1.0F * scale, bounds.Max.Y + 2.0F * scale}};

    widget_internal::add_soft_rect(shadow_1, 4.0F * scale, {0.0F, 0.0F, 0.0F, 0.18F});
    widget_internal::add_soft_rect(shadow_2, 4.0F * scale, {0.0F, 0.0F, 0.0F, 0.14F});
    widget_internal::add_soft_rect(bounds, 4.0F * scale, theme.WindowBackground);
    widget_internal::add_filled_rect(content_bounds, theme.WindowBackground);
    widget_internal::add_soft_rect(current_title_bounds, 4.0F * scale, Hovered ? theme.WindowTitleActive : theme.WindowTitle);
    widget_internal::add_filled_rect({{current_title_bounds.Min.X, current_title_bounds.Max.Y - 4.0F * scale}, current_title_bounds.Max}, Hovered ? theme.WindowTitleActive : theme.WindowTitle);
    widget_internal::add_hline(bounds.Min.X, bounds.Max.X, current_title_bounds.Max.Y, {theme.WindowBorder.R, theme.WindowBorder.G, theme.WindowBorder.B, 0.70F});
    widget_internal::add_soft_outline(bounds, 4.0F * scale, theme.WindowBorder);
    widget_internal::add_text({{current_title_bounds.Min.X + 10.0F * scale, current_title_bounds.Min.Y}, current_title_bounds.Max}, Title, theme.Text);

    const float VisibleHeight = scroll_clip.Max.Y - scroll_clip.Min.Y;
    const float MaxScroll = std::max(0.0F, state.ContentHeight - VisibleHeight);
    if (Hovered && io.MouseWheel != 0.0F) {
        state.ScrollY = std::clamp(state.ScrollY - io.MouseWheel * 44.0F * scale, 0.0F, MaxScroll);
    }
    state.ScrollY = std::clamp(state.ScrollY, 0.0F, MaxScroll);

    const Vec2 content_cursor {
        scroll_clip.Min.X,
        scroll_clip.Min.Y - state.ScrollY,
    };

    widget_internal::layout_stack.push_back(widget_internal::layout);
    widget_internal::window_stack.push_back({
        .Title = std::string(Title),
        .ContentStartY = content_cursor.Y,
        .ContentPaddingY = 16.0F * scale,
        .VisibleHeight = VisibleHeight,
        .MaxScroll = MaxScroll,
        .Hovered = Hovered,
        .ScrollTrack = {{scroll_clip.Max.X - 7.0F * scale, scroll_clip.Min.Y}, {scroll_clip.Max.X - 3.0F * scale, scroll_clip.Max.Y}},
    });
    widget_internal::layout.Cursor = content_cursor;
    widget_internal::layout.StartX = content_cursor.X;
    widget_internal::layout.ContentWidth = scroll_clip.Max.X - scroll_clip.Min.X - 18.0F * scale;
    widget_internal::layout.Clip = scroll_clip;
    widget_internal::layout.HasClip = true;
    widget_internal::layout.HasLastItem = false;
    widget_internal::layout.LineBottom = content_cursor.Y;
    widget_internal::layout.NextItemWidth = 0.0F;
    widget_internal::layout.IndentWidth = 0.0F;

    return true;
}

void EndWindow()
{
    if (widget_internal::layout_stack.empty() || widget_internal::window_stack.empty()) {
        throw std::logic_error("farcal Window stack is empty");
    }

    const widget_internal::window_layout_state window_layout = widget_internal::window_stack.back();
    widget_internal::window_stack.pop_back();

    widget_internal::window_state& state = widget_internal::windows[window_layout.Title];
    state.ContentHeight = std::max(0.0F, widget_internal::layout.Cursor.Y - window_layout.ContentStartY + window_layout.ContentPaddingY);

    if (window_layout.MaxScroll > 0.0F) {
        const Style& theme = CurrentStyle();
        const Rect track = window_layout.ScrollTrack;
        const float track_height = track.Max.Y - track.Min.Y;
        const float thumb_height = std::max(28.0F * theme.FrameScale, track_height * std::min(1.0F, window_layout.VisibleHeight / state.ContentHeight));
        const float thumb_y = track.Min.Y + (track_height - thumb_height) * (state.ScrollY / window_layout.MaxScroll);
        const Rect thumb {{track.Min.X, thumb_y}, {track.Max.X, thumb_y + thumb_height}};

        widget_internal::add_soft_rect(track, 2.0F * theme.FrameScale, {0.0F, 0.0F, 0.0F, 0.20F});
        widget_internal::add_soft_rect(thumb, 2.0F * theme.FrameScale, {theme.TextMuted.R, theme.TextMuted.G, theme.TextMuted.B, window_layout.Hovered ? 0.70F : 0.42F});
    }

    widget_internal::layout = widget_internal::layout_stack.back();
    widget_internal::layout_stack.pop_back();
}

}
