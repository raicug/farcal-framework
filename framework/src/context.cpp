// clang-format off
#include <framework/context.hpp>

#include <windows.h>
#include <psapi.h>
#include <timeapi.h>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
// clang-format on

namespace farcal {
namespace {

struct context {
    InputState Input {};
    DrawData background_draw {};
    DrawData main_draw {};
    DrawData foreground_draw {};
    DrawData combined_draw {};
    Style theme {};
    std::vector<std::pair<StyleColor, Color>> color_stack {};
    std::vector<std::pair<StyleVar, float>> var_stack {};
    std::vector<std::uint64_t> id_stack {};
    Statistics Stats {};
    std::chrono::steady_clock::time_point last_begin {};
    std::chrono::steady_clock::time_point current_begin {};
    float MaxFps {};
    bool timer_period_enabled {};
    std::uint64_t FrameIndex {};
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

Color& color_ref(Style& theme, StyleColor variable)
{
    switch (variable) {
    case StyleColor::Text:
        return theme.Text;
    case StyleColor::TextSecondary:
        return theme.TextSecondary;
    case StyleColor::TextMuted:
        return theme.TextMuted;
    case StyleColor::Button:
        return theme.Button;
    case StyleColor::ButtonHovered:
        return theme.ButtonHovered;
    case StyleColor::ButtonActive:
        return theme.ButtonActive;
    case StyleColor::ButtonPrimary:
        return theme.ButtonPrimary;
    case StyleColor::ButtonPrimaryHovered:
        return theme.ButtonPrimaryHovered;
    case StyleColor::ButtonPrimaryActive:
        return theme.ButtonPrimaryActive;
    case StyleColor::ButtonBorder:
        return theme.ButtonBorder;
    case StyleColor::WindowBackground:
        return theme.WindowBackground;
    case StyleColor::WindowPanel:
        return theme.WindowPanel;
    case StyleColor::WindowBorder:
        return theme.WindowBorder;
    case StyleColor::WindowTitle:
        return theme.WindowTitle;
    case StyleColor::WindowTitleActive:
        return theme.WindowTitleActive;
    case StyleColor::Accent:
        return theme.Accent;
    case StyleColor::Selection:
        return theme.Selection;
    }

    return theme.Text;
}

float& var_ref(Style& theme, StyleVar variable)
{
    switch (variable) {
    case StyleVar::FontSize:
        return theme.FontSize;
    case StyleVar::FrameScale:
        return theme.FrameScale;
    case StyleVar::FramePaddingX:
        return theme.FramePaddingX;
    case StyleVar::FramePaddingY:
        return theme.FramePaddingY;
    case StyleVar::ItemSpacingY:
        return theme.ItemSpacingY;
    case StyleVar::ItemWidth:
        return theme.ItemWidth;
    case StyleVar::WindowWidth:
        return theme.WindowWidth;
    case StyleVar::WindowHeight:
        return theme.WindowHeight;
    case StyleVar::WindowTitleHeight:
        return theme.WindowTitleHeight;
    case StyleVar::SectionSpacingY:
        return theme.SectionSpacingY;
    case StyleVar::AntiAliasing:
        return theme.AntiAliasing;
    }

    return theme.FontSize;
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

void update_memory_statistics(Statistics& Stats)
{
    PROCESS_MEMORY_COUNTERS_EX counters {};
    counters.cb = sizeof(counters);

    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) {
        Stats.MemoryWorkingSet = counters.WorkingSetSize;
        Stats.MemoryPrivate = counters.PrivateUsage;
    }
}

}

void CreateContext(const ContextConfig&)
{
    current = std::make_unique<context>();
    if (timeBeginPeriod(1) == TIMERR_NOERROR) {
        current->timer_period_enabled = true;
    }
}

void DestroyContext()
{
    if (current && current->timer_period_enabled) {
        timeEndPeriod(1);
    }

    current.reset();
}

bool HasContext()
{
    return current != nullptr;
}

void BeginFrame(const InputState& Input)
{
    context& ctx = require_context();
    const auto now = std::chrono::steady_clock::now();
    ctx.current_begin = now;

    if (ctx.last_begin.time_since_epoch().count() != 0) {
        ctx.Stats.DeltaSeconds = std::chrono::duration<double>(now - ctx.last_begin).count();
        if (ctx.Stats.DeltaSeconds > 0.0) {
            ctx.Stats.FramesPerSecond = 1.0 / ctx.Stats.DeltaSeconds;
        }
    }

    ctx.last_begin = now;
    ctx.Input = Input;
    ctx.background_draw.Commands.clear();
    ctx.main_draw.Commands.clear();
    ctx.foreground_draw.Commands.clear();
    ctx.combined_draw.Commands.clear();
    ctx.in_frame = true;
}

void EndFrame()
{
    context& ctx = require_context();
    ctx.combined_draw.Commands.clear();
    ctx.combined_draw.Commands.reserve(ctx.background_draw.Commands.size() + ctx.main_draw.Commands.size() + ctx.foreground_draw.Commands.size());
    ctx.combined_draw.Commands.insert(ctx.combined_draw.Commands.end(), ctx.background_draw.Commands.begin(), ctx.background_draw.Commands.end());
    ctx.combined_draw.Commands.insert(ctx.combined_draw.Commands.end(), ctx.main_draw.Commands.begin(), ctx.main_draw.Commands.end());
    ctx.combined_draw.Commands.insert(ctx.combined_draw.Commands.end(), ctx.foreground_draw.Commands.begin(), ctx.foreground_draw.Commands.end());
    ctx.Stats.FrameSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - ctx.current_begin).count();
    ctx.Stats.FrameIndex = ctx.FrameIndex;
    ctx.Stats.BackgroundCommandCount = ctx.background_draw.Commands.size();
    ctx.Stats.MainCommandCount = ctx.main_draw.Commands.size();
    ctx.Stats.ForegroundCommandCount = ctx.foreground_draw.Commands.size();
    ctx.Stats.DrawCommandCount = ctx.combined_draw.Commands.size();
    update_memory_statistics(ctx.Stats);
    ctx.in_frame = false;
    ++ctx.FrameIndex;
}

void SetMaxFps(float value)
{
    require_context().MaxFps = (std::max)(0.0F, value);
}

float MaxFps()
{
    return require_context().MaxFps;
}

void LimitFrameRate()
{
    const context& ctx = require_context();
    if (ctx.MaxFps <= 0.0F) {
        return;
    }

    const auto target = std::chrono::duration<double>(1.0 / static_cast<double>(ctx.MaxFps));
    const auto target_time = ctx.current_begin + std::chrono::duration_cast<std::chrono::steady_clock::duration>(target);
    constexpr auto spin_margin = std::chrono::microseconds(750);

    for (;;) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= target_time) {
            break;
        }

