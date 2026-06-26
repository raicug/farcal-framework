// clang-format off
#include <framework/context.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
// clang-format on

namespace farcal {
namespace {

struct context {
    input_state input {};
    draw_data background_draw {};
    draw_data main_draw {};
    draw_data foreground_draw {};
    draw_data combined_draw {};
    style theme {};
    std::vector<std::pair<style_color, color>> color_stack {};
    std::vector<std::pair<style_var, float>> var_stack {};
    std::vector<std::uint64_t> id_stack {};
    std::uint64_t frame_index {};
    bool in_frame {};
};

std::unique_ptr<context> current;

context& require_context()
{
    if (!current) {
        throw std::logic_error("farcal context has not been created");
    }
    return *current;
}

color& color_ref(style& theme, style_color variable)
{
    switch (variable) {
    case style_color::text:
        return theme.text;
    case style_color::text_secondary:
        return theme.text_secondary;
    case style_color::text_muted:
        return theme.text_muted;
    case style_color::button:
        return theme.button;
    case style_color::button_hovered:
        return theme.button_hovered;
    case style_color::button_active:
        return theme.button_active;
    case style_color::button_primary:
        return theme.button_primary;
    case style_color::button_primary_hovered:
        return theme.button_primary_hovered;
    case style_color::button_primary_active:
        return theme.button_primary_active;
    case style_color::button_border:
        return theme.button_border;
    case style_color::window_background:
        return theme.window_background;
    case style_color::window_panel:
        return theme.window_panel;
    case style_color::window_border:
        return theme.window_border;
    case style_color::window_title:
        return theme.window_title;
    case style_color::window_title_active:
        return theme.window_title_active;
    case style_color::accent:
        return theme.accent;
    case style_color::selection:
        return theme.selection;
    }

    return theme.text;
}

float& var_ref(style& theme, style_var variable)
{
    switch (variable) {
    case style_var::font_size:
        return theme.font_size;
    case style_var::frame_scale:
        return theme.frame_scale;
    case style_var::frame_padding_x:
        return theme.frame_padding_x;
    case style_var::frame_padding_y:
        return theme.frame_padding_y;
    case style_var::item_spacing_y:
        return theme.item_spacing_y;
    case style_var::item_width:
        return theme.item_width;
    case style_var::window_width:
        return theme.window_width;
    case style_var::window_height:
        return theme.window_height;
    case style_var::window_title_height:
        return theme.window_title_height;
    case style_var::section_spacing_y:
        return theme.section_spacing_y;
    case style_var::anti_aliasing:
        return theme.anti_aliasing;
    }

    return theme.font_size;
}

std::uint64_t hash_bytes(std::uint64_t seed, const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const unsigned char*>(data);
    std::uint64_t result = seed == 0 ? 14695981039346656037ULL : seed;

    for (std::size_t index = 0; index < size; ++index) {
        result ^= bytes[index];
        result *= 1099511628211ULL;
    }

    return result;
}

std::uint64_t current_seed(const context& ctx)
{
    return ctx.id_stack.empty() ? 14695981039346656037ULL : ctx.id_stack.back();
}

}

void create_context(const context_config&)
{
    current = std::make_unique<context>();
}

void destroy_context()
{
    current.reset();
}

bool has_context()
{
    return current != nullptr;
}

void begin_frame(const input_state& input)
{
    context& ctx = require_context();
    ctx.input = input;
    ctx.background_draw.commands.clear();
    ctx.main_draw.commands.clear();
    ctx.foreground_draw.commands.clear();
    ctx.combined_draw.commands.clear();
    ctx.in_frame = true;
}

void end_frame()
{
    context& ctx = require_context();
    ctx.combined_draw.commands.clear();
    ctx.combined_draw.commands.reserve(ctx.background_draw.commands.size() + ctx.main_draw.commands.size() + ctx.foreground_draw.commands.size());
    ctx.combined_draw.commands.insert(ctx.combined_draw.commands.end(), ctx.background_draw.commands.begin(), ctx.background_draw.commands.end());
    ctx.combined_draw.commands.insert(ctx.combined_draw.commands.end(), ctx.main_draw.commands.begin(), ctx.main_draw.commands.end());
    ctx.combined_draw.commands.insert(ctx.combined_draw.commands.end(), ctx.foreground_draw.commands.begin(), ctx.foreground_draw.commands.end());
    ctx.in_frame = false;
    ++ctx.frame_index;
}

const input_state& input()
{
    return require_context().input;
}

const draw_data& draw()
{
    return require_context().combined_draw;
}

draw_data& background_renderer()
{
    return require_context().background_draw;
}

draw_data& foreground_renderer()
{
    return require_context().foreground_draw;
}

draw_data& main_renderer()
{
    return require_context().main_draw;
}

void push_id(std::string_view value)
{
    context& ctx = require_context();
    const std::uint64_t seed = current_seed(ctx);
    const std::uint64_t id = hash_bytes(seed, value.data(), value.size());
    ctx.id_stack.push_back(id);
}

void push_id(const void* value)
{
    context& ctx = require_context();
    const std::uint64_t seed = current_seed(ctx);
    const std::uintptr_t pointer = reinterpret_cast<std::uintptr_t>(value);
    const std::uint64_t id = hash_bytes(seed, &pointer, sizeof(pointer));
    ctx.id_stack.push_back(id);
}

void pop_id()
{
    context& ctx = require_context();
    if (ctx.id_stack.empty()) {
        throw std::logic_error("farcal id stack is empty");
    }

    ctx.id_stack.pop_back();
}

std::uint64_t current_id(std::string_view value)
{
    const context& ctx = require_context();
    const std::uint64_t seed = current_seed(ctx);
    return hash_bytes(seed, value.data(), value.size());
}

std::uint64_t frame_index()
{
    return require_context().frame_index;
}

const style& current_style()
{
    return require_context().theme;
}

void set_style(const style& value)
{
    require_context().theme = value;
}

void push_style_color(style_color variable, color value)
{
    context& ctx = require_context();
    color& target = color_ref(ctx.theme, variable);
    ctx.color_stack.push_back({variable, target});
    target = value;
}

void pop_style_color()
{
    context& ctx = require_context();
    if (ctx.color_stack.empty()) {
        throw std::logic_error("farcal style color stack is empty");
    }

    const auto [variable, value] = ctx.color_stack.back();
    color_ref(ctx.theme, variable) = value;
    ctx.color_stack.pop_back();
}

void push_style_var(style_var variable, float value)
{
    context& ctx = require_context();
    float& target = var_ref(ctx.theme, variable);
    ctx.var_stack.push_back({variable, target});
    target = value;
}

void pop_style_var()
{
    context& ctx = require_context();
    if (ctx.var_stack.empty()) {
        throw std::logic_error("farcal style var stack is empty");
    }

    const auto [variable, value] = ctx.var_stack.back();
    var_ref(ctx.theme, variable) = value;
    ctx.var_stack.pop_back();
}

}
