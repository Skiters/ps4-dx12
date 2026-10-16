// SPDX-License-Identifier: GPL-2.0-or-later
#include "host_fsr.h"
#include "texture_resources.h"
#include "host_fsr_Easu.h"
#include "host_fsr_Rcas.h"
#include <cmath>
#define A_CPU
#include "video_core/host_shaders/fsr/ffx_a.h"
#include "video_core/host_shaders/fsr/ffx_fsr1.h"
namespace NativeDX12 {
FsrPass::FsrPass(ID3D12Device* device):device_(device){
    D3D12_FEATURE_DATA_D3D12_OPTIONS4 options{};
    Check(DX12_API_CALL(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS4,&options,sizeof(options))));
    D3D12_FEATURE_DATA_SHADER_MODEL model{D3D_SHADER_MODEL_6_2};
    Check(DX12_API_CALL(device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,&model,sizeof(model))));
    if(!options.Native16BitShaderOpsSupported||model.HighestShaderModel<D3D_SHADER_MODEL_6_2)
        throw std::runtime_error("Original FSR half-precision path requires native 16-bit shader operations");
    D3D12_DESCRIPTOR_RANGE ranges[]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0},
                                  {D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,1}};
    D3D12_ROOT_PARAMETER parameters[2]{};
    parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[0].DescriptorTable={2,ranges};
    parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;parameters[1].Constants={0,0,20};
    D3D12_STATIC_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MinLOD=-1000;sampler.MaxLOD=1000;sampler.MaxAnisotropy=1;
    sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;
    D3D12_ROOT_SIGNATURE_DESC signature{2,parameters,1,&sampler,D3D12_ROOT_SIGNATURE_FLAG_NONE};
    ComPtr<ID3DBlob> blob,errors;
    Check(DX12_API_CALL(D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors)));
    Check(DX12_API_CALL(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root_))));
    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline{};pipeline.pRootSignature=root_.Get();
    pipeline.CS={HostFsrEasu,sizeof(HostFsrEasu)};
    Check(DX12_API_CALL(device->CreateComputePipelineState(&pipeline,IID_PPV_ARGS(&easu_))));
    pipeline.CS={HostFsrRcas,sizeof(HostFsrRcas)};
    Check(DX12_API_CALL(device->CreateComputePipelineState(&pipeline,IID_PPV_ARGS(&rcas_))));
}
std::shared_ptr<FsrPass::Lease> FsrPass::Prepare(UINT slot,Resource& source,DXGI_FORMAT format,UINT mapping,
    UINT width,UINT height,FsrSettings settings,bool hdr){
    const auto input=source.handle->GetDesc();
    if(slot>=images_.size()||!width||!height||!std::isfinite(settings.rcas_attenuation)||
       input.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||input.SampleDesc.Count!=1||
       input.DepthOrArraySize!=1||input.MipLevels!=1)
        throw std::invalid_argument("Invalid native FSR source, output or settings");
    auto& images=images_[slot];
    if(!images||images->width!=width||images->height!=height){
        auto value=std::make_shared<Images>();value->width=width;value->height=height;
        value->intermediary=Texture2D(device_.Get(),width,height,DXGI_FORMAT_R16G16B16A16_FLOAT,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COMMON);
        value->output=Texture2D(device_.Get(),width,height,DXGI_FORMAT_R16G16B16A16_FLOAT,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COMMON);
        images=std::move(value);
    }
    auto lease=std::make_shared<Lease>();lease->source=source;lease->images=images;
    lease->root=root_;lease->easu=easu_;lease->rcas=rcas_;lease->sharpen=settings.use_rcas;
    lease->descriptors=std::make_shared<Descriptors>(device_.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,4,true);
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=format;srv.Shader4ComponentMapping=mapping;
    srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Texture2D.MipLevels=1;
    DX12_API_CALL(device_->CreateShaderResourceView(source.handle.Get(),&srv,lease->descriptors->Cpu(0)));
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
    uav.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;
    DX12_API_CALL(device_->CreateUnorderedAccessView(settings.use_rcas?images->intermediary.handle.Get():images->output.handle.Get(),
        nullptr,&uav,lease->descriptors->Cpu(1)));
    srv.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    DX12_API_CALL(device_->CreateShaderResourceView(images->intermediary.handle.Get(),&srv,lease->descriptors->Cpu(2)));
    DX12_API_CALL(device_->CreateUnorderedAccessView(images->output.handle.Get(),nullptr,&uav,lease->descriptors->Cpu(3)));
    auto& constants=lease->easu_constants;
    FsrEasuCon(constants.data(),constants.data()+4,constants.data()+8,constants.data()+12,
        float(input.Width),float(input.Height),float(input.Width),float(input.Height),float(width),float(height));
    constants[16]=hdr&&!settings.use_rcas;
    FsrRcasCon(lease->rcas_constants.data(),settings.rcas_attenuation);
    lease->rcas_constants[16]=hdr;
    return lease;
}
void FsrPass::Lease::Record(ID3D12GraphicsCommandList* list,Resource& input)const{
    if(input.handle.Get()!=source.handle.Get())throw std::invalid_argument("Native FSR source changed");
    input.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    auto& first=sharpen?images->intermediary:images->output;
    first.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);first.UavBarrier(list);
    ID3D12DescriptorHeap* heaps[]{descriptors->Heap()};DX12_API_CALL(list->SetDescriptorHeaps(1,heaps));
    DX12_API_CALL(list->SetComputeRootSignature(root.Get()));DX12_API_CALL(list->SetPipelineState(easu.Get()));
    DX12_API_CALL(list->SetComputeRootDescriptorTable(0,descriptors->Gpu(0)));
    DX12_API_CALL(list->SetComputeRoot32BitConstants(1,20,easu_constants.data(),0));
    DX12_API_CALL(list->Dispatch((images->width+15)/16,(images->height+15)/16,1));
    first.UavBarrier(list);
    if(sharpen){
        images->intermediary.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        images->output.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);images->output.UavBarrier(list);
        DX12_API_CALL(list->SetPipelineState(rcas.Get()));DX12_API_CALL(list->SetComputeRootDescriptorTable(0,descriptors->Gpu(2)));
        DX12_API_CALL(list->SetComputeRoot32BitConstants(1,20,rcas_constants.data(),0));
        DX12_API_CALL(list->Dispatch((images->width+15)/16,(images->height+15)/16,1));images->output.UavBarrier(list);
    }
    images->output.Transition(list,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}
}
