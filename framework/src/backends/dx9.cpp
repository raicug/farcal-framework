// clang-format off
#include <framework/backends/dx9.hpp>

#include <d2d1.h>
#include <dwrite.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <string_view>
#include <vector>
// clang-format on

namespace farcal {
namespace {

template <typename T> void release_if_set(T *&object) {
  if (object != nullptr) {
    object->Release();
    object = nullptr;
  }
}

struct vertex {
  float X{};
  float Y{};
  float Z{};
  float Rhw{};
  D3DCOLOR Color{};
};

struct text_vertex {
  float X{};
  float Y{};
  float Z{};
  float Rhw{};
  D3DCOLOR Color{};
  float U{};
  float V{};
};

struct render_batch {
  Rect Clip{};
  UINT IndexStart{};
  UINT IndexCount{};
};

constexpr DWORD vertex_fvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;
constexpr DWORD text_vertex_fvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

bool same_clip(Rect a, Rect b) {
  return a.Min.X == b.Min.X && a.Min.Y == b.Min.Y && a.Max.X == b.Max.X &&
         a.Max.Y == b.Max.Y;
}

void begin_batch(std::vector<render_batch> &batches, Rect clip,
                 UINT IndexStart) {
  if (!batches.empty() && batches.back().IndexCount == 0) {
    batches.back().Clip = clip;
    batches.back().IndexStart = IndexStart;
    return;
  }

  if (!batches.empty() && same_clip(batches.back().Clip, clip)) {
    return;
  }

  batches.push_back({
      .Clip = clip,
      .IndexStart = IndexStart,
      .IndexCount = 0,
  });
}

std::uint8_t byte_channel(float value) {
  return static_cast<std::uint8_t>((std::clamp)(value, 0.0F, 1.0F) * 255.0F +
                                   0.5F);
}

D3DCOLOR to_d3d(Color value) {
  return D3DCOLOR_ARGB(byte_channel(value.A), byte_channel(value.R),
                       byte_channel(value.G), byte_channel(value.B));
}

int next_power_of_two(int value) {
  int result = 1;
  while (result < value) {
    result *= 2;
  }
  return result;
}

vertex make_vertex(Vec2 position, Color tint) {
  return {
      position.X - 0.5F, position.Y - 0.5F, 0.0F, 1.0F, to_d3d(tint),
  };
}

text_vertex make_text_vertex(Vec2 position, Color tint, float u, float v) {
  return {
      position.X - 0.5F, position.Y - 0.5F, 0.0F, 1.0F, to_d3d(tint), u, v,
  };
}

void push_rect(std::vector<vertex> &vertices,
               std::vector<std::uint32_t> &indices,
               std::vector<render_batch> &batches, Rect bounds, Rect clip,
               Color tint) {
  if (bounds.Max.X <= bounds.Min.X || bounds.Max.Y <= bounds.Min.Y) {
    return;
  }

  begin_batch(batches, clip, static_cast<UINT>(indices.size()));

  const std::uint32_t base = static_cast<std::uint32_t>(vertices.size());
  vertices.push_back(make_vertex(bounds.Min, tint));
  vertices.push_back(make_vertex({bounds.Max.X, bounds.Min.Y}, tint));
  vertices.push_back(make_vertex(bounds.Max, tint));
  vertices.push_back(make_vertex({bounds.Min.X, bounds.Max.Y}, tint));

  indices.push_back(base);
  indices.push_back(base + 1);
  indices.push_back(base + 2);
  indices.push_back(base);
  indices.push_back(base + 2);
  indices.push_back(base + 3);

  batches.back().IndexCount += 6;
}

Color alpha(Color value, float multiplier) {
  value.A *= multiplier;
  return value;
}

void push_quad(std::vector<vertex> &vertices,
               std::vector<std::uint32_t> &indices,
               std::vector<render_batch> &batches, Vec2 a, Vec2 b, Vec2 c,
               Vec2 d, Rect clip, Color ca, Color cb, Color cc, Color cd) {
  begin_batch(batches, clip, static_cast<UINT>(indices.size()));

  const std::uint32_t base = static_cast<std::uint32_t>(vertices.size());
  vertices.push_back(make_vertex(a, ca));
  vertices.push_back(make_vertex(b, cb));
  vertices.push_back(make_vertex(c, cc));
  vertices.push_back(make_vertex(d, cd));

  indices.push_back(base);
  indices.push_back(base + 1);
  indices.push_back(base + 2);
  indices.push_back(base);
  indices.push_back(base + 2);
  indices.push_back(base + 3);

  batches.back().IndexCount += 6;
}

Rect clipped(Rect bounds, Rect clip) {
  return {
      {(std::max)(bounds.Min.X, clip.Min.X),
       (std::max)(bounds.Min.Y, clip.Min.Y)},
      {(std::min)(bounds.Max.X, clip.Max.X),
       (std::min)(bounds.Max.Y, clip.Max.Y)},
  };
}

bool empty_clip(Rect clip) {
  return clip.Max.X <= clip.Min.X || clip.Max.Y <= clip.Min.Y;
}

Rect viewport_clip(int Width, int Height) {
  return {{0.0F, 0.0F},
          {static_cast<float>(Width), static_cast<float>(Height)}};
}

Rect effective_clip(Rect clip, int Width, int Height) {
  return empty_clip(clip) ? viewport_clip(Width, Height) : clip;
}

void push_line_rect_clipped(std::vector<vertex> &vertices,
                            std::vector<std::uint32_t> &indices,
                            std::vector<render_batch> &batches, Rect bounds,
                            Rect clip, Color tint, float thickness) {
  push_rect(
      vertices, indices, batches,
      clipped({bounds.Min, {bounds.Max.X, bounds.Min.Y + thickness}}, clip),
      clip, tint);
  push_rect(
      vertices, indices, batches,
      clipped({{bounds.Min.X, bounds.Max.Y - thickness}, bounds.Max}, clip),
      clip, tint);
  push_rect(
      vertices, indices, batches,
      clipped({bounds.Min, {bounds.Min.X + thickness, bounds.Max.Y}}, clip),
      clip, tint);
  push_rect(
      vertices, indices, batches,
      clipped({{bounds.Max.X - thickness, bounds.Min.Y}, bounds.Max}, clip),
      clip, tint);
}

void push_line_segment(std::vector<vertex> &vertices,
                       std::vector<std::uint32_t> &indices,
                       std::vector<render_batch> &batches, Vec2 start, Vec2 end,
                       Rect clip, Color tint, float thickness,
                       float AntiAliasing) {
  if (empty_clip(clip)) {
    return;
  }

  const float dx = end.X - start.X;
  const float dy = end.Y - start.Y;
  const float length = std::sqrt(dx * dx + dy * dy);
  if (length <= 0.001F) {
    return;
  }

  const float dir_x = dx / length;
  const float dir_y = dy / length;
  const float normal_x = -dir_y;
  const float normal_y = dir_x;
  const float half = (std::max)(0.5F, thickness * 0.5F);
  const float feather = (std::max)(0.0F, AntiAliasing);
  const Vec2 a{start.X + normal_x * half, start.Y + normal_y * half};
  const Vec2 b{end.X + normal_x * half, end.Y + normal_y * half};
  const Vec2 c{end.X - normal_x * half, end.Y - normal_y * half};
  const Vec2 d{start.X - normal_x * half, start.Y - normal_y * half};

  push_quad(vertices, indices, batches, a, b, c, d, clip, tint, tint, tint,
            tint);

  if (feather <= 0.0F) {
    return;
  }

  const Color transparent = alpha(tint, 0.0F);
  const float outer = half + feather;
  const Vec2 outer_a{start.X + normal_x * outer - dir_x * feather,
                     start.Y + normal_y * outer - dir_y * feather};
  const Vec2 outer_b{end.X + normal_x * outer + dir_x * feather,
                     end.Y + normal_y * outer + dir_y * feather};
  const Vec2 outer_c{end.X - normal_x * outer + dir_x * feather,
                     end.Y - normal_y * outer + dir_y * feather};
  const Vec2 outer_d{start.X - normal_x * outer - dir_x * feather,
                     start.Y - normal_y * outer - dir_y * feather};

  push_quad(vertices, indices, batches, outer_a, outer_b, b, a, clip,
            transparent, transparent, tint, tint);
  push_quad(vertices, indices, batches, d, c, outer_c, outer_d, clip, tint,
            tint, transparent, transparent);
  push_quad(vertices, indices, batches, outer_d, outer_a, a, d, clip,
            transparent, transparent, tint, tint);
  push_quad(vertices, indices, batches, b, outer_b, outer_c, c, clip, tint,
            transparent, transparent, tint);
}

void append_command_geometry(const DrawCommand &command, int Width, int Height,
                             std::vector<vertex> &vertices,
                             std::vector<std::uint32_t> &indices,
                             std::vector<render_batch> &batches) {
  switch (command.Type) {
  case DrawCommandType::FilledRect: {
    const Rect clip = effective_clip(command.Clip, Width, Height);
    push_rect(vertices, indices, batches, clipped(command.Bounds, clip), clip,
              command.Tint);
    break;
  }

  case DrawCommandType::Rect:
    push_line_rect_clipped(vertices, indices, batches, command.Bounds,
                           effective_clip(command.Clip, Width, Height),
                           command.Tint, command.Thickness);
    break;

  case DrawCommandType::Line:
    push_line_segment(vertices, indices, batches, command.Start, command.End,
                      effective_clip(command.Clip, Width, Height), command.Tint,
                      command.Thickness, command.AntiAliasing);
    break;

  case DrawCommandType::Text:
    break;
  }
}

std::wstring widen(std::string_view value) {
  std::wstring result;
  result.reserve(value.size());

  for (char character : value) {
    result.push_back(
        static_cast<wchar_t>(static_cast<unsigned char>(character)));
  }

  return result;
}

std::uint8_t glyph_alpha(std::uint32_t pixel) {
  return static_cast<std::uint8_t>((pixel >> 24) & 0xFF);
}

} // namespace

Dx9Renderer::Dx9Renderer(Window &target) : Window_(&target) { CreateDevice(); }

Dx9Renderer::~Dx9Renderer() { Release(); }

bool Dx9Renderer::Valid() const {
  return Direct3D_ != nullptr && Device_ != nullptr;
}

void Dx9Renderer::Resize(int Width, int Height) {
  if (Device_ == nullptr || Width <= 0 || Height <= 0) {
    return;
  }

  ResetDevice(Width, Height);
}

void Dx9Renderer::Render(const DrawData &data) {
  if (!Valid()) {
    return;
  }

  if (Window_->Width() != BackBufferWidth_ ||
      Window_->Height() != BackBufferHeight_) {
    Resize(Window_->Width(), Window_->Height());
  }

  const HRESULT cooperative = Device_->TestCooperativeLevel();
  if (cooperative == D3DERR_DEVICELOST) {
    return;
  }
  if (cooperative == D3DERR_DEVICENOTRESET &&
      !ResetDevice(Window_->Width(), Window_->Height())) {
    return;
  }

  Device_->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(23, 26, 31), 1.0F,
                 0);