        const auto remaining = target_time - now;
        if (remaining > spin_margin) {
            std::this_thread::sleep_for(remaining - spin_margin);
        } else {
            std::this_thread::yield();
        }
    }
}

const InputState& Input()
{
    return require_context().Input;
}

const DrawData& Draw()
{
    return require_context().combined_draw;
}

const Statistics& Stats()
{
    return require_context().Stats;
}

DrawData& BackgroundRenderer()
{
    return require_context().background_draw;
}

DrawData& ForegroundRenderer()
{
    return require_context().foreground_draw;
}

DrawData& MainRenderer()
{
    return require_context().main_draw;
}

void PushId(std::string_view value)
{
    context& ctx = require_context();
    const std::uint64_t seed = current_seed(ctx);
    const std::uint64_t id = hash_bytes(seed, value.data(), value.size());
    ctx.id_stack.push_back(id);
}

void PushId(const void* value)
{
    context& ctx = require_context();
    const std::uint64_t seed = current_seed(ctx);
    const std::uintptr_t pointer = reinterpret_cast<std::uintptr_t>(value);
    const std::uint64_t id = hash_bytes(seed, &pointer, sizeof(pointer));
    ctx.id_stack.push_back(id);
}

void PopId()
{
    context& ctx = require_context();
    if (ctx.id_stack.empty()) {
        throw std::logic_error("farcal id stack is empty");
    }

    ctx.id_stack.pop_back();
}

std::uint64_t CurrentId(std::string_view value)
{
    const context& ctx = require_context();
    const std::uint64_t seed = current_seed(ctx);
    return hash_bytes(seed, value.data(), value.size());
}

std::uint64_t FrameIndex()
{
    return require_context().FrameIndex;
}

const Style& CurrentStyle()
{
    return require_context().theme;
}

void SetStyle(const Style& value)
{
    require_context().theme = value;
}

void PushStyleColor(StyleColor variable, Color value)
{
    context& ctx = require_context();
    Color& target = color_ref(ctx.theme, variable);
    ctx.color_stack.push_back({variable, target});
    target = value;
}

void PopStyleColor()
{
    context& ctx = require_context();
    if (ctx.color_stack.empty()) {
        throw std::logic_error("farcal Style Color stack is empty");
    }

    const auto [variable, value] = ctx.color_stack.back();
    color_ref(ctx.theme, variable) = value;
    ctx.color_stack.pop_back();
}

void PushStyleVar(StyleVar variable, float value)
{
    context& ctx = require_context();
    float& target = var_ref(ctx.theme, variable);
    ctx.var_stack.push_back({variable, target});
    target = value;
}

void PopStyleVar()
{
    context& ctx = require_context();
    if (ctx.var_stack.empty()) {
        throw std::logic_error("farcal Style var stack is empty");
    }

    const auto [variable, value] = ctx.var_stack.back();
    var_ref(ctx.theme, variable) = value;
    ctx.var_stack.pop_back();
}

}
