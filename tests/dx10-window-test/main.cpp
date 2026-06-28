// clang-format off
#include <framework/framework.hpp>

#include "../component_demo.hpp"

#include <cstdio>
#include <exception>
// clang-format on

int main() {
  try {
    farcal::CreateContext();

    farcal::Window Window({
        .Title = L"Farcal DX10 Test",
        .Width = 1280,
        .Height = 720,
    });

    Window.SetWndProcHook(
        [](HWND, UINT message, WPARAM, LPARAM) -> std::optional<LRESULT> {
          if (message == WM_SETCURSOR) {
            return std::nullopt;
          }

          return std::nullopt;
        });

    farcal::Dx10Renderer renderer(Window);
    if (!renderer.Valid()) {
      std::fprintf(stderr, "failed to initialize DX10 renderer\n");
      farcal::DestroyContext();
      return 1;
    }

    farcal::tests::ComponentDemoState demo;
    farcal::SetMaxFps(demo.MaximumFps);

    while (Window.PollEvents()) {
      farcal::BeginFrame(Window.ConsumeInput());

      farcal::BackgroundRenderer().Commands.push_back({
          .Type = farcal::DrawCommandType::FilledRect,
          .Bounds = {{0.0F, 0.0F},
                     {static_cast<float>(Window.Width()),
                      static_cast<float>(Window.Height())}},
          .Tint = {0.055F, 0.055F, 0.064F, 1.0F},
      });

      farcal::tests::RenderComponentDemo(demo, Window, "DirectX 10", "DX10");

      farcal::ForegroundRenderer().Commands.push_back({
          .Type = farcal::DrawCommandType::Rect,
          .Bounds = {{8.0F, 8.0F},
                     {static_cast<float>(Window.Width()) - 8.0F,
                      static_cast<float>(Window.Height()) - 8.0F}},
          .Tint = {1.0F, 1.0F, 1.0F, 0.035F},
      });

      farcal::EndFrame();
      renderer.Render(farcal::Draw());
      renderer.Present(demo.Vsync);
      farcal::LimitFrameRate();
    }

    farcal::DestroyContext();
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    farcal::DestroyContext();
    return 1;
  }

  return 0;
}
