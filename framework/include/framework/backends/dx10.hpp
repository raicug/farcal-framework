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

class Dx10Renderer {
public:
    explicit Dx10Renderer(Window& target);
    Dx10Renderer(const Dx10Renderer&) = delete;
    Dx10Renderer& operator=(const Dx10Renderer&) = delete;
    Dx10Renderer(Dx10Renderer&&) = delete;
    Dx10Renderer& operator=(Dx10Renderer&&) = delete;
    ~Dx10Renderer();

    bool Valid() const;
    void Resize(int Width, int Height);
    void Render(const DrawData& data);
    void Present(bool VerticalSync = true);

private:
    Window* Window_ {};
    ID3D10Device1* Device_ {};
    IDXGISwapChain* SwapChain_ {};
    ID3D10RenderTargetView* RenderTarget_ {};
    ID3D10VertexShader* VertexShader_ {};
    ID3D10PixelShader* PixelShader_ {};
    ID3D10InputLayout* InputLayout_ {};
    ID3D10Buffer* VertexBuffer_ {};
    ID3D10Buffer* IndexBuffer_ {};
    ID3D10Buffer* ConstantBuffer_ {};
    ID3D10BlendState* BlendState_ {};
    ID3D10RasterizerState* RasterizerState_ {};
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
