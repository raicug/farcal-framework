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
    float position[2] {};
    float color[4] {};
};

struct render_batch {
    rect clip {};
    UINT index_start {};
    UINT index_count {};
};

struct constants {
    float viewport[2] {};
    float padding[2] {};
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

bool same_clip(rect a, rect b)
{
    return a.min.x == b.min.x && a.min.y == b.min.y && a.max.x == b.max.x && a.max.y == b.max.y;
}

void begin_batch(std::vector<render_batch>& batches, rect clip, UINT index_start)
{
    if (!batches.empty() && batches.back().index_count == 0) {
        batches.back().clip = clip;
        batches.back().index_start = index_start;
        return;
    }

    if (!batches.empty() && same_clip(batches.back().clip, clip)) {
        return;
    }

    batches.push_back({
        .clip = clip,
        .index_start = index_start,
        .index_count = 0,
    });
}

void push_rect(std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<render_batch>& batches, rect bounds, rect clip, color tint)
{
    if (bounds.max.x <= bounds.min.x || bounds.max.y <= bounds.min.y) {
        return;
    }

    begin_batch(batches, clip, static_cast<UINT>(indices.size()));

    const std::uint32_t base = static_cast<std::uint32_t>(vertices.size());
    const vertex a {{bounds.min.x, bounds.min.y}, {tint.r, tint.g, tint.b, tint.a}};
    const vertex b {{bounds.max.x, bounds.min.y}, {tint.r, tint.g, tint.b, tint.a}};
    const vertex c {{bounds.max.x, bounds.max.y}, {tint.r, tint.g, tint.b, tint.a}};
    const vertex d {{bounds.min.x, bounds.max.y}, {tint.r, tint.g, tint.b, tint.a}};

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

    batches.back().index_count += 6;
}

color alpha(color value, float multiplier)
{
    value.a *= multiplier;
    return value;
}

void push_quad(std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<render_batch>& batches, vec2 a, vec2 b, vec2 c, vec2 d, rect clip, color ca, color cb, color cc, color cd)
{
    begin_batch(batches, clip, static_cast<UINT>(indices.size()));

    const std::uint32_t base = static_cast<std::uint32_t>(vertices.size());
    vertices.push_back({{a.x, a.y}, {ca.r, ca.g, ca.b, ca.a}});
    vertices.push_back({{b.x, b.y}, {cb.r, cb.g, cb.b, cb.a}});
    vertices.push_back({{c.x, c.y}, {cc.r, cc.g, cc.b, cc.a}});
    vertices.push_back({{d.x, d.y}, {cd.r, cd.g, cd.b, cd.a}});

    indices.push_back(base);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base);
    indices.push_back(base + 2);
    indices.push_back(base + 3);

    batches.back().index_count += 6;
}

rect clipped(rect bounds, rect clip)
{
    return {
        {(std::max)(bounds.min.x, clip.min.x), (std::max)(bounds.min.y, clip.min.y)},
        {(std::min)(bounds.max.x, clip.max.x), (std::min)(bounds.max.y, clip.max.y)},
    };
}

bool empty_clip(rect clip)
{
    return clip.max.x <= clip.min.x || clip.max.y <= clip.min.y;
}

rect viewport_clip(int width, int height)
{
    return {{0.0F, 0.0F}, {static_cast<float>(width), static_cast<float>(height)}};
}

rect effective_clip(rect clip, int width, int height)
{
    return empty_clip(clip) ? viewport_clip(width, height) : clip;
}

void push_line_rect(std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<render_batch>& batches, rect bounds, rect clip, color tint, float thickness)
{
    push_rect(vertices, indices, batches, {bounds.min, {bounds.max.x, bounds.min.y + thickness}}, clip, tint);
    push_rect(vertices, indices, batches, {{bounds.min.x, bounds.max.y - thickness}, bounds.max}, clip, tint);
    push_rect(vertices, indices, batches, {bounds.min, {bounds.min.x + thickness, bounds.max.y}}, clip, tint);
    push_rect(vertices, indices, batches, {{bounds.max.x - thickness, bounds.min.y}, bounds.max}, clip, tint);
}

