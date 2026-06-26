// clang-format off
#include <framework/framework.hpp>

#include <cstdio>
#include <exception>
// clang-format on

int main()
{
    try {
        farcal::CreateContext();

        farcal::Window Window({
            .Title = L"Farcal DX11 Test",
            .Width = 1280,
            .Height = 720,
        });

        Window.SetWndProcHook([](HWND, UINT message, WPARAM, LPARAM) -> std::optional<LRESULT> {
            if (message == WM_SETCURSOR) {
                return std::nullopt;
            }

            return std::nullopt;
        });

        farcal::Dx11Renderer renderer(Window);
        bool clicked = false;
        bool compiling = false;
        bool vsync = true;
        bool diagnostics = false;
        float exposure = 1.25F;
        float gamma = 2.20F;
        float MaximumFps = 144.0F;
        farcal::SetMaxFps(MaximumFps);

        while (Window.PollEvents()) {
            farcal::BeginFrame(Window.ConsumeInput());

            farcal::BackgroundRenderer().Commands.push_back({
                .Type = farcal::DrawCommandType::FilledRect,
                .Bounds = {{0.0F, 0.0F}, {static_cast<float>(Window.Width()), static_cast<float>(Window.Height())}},
                .Tint = {0.055F, 0.055F, 0.064F, 1.0F},
            });

            farcal::Frame([&] {
                farcal::WindowPanel("Farcal Framework", [&] {
                    const farcal::Statistics& Stats = farcal::Stats();
                    char FpsText[64] {};
                    char FrameText[64] {};
                    char DrawText[64] {};
                    char MemoryText[64] {};
                    std::snprintf(FpsText, sizeof(FpsText), "UI FPS %.1f", Stats.FramesPerSecond);
                    std::snprintf(FrameText, sizeof(FrameText), "UI frame %.2f ms", Stats.FrameSeconds * 1000.0);
                    std::snprintf(DrawText, sizeof(DrawText), "Draw commands %zu", Stats.DrawCommandCount);
                    std::snprintf(MemoryText, sizeof(MemoryText), "Memory %.1f MB", static_cast<double>(Stats.MemoryWorkingSet) / (1024.0 * 1024.0));

                    farcal::TitleText("Interface Preview");
                    farcal::TextSecondary("Dark immediate-mode controls with clipped scrolling.");
                    farcal::Separator();

                    farcal::SectionText("Main");
                    farcal::PrimaryButton(clicked ? "Enabled" : "Enable Feature", [&] {
                        clicked = !clicked;
                    });

                    farcal::Button(compiling ? "Queued" : "Queue Task", [&] {
                        compiling = !compiling;
                    });

                    farcal::PushStyleVar(farcal::StyleVar::AntiAliasing, 1.35F);
                    farcal::Checkbox("VSync", &vsync);
                    farcal::Checkbox("Diagnostics Overlay", &diagnostics);
                    farcal::PopStyleVar();

                    farcal::SliderFloat("Exposure", &exposure, 0.0F, 5.0F);
                    farcal::SliderFloat("Gamma", &gamma, 1.0F, 3.0F);
                    if (farcal::SliderFloat("Max FPS", &MaximumFps, 0.0F, 240.0F)) {
                        farcal::SetMaxFps(MaximumFps);
                    }

                    farcal::Spacing();
                    farcal::SectionText("Status");

                    farcal::PushStyleColor(farcal::StyleColor::Text, {0.322F, 0.824F, 0.451F, 1.0F});
                    farcal::Text(FpsText);
                    farcal::PopStyleColor();

                    farcal::PushStyleColor(farcal::StyleColor::Text, {0.722F, 0.753F, 0.800F, 1.0F});
                    farcal::Text(FrameText);
                    farcal::Text(DrawText);
                    farcal::Text(MemoryText);
                    farcal::PopStyleColor();

                    farcal::Button("Stop Polling Next Frame", [&] {
                        Window.CancelNextPoll();
                    });

                    farcal::Spacing();
                    farcal::SectionText("Core Layout");
                    farcal::Group([] {
                        farcal::SetNextItemWidth(104.0F);
                        farcal::Button("Left");
                        farcal::SameLine();
                        farcal::SetNextItemWidth(104.0F);
                        farcal::Button("Right");
                    });

                    farcal::Indent();
                    farcal::TextSecondary(farcal::IsItemHovered() ? "Group hovered" : "Group idle");
                    farcal::Unindent();
                    farcal::Dummy({1.0F, 6.0F});

                    farcal::Spacing();
                    farcal::SectionText("Scrollable Assets");

                    for (int index = 0; index < 24; ++index) {
                        char label[64] {};
                        std::snprintf(label, sizeof(label), "SM_TestAsset_%02d", index + 1);

                        if (index == 3 || index == 11 || index == 17) {
                            farcal::PushStyleColor(farcal::StyleColor::Text, {0.910F, 0.698F, 0.298F, 1.0F});
                            farcal::Text(label);
                            farcal::PopStyleColor();
                        } else {
                            farcal::TextSecondary(label);
                        }
                    }
                });
            });

            farcal::ForegroundRenderer().Commands.push_back({
                .Type = farcal::DrawCommandType::Rect,
                .Bounds = {{8.0F, 8.0F}, {static_cast<float>(Window.Width()) - 8.0F, static_cast<float>(Window.Height()) - 8.0F}},
                .Tint = {1.0F, 1.0F, 1.0F, 0.035F},
            });

            farcal::EndFrame();
            renderer.Render(farcal::Draw());
            renderer.Present(vsync);
            farcal::LimitFrameRate();
        }

        farcal::DestroyContext();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        farcal::DestroyContext();
        return 1;
    }

    return 0;
}
