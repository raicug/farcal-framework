#pragma once

// clang-format off
#include <framework/draw.hpp>
#include <framework/input.hpp>
#include <framework/style.hpp>

#include <concepts>
#include <cstdint>
#include <string_view>
// clang-format on

namespace farcal {

struct context_config {
};

void create_context(const context_config& config = {});
void destroy_context();
bool has_context();
void begin_frame(const input_state& input = {});
void end_frame();
const input_state& input();
const draw_data& draw();
draw_data& background_renderer();
draw_data& foreground_renderer();
draw_data& main_renderer();
void push_id(std::string_view value);
void push_id(const void* value);
void pop_id();
std::uint64_t current_id(std::string_view value);
std::uint64_t frame_index();

template <std::invocable Callback>
void frame(Callback&& callback)
{
    callback();
}

}