void push_line_rect_clipped(std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<render_batch>& batches, rect bounds, rect clip, color tint, float thickness)
{
    push_rect(vertices, indices, batches, clipped({bounds.min, {bounds.max.x, bounds.min.y + thickness}}, clip), clip, tint);
    push_rect(vertices, indices, batches, clipped({{bounds.min.x, bounds.max.y - thickness}, bounds.max}, clip), clip, tint);
    push_rect(vertices, indices, batches, clipped({bounds.min, {bounds.min.x + thickness, bounds.max.y}}, clip), clip, tint);
    push_rect(vertices, indices, batches, clipped({{bounds.max.x - thickness, bounds.min.y}, bounds.max}, clip), clip, tint);
}

void push_line_segment(std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<render_batch>& batches, vec2 start, vec2 end, rect clip, color tint, float thickness, float anti_aliasing)
{
    if (empty_clip(clip)) {
        return;
    }

    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.001F) {
        return;
    }

    const float dir_x = dx / length;
    const float dir_y = dy / length;
    const float normal_x = -dir_y;
    const float normal_y = dir_x;
    const float half = (std::max)(0.5F, thickness * 0.5F);
    const float feather = (std::max)(0.0F, anti_aliasing);
    const vec2 a {start.x + normal_x * half, start.y + normal_y * half};
    const vec2 b {end.x + normal_x * half, end.y + normal_y * half};
    const vec2 c {end.x - normal_x * half, end.y - normal_y * half};
    const vec2 d {start.x - normal_x * half, start.y - normal_y * half};

    push_quad(vertices, indices, batches, a, b, c, d, clip, tint, tint, tint, tint);

    if (feather <= 0.0F) {
        return;
    }

    const color transparent = alpha(tint, 0.0F);
    const float outer = half + feather;
    const vec2 outer_a {start.x + normal_x * outer - dir_x * feather, start.y + normal_y * outer - dir_y * feather};
    const vec2 outer_b {end.x + normal_x * outer + dir_x * feather, end.y + normal_y * outer + dir_y * feather};
    const vec2 outer_c {end.x - normal_x * outer + dir_x * feather, end.y - normal_y * outer + dir_y * feather};
    const vec2 outer_d {start.x - normal_x * outer - dir_x * feather, start.y - normal_y * outer - dir_y * feather};

    push_quad(vertices, indices, batches, outer_a, outer_b, b, a, clip, transparent, transparent, tint, tint);
    push_quad(vertices, indices, batches, d, c, outer_c, outer_d, clip, tint, tint, transparent, transparent);
    push_quad(vertices, indices, batches, outer_d, outer_a, a, d, clip, transparent, transparent, tint, tint);
    push_quad(vertices, indices, batches, b, outer_b, outer_c, c, clip, tint, transparent, transparent, tint);
}

