// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "resources.h"
#include <array>
#include <memory>
namespace NativeDX12 {
struct GuestTextureDesc;
struct GuestTextureLayout;
class GuestTiler {
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12RootSignature> root_;
    ComPtr<ID3D12PipelineState> pipeline_;
public:
    // The shader addresses bytes with uint. Snapshot buffers are rounded to
    // the default allocation alignment, so keep that full envelope below 4 GiB.
    // This is an addressing limit, not a residency budget.
    static constexpr uint64_t MaxGuestBytes=uint64_t(UINT32_MAX)&
        ~uint64_t(D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT-1);
    static constexpr bool CanAddressGuestBytes(uint64_t bytes){
        return bytes&&!(bytes%4)&&bytes<=MaxGuestBytes;
    }
    struct NativeFormat {
        uint32_t data_format,number_format,guest_bytes,host_bytes,expanded_bytes;
        bool depth;
    };
    static NativeFormat DescribeNativeFormat(const GuestTextureDesc&,const GuestTextureLayout&);
    struct Lease {
        std::shared_ptr<Descriptors> visible,cpu;
        Resource input,output,mask;
        ComPtr<ID3D12RootSignature> root;
        ComPtr<ID3D12PipelineState> pipeline;
        std::array<uint32_t,30> constants;
        bool clear_targets{};
        void Record(ID3D12GraphicsCommandList*,Resource& input,Resource& output,Resource& mask) const;
    };
private:
    std::shared_ptr<Lease> BindBuffers(Resource&,Resource&,Resource&,
        const std::array<uint32_t,30>&,bool);
public:
    explicit GuestTiler(ID3D12Device*);
    static Resource Scratch(ID3D12Device*,uint64_t);
    static uint64_t WrittenBytes(uint64_t bytes){return ((bytes+31)/32)*4;}
    std::shared_ptr<Lease> PrepareMaskedMerge(Resource& input,Resource& output,Resource& mask,uint64_t bytes);
    std::shared_ptr<Lease> Prepare(Resource& input,Resource& output,Resource& mask,
        const GuestTextureDesc&,const GuestTextureLayout&,uint32_t mip,uint32_t first_slice,
        const D3D12_PLACED_SUBRESOURCE_FOOTPRINT&,uint32_t linear_bytes,bool retile,bool clear_targets=false,
        const NativeFormat* source_native_format=nullptr);
    std::shared_ptr<Lease> Record(ID3D12GraphicsCommandList*,Resource& input,Resource& output,
        Resource& mask,const GuestTextureDesc&,const GuestTextureLayout&,uint32_t mip,uint32_t first_slice,
        const D3D12_PLACED_SUBRESOURCE_FOOTPRINT&,uint32_t linear_bytes,bool retile,bool clear_targets=false);
};
} // namespace NativeDX12
