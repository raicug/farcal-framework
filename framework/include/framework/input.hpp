#pragma once

// clang-format off
#include <framework/draw.hpp>

#include <array>
#include <string>
// clang-format on

namespace farcal {

struct InputState {
  Vec2 MousePosition{};
  Vec2 MouseDelta{};
  float MouseWheel{};
  std::array<bool, 5> MouseDown{};
  std::array<bool, 5> MousePressed{};
  std::array<bool, 5> MouseReleased{};
  std::array<bool, 256> KeyDown{};
  std::array<bool, 256> KeyPressed{};
  std::array<bool, 256> KeyRepeated{};
  std::array<bool, 256> KeyReleased{};
  std::string TextInput{};
  bool Shift{};
  bool Control{};
  bool Alt{};
};

} // namespace farcal