  if (FAILED(Device_->BeginScene())) {
    return;
  }

  Device_->SetFVF(vertex_fvf);
  Device_->SetRenderState(D3DRS_LIGHTING, FALSE);
  Device_->SetRenderState(D3DRS_ZENABLE, FALSE);
  Device_->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  Device_->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
  Device_->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
  Device_->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
  Device_->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
  Device_->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
  Device_->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
  Device_->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
  Device_->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

  std::vector<vertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<render_batch> batches;

  vertices.reserve(data.Commands.size() * 4);
  indices.reserve(data.Commands.size() * 6);

  auto flush_geometry = [&]() {
    if (vertices.empty() || indices.empty()) {
      return;
    }

    for (const render_batch &batch : batches) {
      if (batch.IndexCount == 0) {
        continue;
      }

      RECT scissor{
          static_cast<LONG>((std::max)(0.0F, batch.Clip.Min.X)),
          static_cast<LONG>((std::max)(0.0F, batch.Clip.Min.Y)),
          static_cast<LONG>((std::min)(static_cast<float>(Window_->Width()),
                                       batch.Clip.Max.X)),
          static_cast<LONG>((std::min)(static_cast<float>(Window_->Height()),
                                       batch.Clip.Max.Y)),
      };

      if (scissor.right <= scissor.left || scissor.bottom <= scissor.top) {
        continue;
      }

      Device_->SetTexture(0, nullptr);
      Device_->SetFVF(vertex_fvf);
      Device_->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
      Device_->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
      Device_->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
      Device_->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);

      Device_->SetScissorRect(&scissor);
      Device_->DrawIndexedPrimitiveUP(
          D3DPT_TRIANGLELIST, 0, static_cast<UINT>(vertices.size()),
          batch.IndexCount / 3, indices.data() + batch.IndexStart,
          D3DFMT_INDEX32, vertices.data(), sizeof(vertex));
    }

