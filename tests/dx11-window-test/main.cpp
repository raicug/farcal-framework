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
            .title = L"Farcal DX11 Test",
            .width = 1280,
            .height = 720,
        });

        window.set_wnd_proc_hook([](HWND, UINT message, WPARAM, LPARAM) -> std::optional<LRESULT> {
            if (message == WM_SETCURSOR) {
                return std::nullopt;
            }

            return std::nullopt;
        });

        farcal::dx11_renderer renderer(window);
        bool clicked = false;
        bool compiling = false;
        bool vsync = true;
        bool diagnostics = false;
        float exposure = 1.25F;
        float gamma = 2.20F;
        float maximum_fps = 144.0F;
        farcal::set_max_fps(maximum_fps);

        while (window.poll_events()) {
            farcal::begin_frame(window.consume_input());

            farcal::background_renderer().commands.push_back({
                .type = farcal::draw_command_type::filled_rect,
                .bounds = {{0.0F, 0.0F}, {static_cast<float>(window.width()), static_cast<float>(window.height())}},
                .tint = {0.055F, 0.055F, 0.064F, 1.0F},
            });

            farcal::frame([&] {
                farcal::window_panel("Farcal Framework", [&] {
                    const farcal::statistics& stats = farcal::stats();
                    char fps_text[64] {};
                    char frame_text[64] {};
                    char draw_text[64] {};
                    char memory_text[64] {};
                    std::snprintf(fps_text, sizeof(fps_text), "UI FPS %.1f", stats.frames_per_second);
                    std::snprintf(frame_text, sizeof(frame_text), "UI frame %.2f ms", stats.frame_seconds * 1000.0);
                    std::snprintf(draw_text, sizeof(draw_text), "Draw commands %zu", stats.draw_command_count);
                    std::snprintf(memory_text, sizeof(memory_text), "Memory %.1f MB", static_cast<double>(stats.memory_working_set) / (1024.0 * 1024.0));

                    farcal::title_text("Interface Preview");
                    farcal::text_secondary("Dark immediate-mode controls with clipped scrolling.");
                    farcal::separator();

                    farcal::section_text("Main");
                    farcal::primary_button(clicked ? "Enabled" : "Enable Feature", [&] {
                        clicked = !clicked;
                    });

                    farcal::button(compiling ? "Queued" : "Queue Task", [&] {
                        compiling = !compiling;
                    });

                    farcal::push_style_var(farcal::style_var::anti_aliasing, 1.35F);
                    farcal::checkbox("VSync", &vsync);
                    farcal::checkbox("Diagnostics Overlay", &diagnostics);
                    farcal::pop_style_var();

                    farcal::slider_float("Exposure", &exposure, 0.0F, 5.0F);
                    farcal::slider_float("Gamma", &gamma, 1.0F, 3.0F);
                    if (farcal::slider_float("Max FPS", &maximum_fps, 0.0F, 240.0F)) {
                        farcal::set_max_fps(maximum_fps);
                    }

                    farcal::spacing();
                    farcal::section_text("Status");

                    farcal::push_style_color(farcal::style_color::text, {0.322F, 0.824F, 0.451F, 1.0F});
                    farcal::text(fps_text);
                    farcal::pop_style_color();

                    farcal::push_style_color(farcal::style_color::text, {0.722F, 0.753F, 0.800F, 1.0F});
                    farcal::text(frame_text);
                    farcal::text(draw_text);
                    farcal::text(memory_text);
                    farcal::pop_style_color();

                    farcal::button("Stop Polling Next Frame", [&] {
                        window.cancel_next_poll();
                    });

                    farcal::spacing();
                    farcal::section_text("Core Layout");
                    farcal::group([] {
                        farcal::set_next_item_width(104.0F);
                        farcal::button("Left");
                        farcal::same_line();
                        farcal::set_next_item_width(104.0F);
                        farcal::button("Right");
                    });

                    farcal::indent();
                    farcal::text_secondary(farcal::is_item_hovered() ? "group hovered" : "group idle");
                    farcal::unindent();
                    farcal::dummy({1.0F, 6.0F});

                    farcal::spacing();
                    farcal::section_text("Scrollable Assets");

                    for (int index = 0; index < 24; ++index) {
                        char label[64] {};
                        std::snprintf(label, sizeof(label), "SM_TestAsset_%02d", index + 1);

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
            renderer.present(vsync);
            farcal::limit_frame_rate();
        }

        farcal::destroy_context();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        farcal::destroy_context();
        return 1;
    }

    return 0;
}
