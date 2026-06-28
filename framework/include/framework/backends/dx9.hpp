#pragma once

// clang-format off
#include <framework/draw.hpp>
#include <framework/window.hpp>

#include <d3d9.h>

#include <array>
#include <string>
#include <vector>
// clang-format on

namespace farcal {

class Dx9Renderer {
public:
  explicit Dx9Renderer(Window &target);
  Dx9Renderer(const Dx9Renderer &) = delete;
  Dx9Renderer &operator=(const Dx9Renderer &) = delete;
  Dx9Renderer(Dx9Renderer &&) = delete;
  Dx9Renderer &operator=(Dx9Renderer &&) = delete;
  ~Dx9Renderer();

  bool Valid() const;
  void Resize(int Width, int Height);
  void Render(const DrawData &data);
  void Present(bool VerticalSync = true);

private:
  Window *Window_{};
  IDirect3D9 *Direct3D_{};
  IDirect3DDevice9 *Device_{};
  D3DPRESENT_PARAMETERS Present_{};
  int BackBufferWidth_{};
  int BackBufferHeight_{};

  struct Glyph {
    float Advance{};
    float Width{};
    float Height{};
    float U0{};
    float V0{};
    float U1{};
    float V1{};
  };

  struct FontAtlasEntry {
    std::wstring family{};
    int size{};
    float LineHeight{};
    IDirect3DTexture9 *Texture{};
    std::array<Glyph, 95> Glyphs{};
  };

  std::vector<FontAtlasEntry> FontAtlases_{};

  bool CreateDevice();
  bool ResetDevice(int Width, int Height);
  FontAtlasEntry *TextAtlas(const DrawCommand &command);
  void RenderText(const DrawCommand &command);
  void ReleaseFontAtlases();
  void Release();
};

} // namespace farcal
