// clang-format off
#include <framework/backends/dx11.hpp>

#include <d2d1.h>
#include <d3dcompiler.h>
#include <dwrite.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>
// clang-format on

namespace farcal {
namespace {

template <typename T>
void release_if_set(T*& object)
{
    if (object != nullptr) {
        object->Release();
        object = nullptr;
    }
}

struct vertex {
    float Position[2] {};
    float Color[4] {};
};

struct render_batch {
    Rect Clip {};
    UINT IndexStart {};
    UINT IndexCount {};
};

struct constants {
    float Viewport[2] {};
    float Padding[2] {};
};

constexpr char shader_source[] =
    "cbuffer constants : register(b0) { float2 viewport; float2 padding; };"
    "struct vs_in { float2 position : POSITION; float4 color : COLOR; };"
    "struct ps_in { float4 position : SV_POSITION; float4 color : COLOR; };"
    "ps_in vs_main(vs_in input) {"
    "    ps_in output;"
    "    float2 clip = float2((input.position.x / viewport.x) * 2.0f - 1.0f, 1.0f - (input.position.y / viewport.y) * 2.0f);"
    "    output.position = float4(clip, 0.0f, 1.0f);"
    "    output.color = input.color;"
    "    return output;"
    "}"
    "float4 ps_main(ps_in input) : SV_Target { return input.color; }";

bool same_clip(Rect a, Rect b)
{
    return a.Min.X == b.Min.X && a.Min.Y == b.Min.Y && a.Max.X == b.Max.X && a.Max.Y == b.Max.Y;
}

void begin_batch(std::vector<render_batch>& batches, Rect clip, UINT IndexStart)
{
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

void push_rect(std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<render_batch>& batches, Rect bounds, Rect clip, Color tint)
{
    if (bounds.Max.X <= bounds.Min.X || bounds.Max.Y <= bounds.Min.Y) {
        return;
    }

    begin_batch(batches, clip, static_cast<UINT>(indices.size()));

    const std::uint32_t base = static_cast<std::uint32_t>(vertices.size());
    const vertex a {{bounds.Min.X, bounds.Min.Y}, {tint.R, tint.G, tint.B, tint.A}};
    const vertex b {{bounds.Max.X, bounds.Min.Y}, {tint.R, tint.G, tint.B, tint.A}};
    const vertex c {{bounds.Max.X, bounds.Max.Y}, {tint.R, tint.G, tint.B, tint.A}};
    const vertex d {{bounds.Min.X, bounds.Max.Y}, {tint.R, tint.G, tint.B, tint.A}};

    vertices.push_back(a);
    vertices.push_back(b);
    vertices.push_back(c);
    vertices.push_back(d);

    indices.push_back(base);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base);
    indices.push_back(base + 2);
    indices.push_back(base + 3);

    batches.back().IndexCount += 6;
}

Color alpha(Color value, float multiplier)
{
    value.A *= multiplier;
    return value;
}

void push_quad(std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<render_batch>& batches, Vec2 a, Vec2 b, Vec2 c, Vec2 d, Rect clip, Color ca, Color cb, Color cc, Color cd)
{
    begin_batch(batches, clip, static_cast<UINT>(indices.size()));

    const std::uint32_t base = static_cast<std::uint32_t>(vertices.size());
    vertices.push_back({{a.X, a.Y}, {ca.R, ca.G, ca.B, ca.A}});
    vertices.push_back({{b.X, b.Y}, {cb.R, cb.G, cb.B, cb.A}});
    vertices.push_back({{c.X, c.Y}, {cc.R, cc.G, cc.B, cc.A}});
    vertices.push_back({{d.X, d.Y}, {cd.R, cd.G, cd.B, cd.A}});

    indices.push_back(base);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base);
    indices.push_back(base + 2);
    indices.push_back(base + 3);

