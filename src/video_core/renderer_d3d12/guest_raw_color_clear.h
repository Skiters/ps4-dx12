// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "resources.h"
#include "guest_color_clear.h"
#include <d3dcompiler.h>
#include <map>
#include <memory>
#include <span>
#include <string_view>
#include <tuple>

namespace NativeDX12 {
// Numeric RTV clears cannot preserve arbitrary R32 integers, F32 denormals or
// F16/F32 NaN payloads or a chosen packed-float NaN encoding. A uint output
// through a matching typeless family or actual declared cast view writes the
// decoded storage bits directly, including every sample of a covered pixel.
class GuestRawColorClear {
public:
    struct Program {
        ComPtr<ID3D12RootSignature> root;
        ComPtr<ID3D12PipelineState> pipeline;
        DXGI_FORMAT format{};
    };
private:
    ComPtr<ID3D12Device> device_;
    std::map<std::tuple<DXGI_FORMAT, UINT, UINT>, std::shared_ptr<Program>> programs_;
    static ComPtr<ID3DBlob> Compile(std::string_view source, const char* target) {
        ComPtr<ID3DBlob> code, errors;
        const auto hr = DX12_API_CALL(D3DCompile(source.data(), source.size(), "guest-raw-color-clear",
            nullptr, nullptr, "main", target, D3DCOMPILE_ENABLE_STRICTNESS |
            D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_WARNINGS_ARE_ERRORS, 0, &code, &errors));
        if (FAILED(hr)) throw std::runtime_error(errors ?
            std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize()) :
            "Native raw color clear shader compilation failed");
        return code;
    }
public:
    explicit GuestRawColorClear(ID3D12Device* device) : device_(device) {}
    std::shared_ptr<Program> Prepare(const Resource& resource, uint32_t data_format) {
        if (!resource.handle) return {};
        const auto desc = resource.handle->GetDesc();
        DXGI_FORMAT family{}, format{};
        switch (data_format) {
        case 2: family = DXGI_FORMAT_R16_TYPELESS; format = DXGI_FORMAT_R16_UINT; break;
        case 5: family = DXGI_FORMAT_R16G16_TYPELESS; format = DXGI_FORMAT_R16G16_UINT; break;
        case 12: family = DXGI_FORMAT_R16G16B16A16_TYPELESS; format = DXGI_FORMAT_R16G16B16A16_UINT; break;
        case 4: family = DXGI_FORMAT_R32_TYPELESS; format = DXGI_FORMAT_R32_UINT; break;
        case 6:
            // R11G11B10 has no ordinary typeless UINT family. Only an actual
            // format-list allocation declaring both views permits this raw
            // packed-word RTV; checking a description or byte size is not enough.
            if (!resource.AllowsCast(DXGI_FORMAT_R11G11B10_FLOAT) ||
                !resource.AllowsCast(DXGI_FORMAT_R32_UINT)) return {};
            family = DXGI_FORMAT_R11G11B10_FLOAT; format = DXGI_FORMAT_R32_UINT; break;
        case 11: family = DXGI_FORMAT_R32G32_TYPELESS; format = DXGI_FORMAT_R32G32_UINT; break;
        case 14: family = DXGI_FORMAT_R32G32B32A32_TYPELESS; format = DXGI_FORMAT_R32G32B32A32_UINT; break;
        default: return {};
        }
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.Format != family ||
            !(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)) return {};
        const auto key = std::tuple{format, desc.SampleDesc.Count, desc.SampleDesc.Quality};
        if (const auto found = programs_.find(key); found != programs_.end()) return found->second;
        D3D12_FEATURE_DATA_FORMAT_SUPPORT support{format};
        Check(DX12_API_CALL(device_->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support))));
        if (!(support.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET)) return {};
        if (desc.SampleDesc.Count > 1) {
            D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS quality{
                format, desc.SampleDesc.Count, D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE, 0};
            Check(DX12_API_CALL(device_->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
                &quality, sizeof(quality))));
            if (!quality.NumQualityLevels || desc.SampleDesc.Quality >= quality.NumQualityLevels) return {};
        }
        auto program = std::make_shared<Program>(); program->format = format;
        D3D12_ROOT_PARAMETER parameter{};
        parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameter.Constants = {0, 0, 4}; parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC signature{1, &parameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ComPtr<ID3DBlob> blob, errors;
        Check(DX12_API_CALL(D3D12SerializeRootSignature(&signature, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors)));
        Check(DX12_API_CALL(device_->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
            IID_PPV_ARGS(&program->root))));
        const auto vs = Compile("float4 main(uint id:SV_VertexID):SV_Position {"
            "return float4(id==2?3:-1,id==1?3:-1,0,1);}", "vs_5_1");
        const auto ps = Compile("cbuffer ClearBits:register(b0){uint4 bits;}"
            "uint4 main():SV_Target0{return bits;}", "ps_5_1");
        D3D12_GRAPHICS_PIPELINE_STATE_DESC state{};
        state.pRootSignature = program->root.Get();
        state.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
        state.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
        state.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        state.RasterizerState.DepthClipEnable = TRUE;
        state.RasterizerState.MultisampleEnable = TRUE;
        state.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        state.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        auto& blend = state.BlendState.RenderTarget[0];
        blend.SrcBlend = blend.SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.DestBlend = blend.DestBlendAlpha = D3D12_BLEND_ZERO;
        blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blend.LogicOp = D3D12_LOGIC_OP_NOOP;
        blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        state.SampleMask = UINT_MAX; state.SampleDesc = desc.SampleDesc;
        state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        state.NumRenderTargets = 1; state.RTVFormats[0] = format;
        Check(DX12_API_CALL(device_->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&program->pipeline))));
        programs_.emplace(key, program); return program;
    }
    static void Record(ID3D12GraphicsCommandList* list, const Program& program,
        D3D12_CPU_DESCRIPTOR_HANDLE rtv, UINT width, UINT height,
        std::span<const D3D12_RECT> rectangles, const GuestColorClearValue& clear) {
        std::array<uint32_t, 4> bits{};
        const auto component_bytes = clear.data_format == 2 || clear.data_format == 5 ||
            clear.data_format == 12 ? 2u : 4u;
        for (UINT i = 0; i < clear.pixel_bytes / component_bytes; ++i)
            std::memcpy(&bits[i], clear.pixel.data() + i * component_bytes, component_bytes);
        DX12_API_CALL(list->SetPipelineState(program.pipeline.Get()));
        DX12_API_CALL(list->SetGraphicsRootSignature(program.root.Get()));
        DX12_API_CALL(list->SetGraphicsRoot32BitConstants(0, 4, bits.data(), 0));
        DX12_API_CALL(list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
        const D3D12_VIEWPORT viewport{0, 0, float(width), float(height), 0, 1};
        DX12_API_CALL(list->RSSetViewports(1, &viewport));
        DX12_API_CALL(list->OMSetRenderTargets(1, &rtv, FALSE, nullptr));
        for (const auto& rect : rectangles) {
            DX12_API_CALL(list->RSSetScissorRects(1, &rect));
            DX12_API_CALL(list->DrawInstanced(3, 1, 0, 0));
        }
    }
};
} // namespace NativeDX12
