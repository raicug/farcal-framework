#pragma once

// clang-format off
#include <framework/draw.hpp>
#include <framework/window.hpp>

#include <d2d1.h>
#include <d3d10_1.h>
#include <dwrite.h>
#include <dxgi.h>

#include <string>
#include <vector>
// clang-format on

namespace farcal {

class dx10_renderer {
public:
    explicit dx10_renderer(window& target);
    dx10_renderer(const dx10_renderer&) = delete;
    dx10_renderer& operator=(const dx10_renderer&) = delete;
    dx10_renderer(dx10_renderer&&) = delete;
    dx10_renderer& operator=(dx10_renderer&&) = delete;
    ~dx10_renderer();

    bool valid() const;
    void resize(int width, int height);
    void render(const draw_data& data);
    void present();

private:
    window* window_ {};
    ID3D10Device1* device_ {};
    IDXGISwapChain* swap_chain_ {};
    ID3D10RenderTargetView* render_target_ {};
    ID3D10VertexShader* vertex_shader_ {};
    ID3D10PixelShader* pixel_shader_ {};
    ID3D10InputLayout* input_layout_ {};
    ID3D10Buffer* vertex_buffer_ {};
    ID3D10Buffer* index_buffer_ {};
    ID3D10Buffer* constant_buffer_ {};
    ID3D10BlendState* blend_state_ {};
    ID3D10RasterizerState* rasterizer_state_ {};
    ID2D1Factory* d2d_factory_ {};
    IDWriteFactory* dwrite_factory_ {};
    ID2D1RenderTarget* d2d_render_target_ {};
    UINT vertex_capacity_ {};
    UINT index_capacity_ {};
    int back_buffer_width_ {};
    int back_buffer_height_ {};

    struct text_format_cache_entry {
        std::wstring family {};
        float size {};
        IDWriteTextFormat* format {};
    };

    std::vector<text_format_cache_entry> text_format_cache_ {};

    bool create_device();
    bool create_render_target();
    bool create_pipeline();
    bool create_text_pipeline();
    bool ensure_vertex_capacity(UINT capacity);
    bool ensure_index_capacity(UINT capacity);
    IDWriteTextFormat* text_format(const draw_command& command);
    void render_text(const draw_data& data);
    void release_render_target();
    void release();
};

}
