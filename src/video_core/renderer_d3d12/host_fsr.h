// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "resources.h"
#include <array>
#include <memory>
namespace NativeDX12 {
struct FsrSettings {bool enable{},use_rcas{};float rcas_attenuation{};};
class FsrPass {
    struct Images {Resource intermediary,output;UINT width{},height{};};
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12RootSignature> root_;
    ComPtr<ID3D12PipelineState> easu_,rcas_;
    std::array<std::shared_ptr<Images>,3> images_;
public:
    struct Lease {
        Resource source;
        std::shared_ptr<Images> images;
        std::shared_ptr<Descriptors> descriptors;
        ComPtr<ID3D12RootSignature> root;
        ComPtr<ID3D12PipelineState> easu,rcas;
        std::array<UINT,20> easu_constants{},rcas_constants{};
        bool sharpen{};
        void Record(ID3D12GraphicsCommandList*,Resource& source)const;
        Resource& Output()const{return images->output;}
    };
    explicit FsrPass(ID3D12Device*);
    std::shared_ptr<Lease> Prepare(UINT slot,Resource& source,DXGI_FORMAT view_format,UINT mapping,
                                 UINT width,UINT height,FsrSettings settings,bool hdr);
};
}