    batches.back().IndexCount += 6;
}

Rect clipped(Rect bounds, Rect clip)
{
    return {
        {(std::max)(bounds.Min.X, clip.Min.X), (std::max)(bounds.Min.Y, clip.Min.Y)},
        {(std::min)(bounds.Max.X, clip.Max.X), (std::min)(bounds.Max.Y, clip.Max.Y)},
    };
}

bool empty_clip(Rect clip)
{
    return clip.Max.X <= clip.Min.X || clip.Max.Y <= clip.Min.Y;
}

Rect viewport_clip(int Width, int Height)
{
    return {{0.0F, 0.0F}, {static_cast<float>(Width), static_cast<float>(Height)}};
}

Rect effective_clip(Rect clip, int Width, int Height)
{
    return empty_clip(clip) ? viewport_clip(Width, Height) : clip;
}

void push_line_rect(std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<render_batch>& batches, Rect bounds, Rect clip, Color tint, float thickness)
{
    push_rect(vertices, indices, batches, {bounds.Min, {bounds.Max.X, bounds.Min.Y + thickness}}, clip, tint);
    push_rect(vertices, indices, batches, {{bounds.Min.X, bounds.Max.Y - thickness}, bounds.Max}, clip, tint);
    push_rect(vertices, indices, batches, {bounds.Min, {bounds.Min.X + thickness, bounds.Max.Y}}, clip, tint);
    push_rect(vertices, indices, batches, {{bounds.Max.X - thickness, bounds.Min.Y}, bounds.Max}, clip, tint);
}

void push_line_rect_clipped(std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<render_batch>& batches, Rect bounds, Rect clip, Color tint, float thickness)
{
    push_rect(vertices, indices, batches, clipped({bounds.Min, {bounds.Max.X, bounds.Min.Y + thickness}}, clip), clip, tint);
    push_rect(vertices, indices, batches, clipped({{bounds.Min.X, bounds.Max.Y - thickness}, bounds.Max}, clip), clip, tint);
    push_rect(vertices, indices, batches, clipped({bounds.Min, {bounds.Min.X + thickness, bounds.Max.Y}}, clip), clip, tint);
    push_rect(vertices, indices, batches, clipped({{bounds.Max.X - thickness, bounds.Min.Y}, bounds.Max}, clip), clip, tint);
}

void push_line_segment(std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<render_batch>& batches, Vec2 start, Vec2 end, Rect clip, Color tint, float thickness, float AntiAliasing)
{
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
    const Vec2 a {start.X + normal_x * half, start.Y + normal_y * half};
    const Vec2 b {end.X + normal_x * half, end.Y + normal_y * half};
    const Vec2 c {end.X - normal_x * half, end.Y - normal_y * half};
    const Vec2 d {start.X - normal_x * half, start.Y - normal_y * half};

    push_quad(vertices, indices, batches, a, b, c, d, clip, tint, tint, tint, tint);

    if (feather <= 0.0F) {
        return;
    }

    const Color transparent = alpha(tint, 0.0F);
    const float outer = half + feather;
    const Vec2 outer_a {start.X + normal_x * outer - dir_x * feather, start.Y + normal_y * outer - dir_y * feather};
    const Vec2 outer_b {end.X + normal_x * outer + dir_x * feather, end.Y + normal_y * outer + dir_y * feather};
    const Vec2 outer_c {end.X - normal_x * outer + dir_x * feather, end.Y - normal_y * outer + dir_y * feather};
    const Vec2 outer_d {start.X - normal_x * outer - dir_x * feather, start.Y - normal_y * outer - dir_y * feather};

    push_quad(vertices, indices, batches, outer_a, outer_b, b, a, clip, transparent, transparent, tint, tint);
    push_quad(vertices, indices, batches, d, c, outer_c, outer_d, clip, tint, tint, transparent, transparent);
    push_quad(vertices, indices, batches, outer_d, outer_a, a, d, clip, transparent, transparent, tint, tint);
    push_quad(vertices, indices, batches, b, outer_b, outer_c, c, clip, tint, transparent, transparent, tint);
}

void append_command_geometry(const DrawCommand& command, int Width, int Height, std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<render_batch>& batches)
{
    switch (command.Type) {
    case DrawCommandType::FilledRect:
    {
        const Rect clip = effective_clip(command.Clip, Width, Height);
        push_rect(vertices, indices, batches, clipped(command.Bounds, clip), clip, command.Tint);
        break;
    }

    case DrawCommandType::Rect:
        push_line_rect_clipped(vertices, indices, batches, command.Bounds, effective_clip(command.Clip, Width, Height), command.Tint, command.Thickness);
        break;

    case DrawCommandType::Line:
        push_line_segment(vertices, indices, batches, command.Start, command.End, effective_clip(command.Clip, Width, Height), command.Tint, command.Thickness, command.AntiAliasing);
        break;

    case DrawCommandType::Text:
        break;
    }
}

std::wstring widen(std::string_view value)
{
    std::wstring result;
    result.reserve(value.size());

    for (char character : value) {
        result.push_back(static_cast<wchar_t>(static_cast<unsigned char>(character)));
    }

    return result;
}

D2D1_COLOR_F to_d2d(Color value)
{
    return D2D1::ColorF(value.R, value.G, value.B, value.A);
}

}

