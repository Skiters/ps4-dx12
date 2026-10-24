// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_tiler.h"
#include "guest_texture_cache.h"
#include "guest_tile_address.h"
#include "guest_tiling_shader.h"
#include <d3dcompiler.h>
#include <algorithm>
#include <cstring>

namespace NativeDX12 {
GuestTiler::GuestTiler(ID3D12Device* device):device_(device){
    if(!device)throw std::invalid_argument("Null guest tiler device");
    ComPtr<ID3DBlob> shader,errors;
    const auto result=D3DCompile(GuestTilingSource.data(),GuestTilingSource.size(),"guest_tiling.hlsl",
        nullptr,nullptr,"main","cs_5_1",D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3|
        D3DCOMPILE_WARNINGS_ARE_ERRORS,0,&shader,&errors);
    if(FAILED(result))throw std::runtime_error("Native guest tiling shader compilation failed: "+
        (errors?std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize()):std::to_string(result)));
    D3D12_DESCRIPTOR_RANGE ranges[2]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0},
        {D3D12_DESCRIPTOR_RANGE_TYPE_UAV,2,0,0,1}};
    D3D12_ROOT_PARAMETER params[2]{};
    params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[0].Constants={0,0,30};params[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
    params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].DescriptorTable={2,ranges};params[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC desc{2,params,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
    ComPtr<ID3DBlob> signature;
    Check(DX12_API_CALL(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&errors)));
    Check(DX12_API_CALL(device->CreateRootSignature(0,signature->GetBufferPointer(),signature->GetBufferSize(),IID_PPV_ARGS(&root_))));
    D3D12_COMPUTE_PIPELINE_STATE_DESC p{};p.pRootSignature=root_.Get();
    p.CS={shader->GetBufferPointer(),shader->GetBufferSize()};
    Check(DX12_API_CALL(device->CreateComputePipelineState(&p,IID_PPV_ARGS(&pipeline_))));
}
Resource GuestTiler::Scratch(ID3D12Device* device,uint64_t size){
    if(!size||size>UINT32_MAX-3)throw std::out_of_range("Native tiling scratch exceeds guest byte addressing");
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width=(size+3)&~uint64_t(3);desc.Height=desc.DepthOrArraySize=desc.MipLevels=1;
    desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    Resource resource;resource.state=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    Check(DX12_API_CALL(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,resource.state,nullptr,IID_PPV_ARGS(&resource.handle))));
    return resource;
}
GuestTiler::NativeFormat GuestTiler::DescribeNativeFormat(const GuestTextureDesc& d,const GuestTextureLayout& l){
    return {d.data_format,d.number_format,l.bytes_per_pixel,l.HostBytesPerPixel(),l.expanded_bytes_per_pixel,d.depth_target};
}
std::shared_ptr<GuestTiler::Lease> GuestTiler::Prepare(
    Resource& input,Resource& output,Resource& mask,const GuestTextureDesc& d,const GuestTextureLayout& l,
    uint32_t mip,uint32_t first_slice,const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& fp,
    uint32_t linear_bytes,bool retile,bool clear_targets,const NativeFormat* source_native_format){
    if(!input.handle||!output.handle||!mask.handle||mip>=d.levels)
        throw std::invalid_argument("Invalid native tiling dispatch");
    const auto& m=l.mips.at(mip);
    const auto width=(std::max(d.width>>mip,1u)+l.block_extent-1)/l.block_extent;
    const auto rows=(std::max(d.height>>mip,1u)+l.block_extent-1)/l.block_extent;
    const auto depth=fp.Footprint.Depth;
    const auto source_bytes=input.handle->GetDesc().Width,dest_bytes=output.handle->GetDesc().Width;
    const uint64_t last_linear=fp.Offset+(uint64_t(rows)*depth*d.samples-1)*fp.Footprint.RowPitch+
        ((uint64_t(width)*linear_bytes+3)&~uint64_t(3));
    const auto spatial=d.volume_depth?std::max(d.volume_depth>>mip,1u):d.layers;
    if(!depth||first_slice>=spatial||depth>spatial-first_slice||input.handle.Get()==output.handle.Get()||
       input.handle.Get()==mask.handle.Get()||(!retile&&clear_targets)||
       (!linear_bytes||linear_bytes>16)||uint64_t(width)*linear_bytes>fp.Footprint.RowPitch||
       l.guest_bytes>UINT32_MAX-3||m.offset>UINT32_MAX||fp.Offset>UINT32_MAX||last_linear>UINT32_MAX||
       source_bytes%4||dest_bytes%4||mask.handle->GetDesc().Width%4||
       source_bytes>UINT32_MAX||dest_bytes>UINT32_MAX||
       (retile?(source_bytes<last_linear||dest_bytes<l.guest_bytes||mask.handle->GetDesc().Width<WrittenBytes(l.guest_bytes)):
               (source_bytes<l.guest_bytes||dest_bytes<last_linear)))
        throw std::out_of_range("Native tiling dispatch storage/footprint mismatch");
    const GuestTileAddress address(AmdGpu::TileMode(d.tile_mode),l.bytes_per_pixel,d.samples);
    if(m.macro&&address.pipe!=AmdGpu::PipeConfig::P2&&address.pipe!=AmdGpu::PipeConfig::P8_32x32_8x16&&
       address.pipe!=AmdGpu::PipeConfig::P8_32x32_16x16)
        throw std::invalid_argument("No pinned upstream tiling pipe implementation");
    // A mapped retile reads SOURCE native words into TARGET guest geometry.
    // Target packed format/RAW32 views must not select the source's codec.
    const auto native=source_native_format?*source_native_format:DescribeNativeFormat(d,l);
    if((source_native_format&&!retile)||native.guest_bytes!=l.bytes_per_pixel)
        throw std::invalid_argument("Invalid mapped native tiling codec");
    const bool depth16=native.depth&&native.guest_bytes==2&&linear_bytes==4;
    if(!depth16&&linear_bytes!=native.host_bytes)throw std::invalid_argument("Unadapted native tiling format conversion");
    const bool signed_packed10=native.data_format==9&&native.expanded_bytes==16;
    const bool expanded_packed16=native.data_format>=16&&native.data_format<=19&&native.expanded_bytes==16;
    const bool packed_number=native.number_format==0||native.number_format==1||native.number_format==4||native.number_format==5;
    if(signed_packed10&&(linear_bytes!=16||!packed_number||(!retile&&native.number_format!=1&&native.number_format!=5)))
        throw std::invalid_argument("Invalid signed packed sampled tiling conversion");
    if(expanded_packed16&&(linear_bytes!=16||native.number_format!=0||
       (!retile&&!HasExactPackedExpansion(d,l))))
        throw std::invalid_argument("Invalid expanded packed volume tiling conversion");
    std::array<uint32_t,30> constants{uint32_t(address.micro),m.thickness,uint32_t(address.array),uint32_t(address.pipe),
        address.pipes,address.bank_width,address.bank_height,address.banks,address.aspect,address.split,
        l.bytes_per_pixel,d.samples,m.pitch,m.height,uint32_t(m.offset),first_slice,uint32_t(m.macro),
        width,rows,fp.Footprint.RowPitch,uint32_t(fp.Offset),uint32_t(retile),d.bank_swizzle,
        linear_bytes,depth,uint32_t(depth16),uint32_t(d.tile_mode==8||d.tile_mode==31),uint32_t(l.guest_bytes),
        native.number_format==7?0x3f800000u:1u,signed_packed10?9u|(native.number_format<<8):
            expanded_packed16?0x10000u|native.data_format:
            native.data_format>=16&&native.data_format<=19?native.data_format:0u};
    return BindBuffers(input,output,mask,constants,clear_targets);
}
std::shared_ptr<GuestTiler::Lease> GuestTiler::PrepareMaskedMerge(
    Resource& input,Resource& output,Resource& mask,uint64_t bytes){
    if(!input.handle||!output.handle||!mask.handle||!CanAddressGuestBytes(bytes)||
       input.handle.Get()==output.handle.Get()||input.handle.Get()==mask.handle.Get()||
       output.handle.Get()==mask.handle.Get())
        throw std::invalid_argument("Invalid packed guest backing merge");
    for(const auto* resource:{&input,&output,&mask}){
        const auto desc=resource->handle->GetDesc();
        if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||desc.Width%4||desc.Width>UINT32_MAX||
           desc.Width<(resource==&mask?WrittenBytes(bytes):bytes))
            throw std::out_of_range("Packed guest backing merge storage mismatch");
    }
    const auto width=uint32_t(std::min<uint64_t>(bytes/4,16384));
    std::array<uint32_t,30> constants{};
    constants[11]=1;constants[17]=width;constants[18]=uint32_t((bytes+uint64_t(width)*4-1)/(uint64_t(width)*4));
    constants[19]=width*4;constants[21]=2;constants[23]=4;constants[24]=1;constants[27]=uint32_t(bytes);
    return BindBuffers(input,output,mask,constants,false);
}
std::shared_ptr<GuestTiler::Lease> GuestTiler::BindBuffers(Resource& input,Resource& output,Resource& mask,
    const std::array<uint32_t,30>& constants,bool clear_targets){
    // Validate every dispatch dimension before allocating descriptors or
    // recording any transitions. Masked merge uses rows of 16384 DWORDs;
    // even the maximum addressable envelope fits in 65535 rows.
    const uint64_t columns=uint64_t(constants[17])*constants[23];
    const uint64_t groups=(columns+255)/256;
    const uint64_t slices=uint64_t(constants[24])*constants[11];
    if(!groups||groups>D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION||
       !constants[18]||constants[18]>D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION||
       !slices||slices>D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION||
       columns>UINT32_MAX-255)
        throw std::out_of_range("Native tiling dispatch exceeds shader/grid addressing");
    const auto source_bytes=input.handle->GetDesc().Width;
    auto lease=std::make_shared<Lease>();
    lease->root=root_;lease->pipeline=pipeline_;
    lease->visible=std::make_shared<Descriptors>(device_.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,3,true);
    lease->cpu=std::make_shared<Descriptors>(device_.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,3,false);
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;
    srv.Format=DXGI_FORMAT_R32_TYPELESS;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Buffer.NumElements=UINT(source_bytes/4);srv.Buffer.Flags=D3D12_BUFFER_SRV_FLAG_RAW;
    DX12_API_CALL(device_->CreateShaderResourceView(input.handle.Get(),&srv,lease->visible->Cpu(0)));
    for(UINT index=1;index<3;++index){
        auto& resource=index==1?output:mask;
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;
        uav.Format=DXGI_FORMAT_R32_TYPELESS;uav.Buffer.NumElements=UINT(resource.handle->GetDesc().Width/4);
        uav.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
        DX12_API_CALL(device_->CreateUnorderedAccessView(resource.handle.Get(),nullptr,&uav,lease->visible->Cpu(index)));
        DX12_API_CALL(device_->CreateUnorderedAccessView(resource.handle.Get(),nullptr,&uav,lease->cpu->Cpu(index)));
    }
    lease->input=input;lease->output=output;lease->mask=mask;
    lease->constants=constants;lease->clear_targets=clear_targets;
    return lease;
}
void GuestTiler::Lease::Record(ID3D12GraphicsCommandList* list,Resource& source,Resource& destination,
                              Resource& written)const{
    if(!list||source.handle.Get()!=input.handle.Get()||destination.handle.Get()!=output.handle.Get()||
       written.handle.Get()!=mask.handle.Get())throw std::invalid_argument("Native tiling dispatch resource changed");
    // Descriptor/resource allocation above is transactional. Record only once
    // the lease can retain every object referenced by this dispatch.
    if(source.state!=D3D12_RESOURCE_STATE_GENERIC_READ)source.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    destination.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    if(written.handle.Get()!=destination.handle.Get())written.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    ID3D12DescriptorHeap* heaps[]{visible->Heap()};DX12_API_CALL(list->SetDescriptorHeaps(1,heaps));
    if(clear_targets){
        const UINT zero[4]{};
        DX12_API_CALL(list->ClearUnorderedAccessViewUint(visible->Gpu(1),cpu->Cpu(1),destination.handle.Get(),zero,0,nullptr));
        DX12_API_CALL(list->ClearUnorderedAccessViewUint(visible->Gpu(2),cpu->Cpu(2),written.handle.Get(),zero,0,nullptr));
    }
    destination.UavBarrier(list);if(written.handle.Get()!=destination.handle.Get())written.UavBarrier(list);
    DX12_API_CALL(list->SetComputeRootSignature(root.Get()));DX12_API_CALL(list->SetPipelineState(pipeline.Get()));
    DX12_API_CALL(list->SetComputeRoot32BitConstants(0,UINT(constants.size()),constants.data(),0));
    DX12_API_CALL(list->SetComputeRootDescriptorTable(1,visible->Gpu(0)));
    DX12_API_CALL(list->Dispatch((constants[17]*constants[23]+255)/256,constants[18],constants[24]*constants[11]));
}
std::shared_ptr<GuestTiler::Lease> GuestTiler::Record(ID3D12GraphicsCommandList* list,
    Resource& input,Resource& output,Resource& mask,const GuestTextureDesc& d,const GuestTextureLayout& l,
    uint32_t mip,uint32_t first_slice,const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& fp,
    uint32_t linear_bytes,bool retile,bool clear_targets){
    auto lease=Prepare(input,output,mask,d,l,mip,first_slice,fp,linear_bytes,retile,clear_targets);
    lease->Record(list,input,output,mask);
    return lease;
}
} // namespace NativeDX12
