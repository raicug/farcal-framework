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

struct ContextConfig {};

struct Statistics {
  double DeltaSeconds{};
  double FrameSeconds{};
  double FramesPerSecond{};
  std::uint64_t FrameIndex{};
  std::size_t DrawCommandCount{};
  std::size_t BackgroundCommandCount{};
  std::size_t MainCommandCount{};
  std::size_t ForegroundCommandCount{};
  std::size_t MemoryWorkingSet{};
  std::size_t MemoryPrivate{};
};

void CreateContext(const ContextConfig &config = {});
void DestroyContext();
bool HasContext();
void BeginFrame(const InputState &Input = {});
void EndFrame();
void SetMaxFps(float value);
float MaxFps();
void LimitFrameRate();
const InputState &Input();
const DrawData &Draw();
const Statistics &Stats();
DrawData &BackgroundRenderer();
DrawData &ForegroundRenderer();
DrawData &MainRenderer();
void PushId(std::string_view value);
void PushId(const void *value);
void PopId();
std::uint64_t CurrentId(std::string_view value);
std::uint64_t FrameIndex();

template <std::invocable Callback> void Frame(Callback &&callback) {
  callback();
}

} // namespace farcal