Dx11Renderer::Dx11Renderer(Window& target)
    : Window_(&target)
{
    if (CreateDevice()) {
        CreateRenderTarget();
        CreatePipeline();
        CreateTextPipeline();
    }
}

Dx11Renderer::~Dx11Renderer()
{
    Release();
}

bool Dx11Renderer::Valid() const
{
    return Device_ != nullptr && Context_ != nullptr && SwapChain_ != nullptr && RenderTarget_ != nullptr;
}

void Dx11Renderer::Resize(int Width, int Height)
{
    if (SwapChain_ == nullptr || Width <= 0 || Height <= 0) {
        return;
    }

    Context_->OMSetRenderTargets(0, nullptr, nullptr);
    ReleaseRenderTarget();

    const HRESULT result = SwapChain_->ResizeBuffers(0, static_cast<UINT>(Width), static_cast<UINT>(Height), DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(result)) {
        return;
    }

    BackBufferWidth_ = Width;
    BackBufferHeight_ = Height;
    CreateRenderTarget();
    CreateTextPipeline();
}

void Dx11Renderer::Render(const DrawData& data)
{
    if (!Valid()) {
        return;
    }

    if (Window_->Width() != BackBufferWidth_ || Window_->Height() != BackBufferHeight_) {
        Resize(Window_->Width(), Window_->Height());
    }

    constexpr float clear_color[] {0.090F, 0.102F, 0.122F, 1.0F};
    Context_->OMSetRenderTargets(1, &RenderTarget_, nullptr);
    Context_->ClearRenderTargetView(RenderTarget_, clear_color);

    if (VertexShader_ == nullptr || PixelShader_ == nullptr || InputLayout_ == nullptr) {
        return;
    }

    std::vector<DrawCommand> pending_text;
    std::vector<vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<render_batch> batches;

    vertices.reserve(data.Commands.size() * 4);
    indices.reserve(data.Commands.size() * 6);

    auto flush_geometry = [&]() {
        if (vertices.empty() || indices.empty()) {
            return;
        }

        if (!EnsureVertexCapacity(static_cast<UINT>(vertices.size())) || !EnsureIndexCapacity(static_cast<UINT>(indices.size()))) {
            vertices.clear();
            indices.clear();
            batches.clear();
            return;
        }

        D3D11_MAPPED_SUBRESOURCE mapped {};
        if (FAILED(Context_->Map(VertexBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            vertices.clear();
            indices.clear();
            batches.clear();
            return;
        }
        std::memcpy(mapped.pData, vertices.data(), vertices.size() * sizeof(vertex));
        Context_->Unmap(VertexBuffer_, 0);

        if (FAILED(Context_->Map(IndexBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            vertices.clear();
            indices.clear();
            batches.clear();
            return;
        }
        std::memcpy(mapped.pData, indices.data(), indices.size() * sizeof(std::uint32_t));
        Context_->Unmap(IndexBuffer_, 0);

        const UINT stride = sizeof(vertex);
        const UINT offset = 0;
        Context_->IASetVertexBuffers(0, 1, &VertexBuffer_, &stride, &offset);
        Context_->IASetIndexBuffer(IndexBuffer_, DXGI_FORMAT_R32_UINT, 0);

        for (const render_batch& batch : batches) {
            if (batch.IndexCount == 0) {
                continue;
            }

            D3D11_RECT scissor {
                static_cast<LONG>((std::max)(0.0F, batch.Clip.Min.X)),
                static_cast<LONG>((std::max)(0.0F, batch.Clip.Min.Y)),
                static_cast<LONG>((std::min)(static_cast<float>(Window_->Width()), batch.Clip.Max.X)),
                static_cast<LONG>((std::min)(static_cast<float>(Window_->Height()), batch.Clip.Max.Y)),
            };

            if (scissor.right <= scissor.left || scissor.bottom <= scissor.top) {
                continue;
            }

            Context_->RSSetScissorRects(1, &scissor);
            Context_->DrawIndexed(batch.IndexCount, batch.IndexStart, 0);
        }

        vertices.clear();
        indices.clear();
        batches.clear();
    };

    auto flush_text = [&]() {
        if (pending_text.empty()) {
            return;
        }

        DrawData text_data {};
        text_data.Commands = pending_text;
        RenderText(text_data);
        pending_text.clear();
    };

    constants constant_data {{static_cast<float>(Window_->Width()), static_cast<float>(Window_->Height())}, {0.0F, 0.0F}};
    Context_->UpdateSubresource(ConstantBuffer_, 0, nullptr, &constant_data, 0, 0);

    D3D11_VIEWPORT Viewport {};
    Viewport.Width = static_cast<float>(Window_->Width());
    Viewport.Height = static_cast<float>(Window_->Height());
    Viewport.MinDepth = 0.0F;
    Viewport.MaxDepth = 1.0F;

    const float blend_factor[4] {};
    Context_->RSSetViewports(1, &Viewport);
    Context_->RSSetState(RasterizerState_);
    Context_->OMSetBlendState(BlendState_, blend_factor, 0xFFFFFFFF);
    Context_->IASetInputLayout(InputLayout_);
    Context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context_->VSSetShader(VertexShader_, nullptr, 0);
    Context_->VSSetConstantBuffers(0, 1, &ConstantBuffer_);
    Context_->PSSetShader(PixelShader_, nullptr, 0);

    for (const DrawCommand& command : data.Commands) {
        if (command.Type == DrawCommandType::Text) {
            flush_geometry();
            pending_text.push_back(command);
        } else {
            flush_text();
            append_command_geometry(command, Window_->Width(), Window_->Height(), vertices, indices, batches);
        }
    }

    flush_geometry();
    flush_text();
}

void Dx11Renderer::Present(bool VerticalSync)
{
    if (SwapChain_ != nullptr) {
        SwapChain_->Present(VerticalSync ? 1U : 0U, 0);
    }
}

bool Dx11Renderer::CreateDevice()
{
    DXGI_SWAP_CHAIN_DESC SwapChain_desc {};
    SwapChain_desc.BufferCount = 2;
    SwapChain_desc.BufferDesc.Width = static_cast<UINT>(Window_->Width());
    SwapChain_desc.BufferDesc.Height = static_cast<UINT>(Window_->Height());
    SwapChain_desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    SwapChain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChain_desc.OutputWindow = Window_->NativeHandle();
    SwapChain_desc.SampleDesc.Count = 1;
    SwapChain_desc.Windowed = TRUE;
    SwapChain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL feature_level {};
    constexpr D3D_FEATURE_LEVEL feature_levels[] {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    const HRESULT result = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        feature_levels,
        static_cast<UINT>(std::size(feature_levels)),
        D3D11_SDK_VERSION,
        &SwapChain_desc,
        &SwapChain_,
        &Device_,
        &feature_level,
        &Context_);

    if (SUCCEEDED(result)) {
        BackBufferWidth_ = Window_->Width();
        BackBufferHeight_ = Window_->Height();
        return true;
    }

    return false;
}

bool Dx11Renderer::CreateRenderTarget()
{
    ID3D11Texture2D* back_buffer = nullptr;
    HRESULT result = SwapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back_buffer));
    if (FAILED(result)) {
        return false;
    }

    result = Device_->CreateRenderTargetView(back_buffer, nullptr, &RenderTarget_);
    release_if_set(back_buffer);
    return SUCCEEDED(result);
}

bool Dx11Renderer::CreatePipeline()
{
    ID3DBlob* vertex_blob = nullptr;
    ID3DBlob* pixel_blob = nullptr;
    ID3DBlob* error_blob = nullptr;

    HRESULT result = D3DCompile(shader_source, sizeof(shader_source), nullptr, nullptr, nullptr, "vs_main", "vs_4_0", 0, 0, &vertex_blob, &error_blob);
    release_if_set(error_blob);
    if (FAILED(result)) {
        return false;
    }

    result = D3DCompile(shader_source, sizeof(shader_source), nullptr, nullptr, nullptr, "ps_main", "ps_4_0", 0, 0, &pixel_blob, &error_blob);
    release_if_set(error_blob);
    if (FAILED(result)) {
        release_if_set(vertex_blob);
        return false;
    }

    result = Device_->CreateVertexShader(vertex_blob->GetBufferPointer(), vertex_blob->GetBufferSize(), nullptr, &VertexShader_);
    if (FAILED(result)) {
        release_if_set(vertex_blob);
        release_if_set(pixel_blob);
        return false;
    }

    result = Device_->CreatePixelShader(pixel_blob->GetBufferPointer(), pixel_blob->GetBufferSize(), nullptr, &PixelShader_);
    if (FAILED(result)) {
        release_if_set(vertex_blob);
        release_if_set(pixel_blob);
        return false;
    }

    constexpr D3D11_INPUT_ELEMENT_DESC Input_elements[] {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    result = Device_->CreateInputLayout(Input_elements, static_cast<UINT>(std::size(Input_elements)), vertex_blob->GetBufferPointer(), vertex_blob->GetBufferSize(), &InputLayout_);
    release_if_set(vertex_blob);
    release_if_set(pixel_blob);
    if (FAILED(result)) {
        return false;
    }

    D3D11_BUFFER_DESC constants_desc {};
    constants_desc.ByteWidth = sizeof(constants);
    constants_desc.Usage = D3D11_USAGE_DEFAULT;
    constants_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    result = Device_->CreateBuffer(&constants_desc, nullptr, &ConstantBuffer_);
    if (FAILED(result)) {
        return false;
    }

    D3D11_BLEND_DESC blend_desc {};
    blend_desc.RenderTarget[0].BlendEnable = TRUE;
    blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    result = Device_->CreateBlendState(&blend_desc, &BlendState_);
    if (FAILED(result)) {
        return false;
    }

    D3D11_RASTERIZER_DESC rasterizer_desc {};
    rasterizer_desc.FillMode = D3D11_FILL_SOLID;
    rasterizer_desc.CullMode = D3D11_CULL_NONE;
    rasterizer_desc.ScissorEnable = TRUE;
    rasterizer_desc.DepthClipEnable = TRUE;
    result = Device_->CreateRasterizerState(&rasterizer_desc, &RasterizerState_);
    return SUCCEEDED(result);
}

bool Dx11Renderer::CreateTextPipeline()
{
    release_if_set(D2DRenderTarget_);

    if (D2DFactory_ == nullptr) {
        HRESULT result = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &D2DFactory_);
        if (FAILED(result)) {
            return false;
        }
    }

    if (DWriteFactory_ == nullptr) {
        HRESULT result = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&DWriteFactory_));
        if (FAILED(result)) {
            return false;
        }
    }

    IDXGISurface* surface = nullptr;
    HRESULT result = SwapChain_->GetBuffer(0, __uuidof(IDXGISurface), reinterpret_cast<void**>(&surface));
    if (FAILED(result)) {
        return false;
    }

    const D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED));

    result = D2DFactory_->CreateDxgiSurfaceRenderTarget(surface, &properties, &D2DRenderTarget_);
    release_if_set(surface);
    return SUCCEEDED(result);
}

