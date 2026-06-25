#pragma once

// clang-format off
#include <framework/draw.hpp>
#include <framework/window.hpp>

#include <d2d1.h>
#include <d3d11.h>
#include <dwrite.h>
#include <dxgi.h>

#include <string>
#include <vector>
// clang-format on

namespace farcal {

class dx11_renderer {
public:
    explicit dx11_renderer(window& target);
    dx11_renderer(const dx11_renderer&) = delete;
    dx11_renderer& operator=(const dx11_renderer&) = delete;
    dx11_renderer(dx11_renderer&&) = delete;
    dx11_renderer& operator=(dx11_renderer&&) = delete;
    ~dx11_renderer();

    bool valid() const;
    void resize(int width, int height);
    void render(const draw_data& data);
    void present();

private:
    window* window_ {};
    ID3D11Device* device_ {};
    ID3D11DeviceContext* context_ {};
    IDXGISwapChain* swap_chain_ {};
    ID3D11RenderTargetView* render_target_ {};
    ID3D11VertexShader* vertex_shader_ {};
    ID3D11PixelShader* pixel_shader_ {};
    ID3D11InputLayout* input_layout_ {};
    ID3D11Buffer* vertex_buffer_ {};
    ID3D11Buffer* index_buffer_ {};
    ID3D11Buffer* constant_buffer_ {};
    ID3D11BlendState* blend_state_ {};
    ID3D11RasterizerState* rasterizer_state_ {};
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
