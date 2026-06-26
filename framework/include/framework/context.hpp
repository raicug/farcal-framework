#pragma once

// clang-format off
#include <framework/draw.hpp>
#include <framework/input.hpp>
#include <framework/style.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string_view>
// clang-format on

namespace farcal {

struct context_config {
};

struct statistics {
    double delta_seconds {};
    double frame_seconds {};
    double frames_per_second {};
    std::uint64_t frame_index {};
    std::size_t draw_command_count {};
    std::size_t background_command_count {};
    std::size_t main_command_count {};
    std::size_t foreground_command_count {};
    std::size_t memory_working_set {};
    std::size_t memory_private {};
};

void create_context(const context_config& config = {});
void destroy_context();
bool has_context();
void begin_frame(const input_state& input = {});
void end_frame();
void set_max_fps(float value);
float max_fps();
void limit_frame_rate();
const input_state& input();
const draw_data& draw();
const statistics& stats();
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