bool Dx11Renderer::EnsureVertexCapacity(UINT capacity)
{
    if (VertexBuffer_ != nullptr && VertexCapacity_ >= capacity) {
        return true;
    }

    release_if_set(VertexBuffer_);
    VertexCapacity_ = std::max<UINT>(capacity, VertexCapacity_ == 0 ? 1024 : VertexCapacity_ * 2);

    D3D11_BUFFER_DESC desc {};
    desc.ByteWidth = VertexCapacity_ * sizeof(vertex);
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    return SUCCEEDED(Device_->CreateBuffer(&desc, nullptr, &VertexBuffer_));
}

bool Dx11Renderer::EnsureIndexCapacity(UINT capacity)
{
    if (IndexBuffer_ != nullptr && IndexCapacity_ >= capacity) {
        return true;
    }

    release_if_set(IndexBuffer_);
    IndexCapacity_ = std::max<UINT>(capacity, IndexCapacity_ == 0 ? 2048 : IndexCapacity_ * 2);

    D3D11_BUFFER_DESC desc {};
    desc.ByteWidth = IndexCapacity_ * sizeof(std::uint32_t);
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    return SUCCEEDED(Device_->CreateBuffer(&desc, nullptr, &IndexBuffer_));
}

IDWriteTextFormat* Dx11Renderer::TextFormat(const DrawCommand& command)
{
    const std::wstring family = command.FontFamily.empty() ? L"Inter" : command.FontFamily;
    const float size = command.FontSize;

    for (const TextFormatCacheEntry& entry : TextFormatCache_) {
        if (entry.family == family && entry.size == size) {
            return entry.format;
        }
    }

    IDWriteTextFormat* format = nullptr;
    if (DWriteFactory_ == nullptr) {
        return nullptr;
    }

    HRESULT result = DWriteFactory_->CreateTextFormat(
        family.c_str(),
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        size,
        L"",
        &format);

    if (FAILED(result)) {
        return nullptr;
    }

    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    TextFormatCache_.push_back({
        .family = family,
        .size = size,
        .format = format,
    });

    return format;
}

