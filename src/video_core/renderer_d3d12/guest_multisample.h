// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "resources.h"
#include <map>
#include <tuple>
#include <span>
#include <vector>
namespace NativeDX12 {
struct GuestTextureDesc;
struct GuestTextureLayout;
// Transfer every sample independently, using native typed buffer conversion.
// The linear buffer uses the guest format and sample-major rows. sRGB storage
// is transferred through its UNORM view so the transfer does not apply a gamma
// conversion. Integer formats keep integer values throughout both shaders.
class GuestMultisample {
    struct Program {
        ComPtr<ID3D12RootSignature> root;
        ComPtr<ID3D12PipelineState> pipeline;
        std::array<ComPtr<ID3D12PipelineState>,8> stencil_bits;
        bool stencil_bit_upload=false;
    };
    ComPtr<ID3D12Device> device_;
    using Key=std::tuple<DXGI_FORMAT,DXGI_FORMAT,UINT,bool,bool,bool>;
    std::map<Key,std::shared_ptr<Program>> programs_;
    std::map<std::tuple<DXGI_FORMAT,UINT,bool>,std::shared_ptr<Program>> copy_programs_;
public:
    struct Lease {
        Resource texture,linear;
        std::shared_ptr<Descriptors> descriptors,attachment;
        std::shared_ptr<Program> program;
        std::array<UINT,4> constants;
        bool upload,depth,stencil;
        UINT layer;
        void Record(ID3D12GraphicsCommandList*,Resource& texture,Resource& linear) const;
    };
    struct CopyLease {
        Resource source,destination;
        std::shared_ptr<Descriptors> descriptors,attachment;
        std::shared_ptr<Program> program;
        UINT width,height,layer;
        void Record(ID3D12GraphicsCommandList*,Resource& source,Resource& destination)const;
    };
    struct StorageLease {
        Resource texture,linear;
        std::vector<Resource> samples;
        std::vector<std::shared_ptr<Lease>> reads,writes;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
        void Record(ID3D12GraphicsCommandList*,Resource& texture,std::span<Resource> samples,
                    Resource& linear,bool writeback)const;
    };
    explicit GuestMultisample(ID3D12Device* device):device_(device){}
    static D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint(const GuestTextureDesc&,
                                                     const GuestTextureLayout&);
    static uint64_t LinearBytes(const GuestTextureDesc&,const D3D12_PLACED_SUBRESOURCE_FOOTPRINT&);
    std::shared_ptr<Lease> Prepare(Resource& texture,Resource& linear,
        const GuestTextureDesc&,const GuestTextureLayout&,UINT layer,UINT plane,bool upload);
    std::shared_ptr<CopyLease> PrepareCopy(Resource& source,Resource& destination,
        const GuestTextureDesc&,const GuestTextureLayout&,UINT layer);
    std::shared_ptr<StorageLease> PrepareStorage(Resource& texture,std::span<Resource> samples,
        Resource& linear,const GuestTextureDesc&,const GuestTextureLayout&);
};
} // namespace NativeDX12
