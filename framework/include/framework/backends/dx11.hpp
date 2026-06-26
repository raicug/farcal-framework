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

class Dx11Renderer {
public:
    explicit Dx11Renderer(Window& target);
    Dx11Renderer(const Dx11Renderer&) = delete;
    Dx11Renderer& operator=(const Dx11Renderer&) = delete;
    Dx11Renderer(Dx11Renderer&&) = delete;
    Dx11Renderer& operator=(Dx11Renderer&&) = delete;
    ~Dx11Renderer();

    bool Valid() const;
    void Resize(int Width, int Height);
    void Render(const DrawData& data);
    void Present(bool VerticalSync = true);

private:
    Window* Window_ {};
    ID3D11Device* Device_ {};
    ID3D11DeviceContext* Context_ {};
    IDXGISwapChain* SwapChain_ {};
    ID3D11RenderTargetView* RenderTarget_ {};
    ID3D11VertexShader* VertexShader_ {};
    ID3D11PixelShader* PixelShader_ {};
    ID3D11InputLayout* InputLayout_ {};
    ID3D11Buffer* VertexBuffer_ {};
    ID3D11Buffer* IndexBuffer_ {};
    ID3D11Buffer* ConstantBuffer_ {};
    ID3D11BlendState* BlendState_ {};
    ID3D11RasterizerState* RasterizerState_ {};
    ID2D1Factory* D2DFactory_ {};
    IDWriteFactory* DWriteFactory_ {};
    ID2D1RenderTarget* D2DRenderTarget_ {};
    UINT VertexCapacity_ {};
    UINT IndexCapacity_ {};
    int BackBufferWidth_ {};
    int BackBufferHeight_ {};

    struct TextFormatCacheEntry {
        std::wstring family {};
        float size {};
        IDWriteTextFormat* format {};
    };

    std::vector<TextFormatCacheEntry> TextFormatCache_ {};

    bool CreateDevice();
    bool CreateRenderTarget();
    bool CreatePipeline();
    bool CreateTextPipeline();
    bool EnsureVertexCapacity(UINT capacity);
    bool EnsureIndexCapacity(UINT capacity);
    IDWriteTextFormat* TextFormat(const DrawCommand& command);
    void RenderText(const DrawData& data);
    void ReleaseRenderTarget();
    void Release();
};

}