void Dx11Renderer::RenderText(const DrawData& data)
{
    if (D2DRenderTarget_ == nullptr || DWriteFactory_ == nullptr) {
        return;
    }

    bool has_text = false;
    for (const DrawCommand& command : data.Commands) {
        if (command.Type == DrawCommandType::Text && !command.Text.empty()) {
            has_text = true;
            break;
        }
    }

    if (!has_text) {
        return;
    }

    ID3D11RenderTargetView* null_render_target = nullptr;
    Context_->OMSetRenderTargets(1, &null_render_target, nullptr);
    Context_->Flush();

    D2DRenderTarget_->BeginDraw();

    for (const DrawCommand& command : data.Commands) {
        if (command.Type != DrawCommandType::Text || command.Text.empty()) {
            continue;
        }

        IDWriteTextFormat* format = TextFormat(command);
        if (format == nullptr) {
            continue;
        }

        ID2D1SolidColorBrush* brush = nullptr;
        HRESULT result = D2DRenderTarget_->CreateSolidColorBrush(to_d2d(command.Tint), &brush);
        if (FAILED(result)) {
            continue;
        }

        const Rect clip_rect = effective_clip(command.Clip, Window_->Width(), Window_->Height());
        const Rect clipped_bounds = clipped(command.Bounds, clip_rect);
        if (clipped_bounds.Max.X <= clipped_bounds.Min.X || clipped_bounds.Max.Y <= clipped_bounds.Min.Y) {
            release_if_set(brush);
            continue;
        }

        const std::wstring Text = widen(command.Text);
        const D2D1_RECT_F clip = D2D1::RectF(clip_rect.Min.X, clip_rect.Min.Y, clip_rect.Max.X, clip_rect.Max.Y);
        const D2D1_RECT_F bounds = D2D1::RectF(command.Bounds.Min.X, command.Bounds.Min.Y, command.Bounds.Max.X, command.Bounds.Max.Y);
        D2DRenderTarget_->PushAxisAlignedClip(clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        D2DRenderTarget_->DrawText(Text.c_str(), static_cast<UINT32>(Text.size()), format, bounds, brush);
        D2DRenderTarget_->PopAxisAlignedClip();

        release_if_set(brush);
    }

    const HRESULT end_result = D2DRenderTarget_->EndDraw();
    Context_->OMSetRenderTargets(1, &RenderTarget_, nullptr);

    if (end_result == D2DERR_RECREATE_TARGET) {
        release_if_set(D2DRenderTarget_);
        CreateTextPipeline();
    }
}

void Dx11Renderer::ReleaseRenderTarget()
{
    release_if_set(D2DRenderTarget_);
    release_if_set(RenderTarget_);
}

void Dx11Renderer::Release()
{
    for (TextFormatCacheEntry& entry : TextFormatCache_) {
        release_if_set(entry.format);
    }
    TextFormatCache_.clear();

    release_if_set(RasterizerState_);
    release_if_set(BlendState_);
    release_if_set(ConstantBuffer_);
    release_if_set(IndexBuffer_);
    release_if_set(VertexBuffer_);
    release_if_set(InputLayout_);
    release_if_set(PixelShader_);
    release_if_set(VertexShader_);
    ReleaseRenderTarget();
    release_if_set(DWriteFactory_);
    release_if_set(D2DFactory_);
    release_if_set(SwapChain_);
    release_if_set(Context_);
    release_if_set(Device_);
}

}