void append_command_geometry(const draw_command& command, int width, int height, std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<render_batch>& batches)
{
    switch (command.type) {
    case draw_command_type::filled_rect:
    {
        const rect clip = effective_clip(command.clip, width, height);
        push_rect(vertices, indices, batches, clipped(command.bounds, clip), clip, command.tint);
        break;
    }

    case draw_command_type::rect:
        push_line_rect_clipped(vertices, indices, batches, command.bounds, effective_clip(command.clip, width, height), command.tint, command.thickness);
        break;

    case draw_command_type::line:
        push_line_segment(vertices, indices, batches, command.start, command.end, effective_clip(command.clip, width, height), command.tint, command.thickness, command.anti_aliasing);
        break;

    case draw_command_type::text:
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

D2D1_COLOR_F to_d2d(color value)
{
    return D2D1::ColorF(value.r, value.g, value.b, value.a);
}

}

dx11_renderer::dx11_renderer(window& target)
    : window_(&target)
{
    if (create_device()) {
        create_render_target();
        create_pipeline();
        create_text_pipeline();
    }
}

dx11_renderer::~dx11_renderer()
{
    release();
}

bool dx11_renderer::valid() const
{
    return device_ != nullptr && context_ != nullptr && swap_chain_ != nullptr && render_target_ != nullptr;
}

void dx11_renderer::resize(int width, int height)
{
    if (swap_chain_ == nullptr || width <= 0 || height <= 0) {
        return;
    }

    context_->OMSetRenderTargets(0, nullptr, nullptr);
    release_render_target();

    const HRESULT result = swap_chain_->ResizeBuffers(0, static_cast<UINT>(width), static_cast<UINT>(height), DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(result)) {
        return;
    }

    back_buffer_width_ = width;
    back_buffer_height_ = height;
    create_render_target();
    create_text_pipeline();
}

void dx11_renderer::render(const draw_data& data)
{
    if (!valid()) {
        return;
    }

    if (window_->width() != back_buffer_width_ || window_->height() != back_buffer_height_) {
        resize(window_->width(), window_->height());
    }

    constexpr float clear_color[] {0.090F, 0.102F, 0.122F, 1.0F};
    context_->OMSetRenderTargets(1, &render_target_, nullptr);
    context_->ClearRenderTargetView(render_target_, clear_color);

    if (vertex_shader_ == nullptr || pixel_shader_ == nullptr || input_layout_ == nullptr) {
        return;
    }

    std::vector<draw_command> pending_text;
    std::vector<vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<render_batch> batches;

    vertices.reserve(data.commands.size() * 4);
    indices.reserve(data.commands.size() * 6);

    auto flush_geometry = [&]() {
        if (vertices.empty() || indices.empty()) {
            return;
        }

        if (!ensure_vertex_capacity(static_cast<UINT>(vertices.size())) || !ensure_index_capacity(static_cast<UINT>(indices.size()))) {
            vertices.clear();
            indices.clear();
            batches.clear();
            return;
        }

        D3D11_MAPPED_SUBRESOURCE mapped {};
        if (FAILED(context_->Map(vertex_buffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            vertices.clear();
            indices.clear();
            batches.clear();
            return;
        }
        std::memcpy(mapped.pData, vertices.data(), vertices.size() * sizeof(vertex));
        context_->Unmap(vertex_buffer_, 0);

        if (FAILED(context_->Map(index_buffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            vertices.clear();
            indices.clear();
            batches.clear();
            return;
        }
        std::memcpy(mapped.pData, indices.data(), indices.size() * sizeof(std::uint32_t));
        context_->Unmap(index_buffer_, 0);

        const UINT stride = sizeof(vertex);
        const UINT offset = 0;
        context_->IASetVertexBuffers(0, 1, &vertex_buffer_, &stride, &offset);
        context_->IASetIndexBuffer(index_buffer_, DXGI_FORMAT_R32_UINT, 0);

        for (const render_batch& batch : batches) {
            if (batch.index_count == 0) {
                continue;
            }

            D3D11_RECT scissor {
                static_cast<LONG>((std::max)(0.0F, batch.clip.min.x)),
                static_cast<LONG>((std::max)(0.0F, batch.clip.min.y)),
                static_cast<LONG>((std::min)(static_cast<float>(window_->width()), batch.clip.max.x)),
                static_cast<LONG>((std::min)(static_cast<float>(window_->height()), batch.clip.max.y)),
            };

            if (scissor.right <= scissor.left || scissor.bottom <= scissor.top) {
                continue;
            }

            context_->RSSetScissorRects(1, &scissor);
            context_->DrawIndexed(batch.index_count, batch.index_start, 0);
        }

        vertices.clear();
        indices.clear();
        batches.clear();
    };

    auto flush_text = [&]() {
        if (pending_text.empty()) {
            return;
        }

        draw_data text_data {};
        text_data.commands = pending_text;
        render_text(text_data);
        pending_text.clear();
    };

    constants constant_data {{static_cast<float>(window_->width()), static_cast<float>(window_->height())}, {0.0F, 0.0F}};
    context_->UpdateSubresource(constant_buffer_, 0, nullptr, &constant_data, 0, 0);

    D3D11_VIEWPORT viewport {};
    viewport.Width = static_cast<float>(window_->width());
    viewport.Height = static_cast<float>(window_->height());
    viewport.MinDepth = 0.0F;
    viewport.MaxDepth = 1.0F;

    const float blend_factor[4] {};
    context_->RSSetViewports(1, &viewport);
    context_->RSSetState(rasterizer_state_);
    context_->OMSetBlendState(blend_state_, blend_factor, 0xFFFFFFFF);
    context_->IASetInputLayout(input_layout_);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(vertex_shader_, nullptr, 0);
    context_->VSSetConstantBuffers(0, 1, &constant_buffer_);
    context_->PSSetShader(pixel_shader_, nullptr, 0);

    for (const draw_command& command : data.commands) {
        if (command.type == draw_command_type::text) {
            flush_geometry();
            pending_text.push_back(command);
        } else {
            flush_text();
            append_command_geometry(command, window_->width(), window_->height(), vertices, indices, batches);
        }
    }

    flush_geometry();
    flush_text();
}

void dx11_renderer::present(bool vertical_sync)
{
    if (swap_chain_ != nullptr) {
        swap_chain_->Present(vertical_sync ? 1U : 0U, 0);
    }
}

bool dx11_renderer::create_device()
{
    DXGI_SWAP_CHAIN_DESC swap_chain_desc {};
    swap_chain_desc.BufferCount = 2;
    swap_chain_desc.BufferDesc.Width = static_cast<UINT>(window_->width());
    swap_chain_desc.BufferDesc.Height = static_cast<UINT>(window_->height());
    swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.OutputWindow = window_->native_handle();
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.Windowed = TRUE;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

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
        &swap_chain_desc,
        &swap_chain_,
        &device_,
        &feature_level,
        &context_);

    if (SUCCEEDED(result)) {
        back_buffer_width_ = window_->width();
        back_buffer_height_ = window_->height();
        return true;
    }

    return false;
}

bool dx11_renderer::create_render_target()
{
    ID3D11Texture2D* back_buffer = nullptr;
    HRESULT result = swap_chain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back_buffer));
    if (FAILED(result)) {
        return false;
    }

    result = device_->CreateRenderTargetView(back_buffer, nullptr, &render_target_);
    release_if_set(back_buffer);
    return SUCCEEDED(result);
}

bool dx11_renderer::create_pipeline()
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

    result = device_->CreateVertexShader(vertex_blob->GetBufferPointer(), vertex_blob->GetBufferSize(), nullptr, &vertex_shader_);
    if (FAILED(result)) {
        release_if_set(vertex_blob);
        release_if_set(pixel_blob);
        return false;
    }

    result = device_->CreatePixelShader(pixel_blob->GetBufferPointer(), pixel_blob->GetBufferSize(), nullptr, &pixel_shader_);
    if (FAILED(result)) {
        release_if_set(vertex_blob);
        release_if_set(pixel_blob);
        return false;
    }

    constexpr D3D11_INPUT_ELEMENT_DESC input_elements[] {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    result = device_->CreateInputLayout(input_elements, static_cast<UINT>(std::size(input_elements)), vertex_blob->GetBufferPointer(), vertex_blob->GetBufferSize(), &input_layout_);
    release_if_set(vertex_blob);
    release_if_set(pixel_blob);
    if (FAILED(result)) {
        return false;
    }

    D3D11_BUFFER_DESC constants_desc {};
    constants_desc.ByteWidth = sizeof(constants);
    constants_desc.Usage = D3D11_USAGE_DEFAULT;
    constants_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    result = device_->CreateBuffer(&constants_desc, nullptr, &constant_buffer_);
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
    result = device_->CreateBlendState(&blend_desc, &blend_state_);
    if (FAILED(result)) {
        return false;
    }

    D3D11_RASTERIZER_DESC rasterizer_desc {};
    rasterizer_desc.FillMode = D3D11_FILL_SOLID;
    rasterizer_desc.CullMode = D3D11_CULL_NONE;
    rasterizer_desc.ScissorEnable = TRUE;
    rasterizer_desc.DepthClipEnable = TRUE;
    result = device_->CreateRasterizerState(&rasterizer_desc, &rasterizer_state_);
    return SUCCEEDED(result);
}

bool dx11_renderer::create_text_pipeline()
{
    release_if_set(d2d_render_target_);

    if (d2d_factory_ == nullptr) {
        HRESULT result = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_factory_);
        if (FAILED(result)) {
            return false;
        }
    }

    if (dwrite_factory_ == nullptr) {
        HRESULT result = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&dwrite_factory_));
        if (FAILED(result)) {
            return false;
        }
    }

    IDXGISurface* surface = nullptr;
    HRESULT result = swap_chain_->GetBuffer(0, __uuidof(IDXGISurface), reinterpret_cast<void**>(&surface));
    if (FAILED(result)) {
        return false;
    }

    const D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED));

    result = d2d_factory_->CreateDxgiSurfaceRenderTarget(surface, &properties, &d2d_render_target_);
    release_if_set(surface);
    return SUCCEEDED(result);
}

