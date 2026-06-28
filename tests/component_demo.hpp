#pragma once

// clang-format off
#include <framework/framework.hpp>

#include <algorithm>
#include <cstdio>
#include <string_view>
// clang-format on

namespace farcal::tests {

struct ComponentDemoState {
    bool PrimaryEnabled {};
    bool TaskQueued {};
    bool Vsync {true};
    bool Diagnostics {};
    bool CompactMode {};
    bool DrawLayerTest {true};
    float Exposure {1.25F};
    float Gamma {2.20F};
    float MaximumFps {144.0F};
    float FrameScale {1.0F};
    float WindowHeight {412.0F};
    int SampleCount {32};
    int SelectedAsset {};
    int RenderMode {};
    int ComponentTab {};
    int DirectClicks {};
    int CallbackClicks {};
    int ToggleMenuKey {45};
    KeybindMode ToggleMenuMode {KeybindMode::Toggle};
    bool ToggleMenuEnabled {};
    bool ToggleMenuWasDown {};
    Color AccentPreview {Color::Rgb(92, 139, 255)};
    char SearchText[64] {};
};

inline bool IsDemoBindingDown(int binding)
{
    const InputState& input = Input();
    if (binding <= 0) {
        return false;
    }

    if (binding >= 256) {
        const int mouse = binding - 256;
        return mouse >= 0 && mouse < static_cast<int>(input.MouseDown.size()) && input.MouseDown[static_cast<std::size_t>(mouse)];
    }

    return binding < static_cast<int>(input.KeyDown.size()) && input.KeyDown[static_cast<std::size_t>(binding)];
}

inline void UpdateDemoKeybindLog(ComponentDemoState& state)
{
    if (IsTextInputFocused()) {
        state.ToggleMenuWasDown = false;
        return;
    }

    const bool down = IsDemoBindingDown(state.ToggleMenuKey);

    if (state.ToggleMenuMode == KeybindMode::Toggle) {
        if (down && !state.ToggleMenuWasDown) {
            state.ToggleMenuEnabled = !state.ToggleMenuEnabled;
            std::printf("Toggle Menu %s\n", state.ToggleMenuEnabled ? "enabled" : "disabled");
        }
    } else {
        if (down) {
            state.ToggleMenuEnabled = true;
            std::printf("Toggle Menu enabled\n");
        } else if (state.ToggleMenuEnabled) {
            state.ToggleMenuEnabled = false;
            std::printf("Toggle Menu disabled\n");
        }
    }

    state.ToggleMenuWasDown = down;
}

inline void RenderDrawLayerTest(const Window& window)
{
    const Style& style = CurrentStyle();
    const float scale = style.FrameScale;
    const Rect bounds {
        {24.0F * scale, static_cast<float>(window.Height()) - 158.0F * scale},
        {268.0F * scale, static_cast<float>(window.Height()) - 24.0F * scale},
    };
    const Rect background {
        {bounds.Min.X + 10.0F * scale, bounds.Min.Y + 14.0F * scale},
        {bounds.Min.X + 154.0F * scale, bounds.Min.Y + 86.0F * scale},
    };
    const Rect main {
        {bounds.Min.X + 44.0F * scale, bounds.Min.Y + 38.0F * scale},
        {bounds.Min.X + 188.0F * scale, bounds.Min.Y + 110.0F * scale},
    };
    const Rect foreground {
        {bounds.Min.X + 80.0F * scale, bounds.Min.Y + 20.0F * scale},
        {bounds.Min.X + 222.0F * scale, bounds.Min.Y + 96.0F * scale},
    };

    BackgroundRenderer().Commands.push_back({
        .Type = DrawCommandType::FilledRect,
        .Bounds = background,
        .Tint = Color::Rgba(232, 78, 88, 150),
    });
    MainRenderer().Commands.push_back({
        .Type = DrawCommandType::FilledRect,
        .Bounds = main,
        .Tint = Color::Rgba(82, 210, 115, 170),
    });
    ForegroundRenderer().Commands.push_back({
        .Type = DrawCommandType::Rect,
        .Bounds = foreground,
        .Tint = Color::Rgba(92, 139, 255, 230),
        .Thickness = 2.0F * scale,
    });
    ForegroundRenderer().Commands.push_back({
        .Type = DrawCommandType::Text,
        .Bounds = {{foreground.Min.X + 8.0F * scale, foreground.Min.Y}, foreground.Max},
        .Tint = Color::Rgb(232, 236, 244),
        .FontSize = 13.0F * scale,
        .Text = "Foreground layer",
    });
}

inline void RenderComponentDemo(ComponentDemoState& state, Window& window, std::string_view backendName, std::string_view assetPrefix)
{
    const Statistics& stats = Stats();
    char fpsText[64] {};
    char frameText[64] {};
    char drawText[64] {};
    char memoryText[64] {};
    char directClickText[64] {};
    char callbackClickText[64] {};
    char hoverText[96] {};
    std::snprintf(fpsText, sizeof(fpsText), "UI FPS %.1f", stats.FramesPerSecond);
    std::snprintf(frameText, sizeof(frameText), "UI frame %.2f ms", stats.FrameSeconds * 1000.0);
    std::snprintf(drawText, sizeof(drawText), "Draw commands %zu", stats.DrawCommandCount);
    std::snprintf(memoryText, sizeof(memoryText), "Memory %.1f MB", static_cast<double>(stats.MemoryWorkingSet) / (1024.0 * 1024.0));
    std::snprintf(directClickText, sizeof(directClickText), "Direct button clicks %d", state.DirectClicks);
    std::snprintf(callbackClickText, sizeof(callbackClickText), "Callback clicks %d", state.CallbackClicks);

    Style style = CurrentStyle();
    style.FrameScale = state.FrameScale;
    style.WindowHeight = state.WindowHeight;
    style.ItemSpacingY = state.CompactMode ? 5.0F : 8.0F;
    SetStyle(style);
    UpdateDemoKeybindLog(state);

    Frame([&] {
        WindowPanel("Farcal Component Test", [&] {
            TitleText("Framework Components");
            TextSecondary("Shared smoke window covering the public immediate-mode controls.");
            Separator();

            SectionText("Backend");
            Text(backendName);
            TextSecondary(Version());

            Spacing();
            const char* componentTabs[] {"Controls", "Style", "Layout", "Assets"};
            if (BeginTabs("Component Tabs", &state.ComponentTab, componentTabs)) {
                if (state.ComponentTab == 0) {
                    SectionText("Buttons");
                    PrimaryButton(state.PrimaryEnabled ? "Primary Enabled" : "Enable Primary", [&] {
                        state.PrimaryEnabled = !state.PrimaryEnabled;
                        ++state.CallbackClicks;
                    });
                    SetTooltip("Toggles the primary action state and increments callback clicks.");
                    TextSecondary(IsItemActive() ? "Primary active" : IsItemHovered() ? "Primary hovered" : IsItemFocused() ? "Primary focused" : "Primary idle");

                    if (Button(state.TaskQueued ? "Task Queued" : "Queue Task")) {
                        state.TaskQueued = !state.TaskQueued;
                        ++state.DirectClicks;
                    }
                    SetTooltip("Queues or clears a sample task using the direct button API.");
                    TextSecondary(IsItemActive() ? "Button active" : IsItemHovered() ? "Button hovered" : IsItemFocused() ? "Button focused" : "Button idle");

                    Group([&] {
                        SetNextItemWidth(104.0F);
                        Button("Left", [&] {
                            ++state.CallbackClicks;
                        });
                        SameLine();
                        SetNextItemWidth(104.0F);
                        Button("Right", [&] {
                            ++state.CallbackClicks;
                        });
                    });
                    std::snprintf(hoverText, sizeof(hoverText), "Group is %s", IsItemHovered() ? "hovered" : "idle");
                    Indent();
                    TextSecondary(hoverText);
                    Unindent();

                    Spacing();
                    SectionText("Toggles");
                    PushStyleVar(StyleVar::AntiAliasing, state.Diagnostics ? 1.65F : 1.0F);
                    Checkbox("VSync", &state.Vsync);
                    Checkbox("Diagnostics Overlay", &state.Diagnostics);
                    Checkbox("Compact Layout", &state.CompactMode);
                    PopStyleVar();

                    Spacing();
                    SectionText("Inputs");
                    const char* renderModes[] {"Balanced", "Quality", "Performance", "Diagnostics"};
                    Dropdown("Render Mode", &state.RenderMode, renderModes);
                    SetTooltip("Selects the demo render quality preset.");
                    InputText("Search assets", state.SearchText, sizeof(state.SearchText));
                    SetTooltip("Typing here suppresses demo keybind logging.");
                    Keybind("Toggle Menu", &state.ToggleMenuKey, &state.ToggleMenuMode);
                    SetTooltip("Click to capture a key. Right-click to choose Toggle or Hold.");

                    Spacing();
                    SectionText("Sliders");
                    Slider<float>("Exposure", &state.Exposure, 0.0F, 5.0F, "ev");
                    Slider<float>("Gamma", &state.Gamma, 1.0F, 3.0F);
                    Slider<int>("Samples", &state.SampleCount, 1, 128, "x");
                    if (Slider<float>("Max FPS", &state.MaximumFps, 0.0F, 240.0F, "fps")) {
                        SetMaxFps(state.MaximumFps);
                    }
                    Slider<float>("Frame Scale", &state.FrameScale, 0.80F, 1.35F);
                    Slider<float>("Window Height", &state.WindowHeight, 320.0F, 560.0F, "px");
                } else if (state.ComponentTab == 1) {
                    SectionText("Style Stack");
                    PushStyleColor(StyleColor::Text, state.PrimaryEnabled ? Color::Rgb(82, 210, 115) : Color::Rgb(232, 178, 76));
                    Text(state.PrimaryEnabled ? "Primary feature is enabled" : "Primary feature is disabled");
                    PopStyleColor();

                    PushStyleColor(StyleColor::Button, Color::Rgb(32, 18, 17));
                    PushStyleColor(StyleColor::ButtonHovered, Color::Rgb(48, 23, 20));
                    PushStyleColor(StyleColor::ButtonActive, Color::Rgb(62, 27, 23));
                    Button("Styled Button", [&] {
                        ++state.CallbackClicks;
                    });
                    PopStyleColor();
                    PopStyleColor();
                    PopStyleColor();

                    Spacing();
                    SectionText("Color Edit");
                    ColorEdit("Accent Preview", &state.AccentPreview);
                    SetTooltip("Opens a draggable color picker with RGBA byte fields.");
                    PushStyleColor(StyleColor::ButtonPrimary, state.AccentPreview);
                    PushStyleColor(StyleColor::ButtonPrimaryHovered, Color::Rgba(
                        (std::min)(255, static_cast<int>(state.AccentPreview.R * 255.0F) + 24),
                        (std::min)(255, static_cast<int>(state.AccentPreview.G * 255.0F) + 24),
                        (std::min)(255, static_cast<int>(state.AccentPreview.B * 255.0F) + 24)));
                    PushStyleColor(StyleColor::ButtonPrimaryActive, Color::Rgba(
                        (std::max)(0, static_cast<int>(state.AccentPreview.R * 255.0F) - 24),
                        (std::max)(0, static_cast<int>(state.AccentPreview.G * 255.0F) - 24),
                        (std::max)(0, static_cast<int>(state.AccentPreview.B * 255.0F) - 24)));
                    PrimaryButton("Preview Color", [&] {
                        ++state.CallbackClicks;
                    });
                    PopStyleColor();
                    PopStyleColor();
                    PopStyleColor();

                    Spacing();
                    SectionText("Status");
                    PushStyleColor(StyleColor::Text, Color::Rgb(82, 210, 115));
                    Text(fpsText);
                    PopStyleColor();

                    PushStyleColor(StyleColor::Text, Color::Rgb(184, 192, 204));
                    Text(frameText);
                    Text(drawText);
                    Text(memoryText);
                    Text(directClickText);
                    Text(callbackClickText);
                    PopStyleColor();

                    Button("Stop Polling Next Frame", [&] {
                        window.CancelNextPoll();
                    });
                } else if (state.ComponentTab == 2) {
                    SectionText("Layout Primitives");
                    Dummy({1.0F, 6.0F});
                    BeginGroup();
                    TextSecondary("Manual group");
                    Indent(12.0F);
                    TextSecondary("Indented text");
                    Unindent(12.0F);
                    EndGroup();

                    Spacing();
                    SectionText("Draw Layers");
                    Checkbox("Show Draw Layer Test", &state.DrawLayerTest);
                    SetTooltip("Shows a viewport overlay that verifies background, main, and foreground ordering.");
                    TextSecondary("Bottom-left overlay: background red, main green, foreground blue.");

                    Spacing();
                    SectionText("List");
                    List("Asset List", [&] {
                        for (int index = 0; index < 8; ++index) {
                            char label[64] {};
                            std::snprintf(label, sizeof(label), "%.*s_ListItem_%02d", static_cast<int>(assetPrefix.size()), assetPrefix.data(), index + 1);

                            PushId(&index);
                            if (ListItem(label, state.SelectedAsset == index)) {
                                state.SelectedAsset = index;
                            }
                            SetTooltip("Selects this generated asset row.");
                            PopId();
                        }
                    }, {0.0F, 224.0F * state.FrameScale});
                } else {
                    PushId("asset-list");
                    SectionText("Scrollable Assets");
                    for (int index = 0; index < 24; ++index) {
                        PushId(&index);

                        char label[64] {};
                        std::snprintf(label, sizeof(label), "%.*s_TestAsset_%02d", static_cast<int>(assetPrefix.size()), assetPrefix.data(), index + 1);

                        if (index == 3 || index == 11 || index == 17) {
                            PushStyleColor(StyleColor::Text, Color::Rgb(232, 178, 76));
                            Text(label);
                            PopStyleColor();
                        } else {
                            TextSecondary(label);
                        }

                        PopId();
                    }
                    PopId();
                }

                EndTabs();
            }
        });

        SameLine();
        if (BeginWindow("Direct BeginWindow API")) {
            SectionText("Manual Panel");
            TextSecondary("This panel uses BeginWindow and EndWindow directly.");
            Separator();
            Text(state.Diagnostics ? "Diagnostics enabled" : "Diagnostics disabled");
            Text(state.Vsync ? "Presentation waits for VSync" : "Presentation presents immediately");
        }
        EndWindow();
    });

    if (state.DrawLayerTest) {
        RenderDrawLayerTest(window);
    }
}

}
