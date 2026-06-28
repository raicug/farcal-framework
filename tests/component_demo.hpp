#pragma once

// clang-format off
#include <framework/framework.hpp>

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
    float Exposure {1.25F};
    float Gamma {2.20F};
    float MaximumFps {144.0F};
    float FrameScale {1.0F};
    float WindowHeight {412.0F};
    int SampleCount {32};
    int SelectedAsset {};
    int DirectClicks {};
    int CallbackClicks {};
};

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

    Frame([&] {
        WindowPanel("Farcal Component Test", [&] {
            TitleText("Framework Components");
            TextSecondary("Shared smoke window covering the public immediate-mode controls.");
            Separator();

            SectionText("Backend");
            Text(backendName);
            TextSecondary(Version());

            Spacing();
            SectionText("Buttons");
            PrimaryButton(state.PrimaryEnabled ? "Primary Enabled" : "Enable Primary", [&] {
                state.PrimaryEnabled = !state.PrimaryEnabled;
                ++state.CallbackClicks;
            });
            TextSecondary(IsItemActive() ? "Primary active" : IsItemHovered() ? "Primary hovered" : IsItemFocused() ? "Primary focused" : "Primary idle");

            if (Button(state.TaskQueued ? "Task Queued" : "Queue Task")) {
                state.TaskQueued = !state.TaskQueued;
                ++state.DirectClicks;
            }
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
            SectionText("Sliders");
            Slider<float>("Exposure", &state.Exposure, 0.0F, 5.0F);
            Slider<float>("Gamma", &state.Gamma, 1.0F, 3.0F);
            Slider<int>("Samples", &state.SampleCount, 1, 128);
            if (Slider<float>("Max FPS", &state.MaximumFps, 0.0F, 240.0F)) {
                SetMaxFps(state.MaximumFps);
            }
            Slider<float>("Frame Scale", &state.FrameScale, 0.80F, 1.35F);
            Slider<float>("Window Height", &state.WindowHeight, 320.0F, 560.0F);

            Spacing();
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

            Spacing();
            SectionText("Layout Primitives");
            Dummy({1.0F, 6.0F});
            BeginGroup();
            TextSecondary("Manual group");
            Indent(12.0F);
            TextSecondary("Indented text");
            Unindent(12.0F);
            EndGroup();

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
                    PopId();
                }
            }, {0.0F, 224.0F * state.FrameScale});

            PushId("asset-list");
            Spacing();
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
}

}
