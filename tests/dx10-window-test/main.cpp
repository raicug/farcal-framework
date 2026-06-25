// clang-format off
#include <framework/framework.hpp>

#include <cstdio>
#include <exception>
// clang-format on

int main()
{
    try {
        farcal::create_context();

        farcal::window window({
            .title = L"Farcal DX10 Test",
            .width = 1280,
            .height = 720,
        });

        window.set_wnd_proc_hook([](HWND, UINT message, WPARAM, LPARAM) -> std::optional<LRESULT> {
            if (message == WM_SETCURSOR) {
                return std::nullopt;
            }

            return std::nullopt;
        });

        farcal::dx10_renderer renderer(window);
        if (!renderer.valid()) {
            std::fprintf(stderr, "failed to initialize DX10 renderer\n");
            farcal::destroy_context();
            return 1;
        }

        bool clicked = false;
        bool compiling = false;

        while (window.poll_events()) {
            farcal::begin_frame(window.consume_input());

            farcal::background_renderer().commands.push_back({
                .type = farcal::draw_command_type::filled_rect,
                .bounds = {{0.0F, 0.0F}, {static_cast<float>(window.width()), static_cast<float>(window.height())}},
                .tint = {0.055F, 0.055F, 0.064F, 1.0F},
            });

            farcal::frame([&] {
                farcal::window_panel("Farcal Framework", [&] {
                    farcal::title_text("DX10 Backend Preview");
                    farcal::text_secondary("Same immediate-mode UI rendered through DirectX 10.");
                    farcal::separator();

                    farcal::section_text("Main");
                    farcal::primary_button(clicked ? "Enabled" : "Enable Feature", [&] {
                        clicked = !clicked;
                    });

                    farcal::button(compiling ? "Queued" : "Queue Task", [&] {
                        compiling = !compiling;
                    });

                    farcal::spacing();
                    farcal::section_text("Status");

                    farcal::push_style_color(farcal::style_color::text, {0.322F, 0.824F, 0.451F, 1.0F});
                    farcal::text("FPS 144.0");
                    farcal::pop_style_color();

                    farcal::push_style_color(farcal::style_color::text, {0.722F, 0.753F, 0.800F, 1.0F});
                    farcal::text("Memory 412 MB");
                    farcal::pop_style_color();

                    farcal::button("Stop Polling Next Frame", [&] {
                        window.cancel_next_poll();
                    });

                    farcal::spacing();
                    farcal::section_text("Scrollable Assets");

                    for (int index = 0; index < 24; ++index) {
                        char label[64] {};
                        std::snprintf(label, sizeof(label), "DX10_TestAsset_%02d", index + 1);

                        if (index == 3 || index == 11 || index == 17) {
                            farcal::push_style_color(farcal::style_color::text, {0.910F, 0.698F, 0.298F, 1.0F});
                            farcal::text(label);
                            farcal::pop_style_color();
                        } else {
                            farcal::text_secondary(label);
                        }
                    }
                });
            });

            farcal::foreground_renderer().commands.push_back({
                .type = farcal::draw_command_type::rect,
                .bounds = {{8.0F, 8.0F}, {static_cast<float>(window.width()) - 8.0F, static_cast<float>(window.height()) - 8.0F}},
                .tint = {1.0F, 1.0F, 1.0F, 0.035F},
            });

            farcal::end_frame();
            renderer.render(farcal::draw());
            renderer.present();
        }

        farcal::destroy_context();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        farcal::destroy_context();
        return 1;
    }

    return 0;
}
