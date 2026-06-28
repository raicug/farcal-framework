#pragma once

// clang-format off
#include <framework/input.hpp>
// clang-format on

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// clang-format off
#include <windows.h>

#include <functional>
#include <optional>
#include <string>
// clang-format on

namespace farcal {

struct WindowDesc {
  std::wstring Title{L"Farcal"};
  int Width{1280};
  int Height{720};
};

using WndProcHook =
    std::function<std::optional<LRESULT>(HWND, UINT, WPARAM, LPARAM)>;

class Window {
public:
  explicit Window(const WindowDesc &desc = {});
  Window(const Window &) = delete;
  Window &operator=(const Window &) = delete;
  Window(Window &&other) noexcept;
  Window &operator=(Window &&other) noexcept;
  ~Window();

  bool PollEvents();
  void CancelNextPoll();
  InputState ConsumeInput();
  void SetWndProcHook(WndProcHook hook);
  HWND NativeHandle() const;
  int Width() const;
  int Height() const;
  bool CloseRequested() const;

  static LRESULT CALLBACK WndProc(HWND handle, UINT message, WPARAM wparam,
                                  LPARAM lparam);

private:
  HWND Handle_{};
  int Width_{};
  int Height_{};
  bool CloseRequested_{};
  bool CancelNextPoll_{};
  InputState Input_{};
  WndProcHook Hook_{};

  LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);
  void Release();
};

} // namespace farcal