    vertices.clear();
    indices.clear();
    batches.clear();
  };

  for (const DrawCommand &command : data.Commands) {
    if (command.Type == DrawCommandType::Text) {
      flush_geometry();
      RenderText(command);
    } else {
      append_command_geometry(command, Window_->Width(), Window_->Height(),
                              vertices, indices, batches);
    }
  }

  flush_geometry();
  Device_->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
  Device_->EndScene();
}

void Dx9Renderer::Present(bool VerticalSync) {
  if (Device_ == nullptr) {
    return;
  }

  if ((VerticalSync
           ? D3DPRESENT_INTERVAL_ONE
           : D3DPRESENT_INTERVAL_IMMEDIATE) != Present_.PresentationInterval) {
    Present_.PresentationInterval =
        VerticalSync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
    ResetDevice(Window_->Width(), Window_->Height());
  }

  Device_->Present(nullptr, nullptr, nullptr, nullptr);
}

bool Dx9Renderer::CreateDevice() {
  Direct3D_ = Direct3DCreate9(D3D_SDK_VERSION);
  if (Direct3D_ == nullptr) {
    return false;
  }

  ZeroMemory(&Present_, sizeof(Present_));
  Present_.Windowed = TRUE;
  Present_.SwapEffect = D3DSWAPEFFECT_DISCARD;
  Present_.BackBufferFormat = D3DFMT_A8R8G8B8;
  Present_.BackBufferWidth = static_cast<UINT>(Window_->Width());
  Present_.BackBufferHeight = static_cast<UINT>(Window_->Height());
  Present_.EnableAutoDepthStencil = FALSE;
  Present_.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
  Present_.hDeviceWindow = Window_->NativeHandle();

  HRESULT result = Direct3D_->CreateDevice(
      D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, Window_->NativeHandle(),
      D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &Present_,
      &Device_);

  if (FAILED(result)) {
    result = Direct3D_->CreateDevice(
        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, Window_->NativeHandle(),
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
        &Present_, &Device_);
  }

  if (FAILED(result)) {
    release_if_set(Device_);
    return false;
  }

  BackBufferWidth_ = Window_->Width();
  BackBufferHeight_ = Window_->Height();
  return true;
}