bool dx11_renderer::ensure_vertex_capacity(UINT capacity)
{
    if (vertex_buffer_ != nullptr && vertex_capacity_ >= capacity) {
        return true;
    }

    release_if_set(vertex_buffer_);
    vertex_capacity_ = std::max<UINT>(capacity, vertex_capacity_ == 0 ? 1024 : vertex_capacity_ * 2);

    D3D11_BUFFER_DESC desc {};
    desc.ByteWidth = vertex_capacity_ * sizeof(vertex);
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    return SUCCEEDED(device_->CreateBuffer(&desc, nullptr, &vertex_buffer_));
}

bool dx11_renderer::ensure_index_capacity(UINT capacity)
{
    if (index_buffer_ != nullptr && index_capacity_ >= capacity) {
        return true;
    }

    release_if_set(index_buffer_);
    index_capacity_ = std::max<UINT>(capacity, index_capacity_ == 0 ? 2048 : index_capacity_ * 2);

    D3D11_BUFFER_DESC desc {};
    desc.ByteWidth = index_capacity_ * sizeof(std::uint32_t);
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    return SUCCEEDED(device_->CreateBuffer(&desc, nullptr, &index_buffer_));
}

IDWriteTextFormat* dx11_renderer::text_format(const draw_command& command)
{
    const std::wstring family = command.font_family.empty() ? L"Inter" : command.font_family;
    const float size = command.font_size;

    for (const text_format_cache_entry& entry : text_format_cache_) {
        if (entry.family == family && entry.size == size) {
            return entry.format;
        }
    }

    IDWriteTextFormat* format = nullptr;
    if (dwrite_factory_ == nullptr) {
        return nullptr;
    }

    HRESULT result = dwrite_factory_->CreateTextFormat(
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

    text_format_cache_.push_back({
        .family = family,
        .size = size,
        .format = format,
    });

    return format;
}

void dx11_renderer::render_text(const draw_data& data)
{
    if (d2d_render_target_ == nullptr || dwrite_factory_ == nullptr) {
        return;
    }

    bool has_text = false;
    for (const draw_command& command : data.commands) {
        if (command.type == draw_command_type::text && !command.text.empty()) {
            has_text = true;
            break;
        }
    }

    if (!has_text) {
        return;
    }

    ID3D11RenderTargetView* null_render_target = nullptr;
    context_->OMSetRenderTargets(1, &null_render_target, nullptr);
    context_->Flush();

    d2d_render_target_->BeginDraw();

    for (const draw_command& command : data.commands) {
        if (command.type != draw_command_type::text || command.text.empty()) {
            continue;
        }

        IDWriteTextFormat* format = text_format(command);
        if (format == nullptr) {
            continue;
        }

        ID2D1SolidColorBrush* brush = nullptr;
        HRESULT result = d2d_render_target_->CreateSolidColorBrush(to_d2d(command.tint), &brush);
        if (FAILED(result)) {
            continue;
        }

        const rect clip_rect = effective_clip(command.clip, window_->width(), window_->height());
        const rect clipped_bounds = clipped(command.bounds, clip_rect);
        if (clipped_bounds.max.x <= clipped_bounds.min.x || clipped_bounds.max.y <= clipped_bounds.min.y) {
            release_if_set(brush);
            continue;
        }

        const std::wstring text = widen(command.text);
        const D2D1_RECT_F clip = D2D1::RectF(clip_rect.min.x, clip_rect.min.y, clip_rect.max.x, clip_rect.max.y);
        const D2D1_RECT_F bounds = D2D1::RectF(command.bounds.min.x, command.bounds.min.y, command.bounds.max.x, command.bounds.max.y);
        d2d_render_target_->PushAxisAlignedClip(clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        d2d_render_target_->DrawText(text.c_str(), static_cast<UINT32>(text.size()), format, bounds, brush);
        d2d_render_target_->PopAxisAlignedClip();

        release_if_set(brush);
    }

    const HRESULT end_result = d2d_render_target_->EndDraw();
    context_->OMSetRenderTargets(1, &render_target_, nullptr);

    if (end_result == D2DERR_RECREATE_TARGET) {
        release_if_set(d2d_render_target_);
        create_text_pipeline();
    }
}

void dx11_renderer::release_render_target()
{
    release_if_set(d2d_render_target_);
    release_if_set(render_target_);
}

void dx11_renderer::release()
{
    for (text_format_cache_entry& entry : text_format_cache_) {
        release_if_set(entry.format);
    }
    text_format_cache_.clear();

    release_if_set(rasterizer_state_);
    release_if_set(blend_state_);
    release_if_set(constant_buffer_);
    release_if_set(index_buffer_);
    release_if_set(vertex_buffer_);
    release_if_set(input_layout_);
    release_if_set(pixel_shader_);
    release_if_set(vertex_shader_);
    release_render_target();
    release_if_set(dwrite_factory_);
    release_if_set(d2d_factory_);
    release_if_set(swap_chain_);
    release_if_set(context_);
    release_if_set(device_);
}

}
