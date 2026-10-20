// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "resource_bindings.h"
#include <array>
namespace NativeDX12 {
// The root layout is derived from the same bytecode passed to CreatePipelineState.
// Keep the pipeline and per-submission bindings alive until the GPU fence retires.
class ShaderPipeline {
    ResourceLayout layout_;
    ComPtr<ID3D12PipelineState> state_;
    bool compute_;
    std::array<DXGI_FORMAT,8> targets_{};
    UINT target_count_{},sample_count_{};
    DXGI_FORMAT depth_=DXGI_FORMAT_UNKNOWN;
public:
    struct StencilFaces {
        UINT8 front_read{},front_write{},back_read{},back_write{};
        bool separate{};
    };
    struct GraphicsShaders {
        std::span<const uint8_t> vertex,pixel,hull,domain,geometry;
        bool root_graphics_runtime{};
        std::array<std::span<const uint8_t>,5> All() const {
            return {vertex,pixel,hull,domain,geometry};
        }
    };
    ShaderPipeline(ID3D12Device*,const std::wstring& dxc,
                   std::span<const uint8_t> compute);
    ShaderPipeline(ID3D12Device*,const std::wstring& dxc,const GraphicsShaders&,
                   const D3D12_GRAPHICS_PIPELINE_STATE_DESC& fixed_state,bool depth_bounds=false);
    ShaderPipeline(ID3D12Device*,const std::wstring& dxc,const GraphicsShaders&,
                   const D3D12_GRAPHICS_PIPELINE_STATE_DESC& fixed_state,bool depth_bounds,
                   StencilFaces stencil);
    const ResourceLayout& Layout() const {return layout_;}
    bool IsCompute() const {return compute_;}
    std::span<const DXGI_FORMAT> TargetFormats()const{return {targets_.data(),target_count_};}
    DXGI_FORMAT DepthFormat()const{return depth_;}
    UINT SampleCount()const{return sample_count_;}
    void Bind(ID3D12GraphicsCommandList*,ResourceBindings&) const;
};
}