bool Dx9Renderer::ResetDevice(int Width, int Height) {
  if (Device_ == nullptr || Width <= 0 || Height <= 0) {
    return false;
  }

  Present_.BackBufferWidth = static_cast<UINT>(Width);
  Present_.BackBufferHeight = static_cast<UINT>(Height);
  Present_.hDeviceWindow = Window_->NativeHandle();

  const HRESULT result = Device_->Reset(&Present_);
  if (FAILED(result)) {
    return false;
  }

  BackBufferWidth_ = Width;
  BackBufferHeight_ = Height;
  return true;
}

Dx9Renderer::FontAtlasEntry *
Dx9Renderer::TextAtlas(const DrawCommand &command) {
  const std::wstring family =
      command.FontFamily.empty() ? L"Inter" : command.FontFamily;
  const int size = static_cast<int>((std::max)(1.0F, command.FontSize));

  for (FontAtlasEntry &entry : FontAtlases_) {
    if (entry.family == family && entry.size == size) {
      return &entry;
    }
  }

  IDWriteFactory *write_factory{};
  HRESULT result =
      DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                          reinterpret_cast<IUnknown **>(&write_factory));
  if (FAILED(result)) {
    return nullptr;
  }

  IDWriteTextFormat *format{};
  result = write_factory->CreateTextFormat(
      family.c_str(), nullptr, DWRITE_FONT_WEIGHT_NORMAL,
      DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
      static_cast<float>(size), L"", &format);
  if (FAILED(result)) {
    release_if_set(write_factory);
    return nullptr;
  }

  format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
  format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

  int cell_width = 1;
  float line_height = static_cast<float>(size);
  std::array<DWRITE_TEXT_METRICS, 95> metrics{};

  for (wchar_t character = L' '; character <= L'~'; ++character) {
    const int index = static_cast<int>(character - L' ');
    IDWriteTextLayout *layout{};
    result = write_factory->CreateTextLayout(
        &character, 1, format, static_cast<float>(size * 4),
        static_cast<float>(size * 3), &layout);
    if (SUCCEEDED(result)) {
      layout->GetMetrics(&metrics[static_cast<std::size_t>(index)]);
      cell_width =
          (std::max)(cell_width, static_cast<int>(std::ceil(
                                     metrics[static_cast<std::size_t>(index)]
                                         .widthIncludingTrailingWhitespace)) +
                                     4);
      line_height = (std::max)(line_height,
                               metrics[static_cast<std::size_t>(index)].height);
      release_if_set(layout);
    }
  }

  const int cell_height = static_cast<int>(std::ceil(line_height)) + 4;
  constexpr int columns = 16;
  constexpr int rows = 6;
  const int texture_width = next_power_of_two(cell_width * columns);
  const int texture_height = next_power_of_two(cell_height * rows);

  HDC dc = CreateCompatibleDC(nullptr);
  if (dc == nullptr) {
    release_if_set(format);
    release_if_set(write_factory);
    return nullptr;
  }

  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = texture_width;
  info.bmiHeader.biHeight = -texture_height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;

  void *bitmap_pixels{};
  HBITMAP bitmap =
      CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bitmap_pixels, nullptr, 0);
  if (bitmap == nullptr || bitmap_pixels == nullptr) {
    DeleteDC(dc);
    release_if_set(format);
    release_if_set(write_factory);
    return nullptr;
  }

  HBITMAP old_bitmap = static_cast<HBITMAP>(SelectObject(dc, bitmap));

  ID2D1Factory *draw_factory{};
  result = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &draw_factory);
  if (FAILED(result)) {
    SelectObject(dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
    release_if_set(format);
    release_if_set(write_factory);
    return nullptr;
  }

  const D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(
      D2D1_RENDER_TARGET_TYPE_DEFAULT,
      D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                        D2D1_ALPHA_MODE_PREMULTIPLIED));

  ID2D1DCRenderTarget *target{};
  result = draw_factory->CreateDCRenderTarget(&properties, &target);
  if (FAILED(result)) {
    release_if_set(draw_factory);
    SelectObject(dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
    release_if_set(format);
    release_if_set(write_factory);
    return nullptr;
  }

  RECT target_rect{0, 0, texture_width, texture_height};
  result = target->BindDC(dc, &target_rect);
  if (FAILED(result)) {
    release_if_set(target);
    release_if_set(draw_factory);
    SelectObject(dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
    release_if_set(format);
    release_if_set(write_factory);
    return nullptr;
  }

  ID2D1SolidColorBrush *brush{};
  result = target->CreateSolidColorBrush(D2D1::ColorF(1.0F, 1.0F, 1.0F, 1.0F),
                                         &brush);
  if (FAILED(result)) {
    release_if_set(target);
    release_if_set(draw_factory);
    SelectObject(dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
    release_if_set(format);
    release_if_set(write_factory);
    return nullptr;
  }

  FontAtlasEntry atlas{};
  atlas.family = family;
  atlas.size = size;
  atlas.LineHeight = line_height;

  target->BeginDraw();
  target->Clear(D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.0F));
  target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

  for (wchar_t character = L' '; character <= L'~'; ++character) {
    const int index = static_cast<int>(character - L' ');
    const int column = index % columns;
    const int row = index / columns;
    const int x = column * cell_width;
    const int y = row * cell_height;

    target->DrawText(&character, 1, format,
                     D2D1::RectF(static_cast<float>(x + 2),
                                 static_cast<float>(y + 2),
                                 static_cast<float>(x + cell_width),
                                 static_cast<float>(y + cell_height)),
                     brush);

    Glyph &glyph = atlas.Glyphs[static_cast<std::size_t>(index)];
    glyph.Advance = metrics[static_cast<std::size_t>(index)]
                        .widthIncludingTrailingWhitespace;
    glyph.Width = metrics[static_cast<std::size_t>(index)].width;
    glyph.Height = metrics[static_cast<std::size_t>(index)].height;
    glyph.U0 = static_cast<float>(x + 2) / static_cast<float>(texture_width);
    glyph.V0 = static_cast<float>(y + 2) / static_cast<float>(texture_height);
    glyph.U1 =
        static_cast<float>(x + 2 + static_cast<int>(std::ceil(glyph.Width))) /
        static_cast<float>(texture_width);
    glyph.V1 =
        static_cast<float>(y + 2 + static_cast<int>(std::ceil(glyph.Height))) /
        static_cast<float>(texture_height);
  }

  result = target->EndDraw();
  release_if_set(brush);
  release_if_set(target);
  release_if_set(draw_factory);
  if (FAILED(result)) {
    SelectObject(dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
    release_if_set(format);
    release_if_set(write_factory);
    return nullptr;
  }

  result = Device_->CreateTexture(
      static_cast<UINT>(texture_width), static_cast<UINT>(texture_height), 1, 0,
      D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &atlas.Texture, nullptr);
  if (FAILED(result)) {
    SelectObject(dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
    release_if_set(format);
    release_if_set(write_factory);
    return nullptr;
  }

  D3DLOCKED_RECT locked{};
  result = atlas.Texture->LockRect(0, &locked, nullptr, 0);
  if (FAILED(result)) {
    release_if_set(atlas.Texture);
    SelectObject(dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
    release_if_set(format);
    release_if_set(write_factory);
    return nullptr;
  }

  const auto *source = static_cast<const std::uint32_t *>(bitmap_pixels);
  for (int y = 0; y < texture_height; ++y) {
    auto *target = reinterpret_cast<std::uint32_t *>(
        static_cast<unsigned char *>(locked.pBits) + y * locked.Pitch);
    for (int x = 0; x < texture_width; ++x) {
      const std::uint8_t alpha = glyph_alpha(source[y * texture_width + x]);
      target[x] = (static_cast<std::uint32_t>(alpha) << 24) | 0x00FFFFFFU;
    }
  }
  atlas.Texture->UnlockRect(0);

  SelectObject(dc, old_bitmap);
  DeleteObject(bitmap);
  DeleteDC(dc);
  release_if_set(format);
  release_if_set(write_factory);

  FontAtlases_.push_back(atlas);
  return &FontAtlases_.back();
}

void Dx9Renderer::RenderText(const DrawCommand &command) {
  if (command.Type != DrawCommandType::Text || command.Text.empty()) {
    return;
  }

  FontAtlasEntry *atlas = TextAtlas(command);
  if (atlas == nullptr || atlas->Texture == nullptr) {
    return;
  }

  const Rect clip_rect =
      clipped(command.Bounds, effective_clip(command.Clip, Window_->Width(),
                                             Window_->Height()));
  if (clip_rect.Max.X <= clip_rect.Min.X ||
      clip_rect.Max.Y <= clip_rect.Min.Y) {
    return;
  }

  std::vector<text_vertex> vertices;
  std::vector<std::uint32_t> indices;
  vertices.reserve(command.Text.size() * 4);
  indices.reserve(command.Text.size() * 6);

  const float bounds_height = command.Bounds.Max.Y - command.Bounds.Min.Y;
  float x = command.Bounds.Min.X;
  const float y = command.Bounds.Min.Y +
                  (std::max)(0.0F, (bounds_height - atlas->LineHeight) * 0.5F);

  for (char character : command.Text) {
    unsigned char byte = static_cast<unsigned char>(character);
    if (byte < 32 || byte > 126) {
      byte = '?';
    }

    const Glyph &glyph = atlas->Glyphs[static_cast<std::size_t>(byte - 32)];
    if (byte != ' ' && glyph.Width > 0.0F && glyph.Height > 0.0F) {
      const float x0 = x;
      const float y0 = y;
      const float x1 = x + glyph.Width;
      const float y1 = y + glyph.Height;
      const std::uint32_t base = static_cast<std::uint32_t>(vertices.size());

      vertices.push_back(
          make_text_vertex({x0, y0}, command.Tint, glyph.U0, glyph.V0));
      vertices.push_back(
          make_text_vertex({x1, y0}, command.Tint, glyph.U1, glyph.V0));
      vertices.push_back(
          make_text_vertex({x1, y1}, command.Tint, glyph.U1, glyph.V1));
      vertices.push_back(
          make_text_vertex({x0, y1}, command.Tint, glyph.U0, glyph.V1));

      indices.push_back(base);
      indices.push_back(base + 1);
      indices.push_back(base + 2);
      indices.push_back(base);
      indices.push_back(base + 2);
      indices.push_back(base + 3);
    }

    x += glyph.Advance;
    if (x >= command.Bounds.Max.X) {
      break;
    }
  }

  if (vertices.empty() || indices.empty()) {
    return;
  }

  RECT scissor{
      static_cast<LONG>((std::max)(0.0F, clip_rect.Min.X)),
      static_cast<LONG>((std::max)(0.0F, clip_rect.Min.Y)),
      static_cast<LONG>(
          (std::min)(static_cast<float>(Window_->Width()), clip_rect.Max.X)),
      static_cast<LONG>(
          (std::min)(static_cast<float>(Window_->Height()), clip_rect.Max.Y)),
  };

  if (scissor.right <= scissor.left || scissor.bottom <= scissor.top) {
    return;
  }

  Device_->SetScissorRect(&scissor);
  Device_->SetTexture(0, atlas->Texture);
  Device_->SetFVF(text_vertex_fvf);
  Device_->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
  Device_->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
  Device_->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
  Device_->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
  Device_->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
  Device_->DrawIndexedPrimitiveUP(
      D3DPT_TRIANGLELIST, 0, static_cast<UINT>(vertices.size()),
      static_cast<UINT>(indices.size() / 3), indices.data(), D3DFMT_INDEX32,
      vertices.data(), sizeof(text_vertex));
}

void Dx9Renderer::ReleaseFontAtlases() {
  for (FontAtlasEntry &entry : FontAtlases_) {
    release_if_set(entry.Texture);
  }
  FontAtlases_.clear();
}

void Dx9Renderer::Release() {
  ReleaseFontAtlases();
  release_if_set(Device_);
  release_if_set(Direct3D_);
}

} // namespace farcal
