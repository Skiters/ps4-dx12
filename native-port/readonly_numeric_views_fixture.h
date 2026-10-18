// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "video_core/renderer_d3d12/guest_image_ownership.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include <d3dcompiler.h>
#include <array>
#include <cstring>
#include <cstdio>
namespace NativeDX12 {
inline void RunReadonlyNumericViewsFixture(ID3D12Device* device){
    const auto require=[](bool b,const char* message){if(!b)throw std::runtime_error(message);};
    const char* code=R"(
Texture2DArray<float4> f:register(t0);Texture2DArray<uint4> u:register(t1);RWByteAddressBuffer output:register(u0);
[numthreads(1,1,1)]void main(){output.Store4(0,asuint(f.Load(int4(0,0,0,0))));output.Store4(16,u.Load(int4(0,0,0,0)));})";
    ComPtr<ID3DBlob> shader,errors;const auto hr=D3DCompile(code,strlen(code),"readonly-numeric-probe",nullptr,nullptr,"main","cs_5_1",D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_WARNINGS_ARE_ERRORS,0,&shader,&errors);
    if(FAILED(hr))throw std::runtime_error(errors?std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize()):"Readonly numeric probe compile failed");
    D3D12_DESCRIPTOR_RANGE ranges[2]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,2,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,2}};
    D3D12_ROOT_PARAMETER param{};param.ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;param.DescriptorTable={2,ranges};
    D3D12_ROOT_SIGNATURE_DESC signature{1,&param,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};ComPtr<ID3DBlob> blob;ComPtr<ID3D12RootSignature> root;
    Check(D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors));Check(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root)));
    D3D12_COMPUTE_PIPELINE_STATE_DESC state{};state.pRootSignature=root.Get();state.CS={shader->GetBufferPointer(),shader->GetBufferSize()};ComPtr<ID3D12PipelineState> pipeline;Check(device->CreateComputePipelineState(&state,IID_PPV_ARGS(&pipeline)));
    for(const u32 data:{1u,2u,3u,4u,5u,9u,10u,11u,12u,13u,14u})for(const u32 tile:{0u,14u})for(const bool integer_first:{false,true}){
        printf("CASE: readonly numeric data=%u tile=%u integer_first=%u mips=2 layers=2\n",data,tile,integer_first);
        Rasterizer renderer(device);GuestTextureCache cache(device);cache.SetRecordingListProvider([&]{return renderer.RecordingList();});
        try{
            GuestTextureDesc f;f.address=0x40000;f.width=13;f.height=11;f.pitch=16;f.levels=f.layers=f.view_layers=2;f.tile_mode=tile;f.data_format=data;
            const bool floats=data==4||data==11||data==13||data==14;f.number_format=floats?7:0;
            auto u=f;u.number_format=4;auto selected_f=f,selected_u=u;
            selected_f.first_mip=selected_u.first_mip=1;selected_f.first_layer=selected_u.first_layer=1;selected_f.view_layers=selected_u.view_layers=1;
            const auto layout=LayoutGuestTexture(f);std::array<u32,4> input{},integer{0,0,0,1};
            const u32 one=std::bit_cast<u32>(1.f);u32 red{};
            switch(data){case 1:case 3:red=255;break;case 2:case 5:case 12:red=65535;break;case 9:red=1023;break;case 10:red=255;break;default:red=one;break;}
            input[0]=red;integer[0]=red;
            if(data==9){input[0]|=3u<<30;integer[3]=3;}
            if(data==10){input[0]|=255u<<24;integer[3]=255;}
            if(data==12){input[1]=65535u<<16;integer[3]=65535;}
            if(data==14){input[3]=one;integer[3]=one;}
            std::vector<std::byte> original(size_t(layout.guest_bytes),std::byte{0xa7}),unrelated(size_t(layout.guest_bytes),std::byte{0x39});
            const std::array<u32,4> zero{};
            for(u32 mip=0;mip<2;++mip)for(u32 layer=0;layer<2;++layer)for(u32 y=0;y<std::max(f.height>>mip,1u);++y)for(u32 x=0;x<std::max(f.width>>mip,1u);++x)
                memcpy(original.data()+GuestPixelOffset(f,layout,mip,layer,x,y),(mip==1&&layer==1?input:zero).data(),layout.bytes_per_pixel);
            unsigned transfers{},retirements{};
            cache.SetIndependentImageRetention([&](const auto&,const auto& physical){return physical.guest_bytes==layout.guest_bytes;});
            const auto acquire=[&](auto& image){const auto owner=DescribeGuestImageOwnership(image);if(!owner||!CanRetainGuestImageOwnership(cache,*owner))return false;
                image.buffer_alias_version=true;image.SetGpuOwned(true);return true;};
            cache.SetOwnershipAcquirer(acquire);
            cache.SetIndependentVersionTransfer([&](auto&,u64,u64)->Resource{++transfers;throw std::runtime_error("Readonly compatible view attempted private I conversion");},acquire,[&](auto&){++retirements;});
            const auto source_desc=integer_first?u:f;
            auto& source=cache.Upload(source_desc,original,renderer.RecordingList(),renderer.CompletedFence());
            // A real GPU copy, like original CopyImage, establishes modified I
            // while retaining a readonly cache description/allocation. CPU
            // upload alone must not invent that original GpuModified history.
            Resource copied;copied.state=D3D12_RESOURCE_STATE_COPY_DEST;D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
            const auto native_shape=source.texture.handle->GetDesc();Check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&native_shape,copied.state,nullptr,IID_PPV_ARGS(&copied.handle)));
            source.texture.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);renderer.RecordingList()->CopyResource(copied.handle.Get(),source.texture.handle.Get());
            copied.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);source.texture.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);renderer.RecordingList()->CopyResource(source.texture.handle.Get(),copied.handle.Get());
            renderer.RetainUntilFinish([copied](u64){(void)copied;});
            require(cache.Own(source),"Readonly GPU-copy fixture cannot retain its physical-geometry witness");
            const auto uid=source.image_uid;auto* native=source.texture.handle.Get();auto pin=cache.PinBinding(source);const bool modified=source.logical_gpu_modified;
            require(!(native->GetDesc().Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),"Readonly allocation gained unrequested UAV permission");
            auto& first=cache.Upload(integer_first?f:u,unrelated,renderer.RecordingList(),renderer.CompletedFence());
            require(&first==&source&&cache.Entries().size()==1&&source.image_uid==uid&&source.texture.handle.Get()==native&&source.desc==source_desc&&
                !transfers&&!retirements&&source.logical_gpu_modified==modified,"Readonly numeric view recreated or reclassified the original I");
            const auto fsrv=source.Srv(selected_f),usrv=source.Srv(selected_u);
            auto views=std::make_shared<Descriptors>(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,3,true);auto output=GuestTiler::Scratch(device,32);
            device->CreateShaderResourceView(native,&fsrv,views->Cpu(0));device->CreateShaderResourceView(native,&usrv,views->Cpu(1));
            D3D12_UNORDERED_ACCESS_VIEW_DESC out{};out.Format=DXGI_FORMAT_R32_TYPELESS;out.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;out.Buffer.NumElements=8;out.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
            device->CreateUnorderedAccessView(output.handle.Get(),nullptr,&out,views->Cpu(2));
            auto readback=Buffer(device,32,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);renderer.RetainUntilFinish([root,pipeline,views,output,readback,pin](u64){(void)root;(void)pipeline;(void)views;(void)output;(void)readback;(void)pin;});
            auto* list=renderer.RecordingList();source.texture.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);output.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            ID3D12DescriptorHeap* heaps[]{views->Heap()};list->SetDescriptorHeaps(1,heaps);list->SetComputeRootSignature(root.Get());list->SetPipelineState(pipeline.Get());list->SetComputeRootDescriptorTable(0,views->Gpu(0));list->Dispatch(1,1,1);
            output.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);list->CopyBufferRegion(readback.handle.Get(),0,output.handle.Get(),0,32);cache.Submitted(renderer.Flush());renderer.Finish();cache.Collect(renderer.CompletedFence());
            void* mapped{};Check(readback.handle->Map(0,nullptr,&mapped));std::array<u32,8> actual{};memcpy(actual.data(),mapped,32);D3D12_RANGE none{};readback.handle->Unmap(0,&none);
            const std::array<u32,4> expected_float{one,0,0,one};
            for(u32 c=0;c<4;++c)if(actual[c]!=expected_float[c]||actual[c+4]!=integer[c]){fprintf(stderr,"Readonly numeric data=%u channel=%u float=%08x/%08x uint=%08x/%08x\n",data,c,actual[c],expected_float[c],actual[c+4],integer[c]);throw std::runtime_error("Readonly typed GPU view contents mismatch");}
            renderer.ValidateDebugMessages();
        }catch(...){renderer.Finish();throw;}
    }
    GuestTextureDesc depth;depth.data_format=2;depth.number_format=0;depth.depth_target=true;auto integer_depth=depth;integer_depth.depth_target=false;integer_depth.number_format=4;
    require(!CompatibleGuestShaderStorage(depth,integer_depth),"Readonly numeric sharing admitted a depth/color aspect reinterpretation");
    GuestTextureDesc packed;packed.data_format=9;packed.number_format=1;auto packed_integer=packed;packed_integer.number_format=5;
    require(!CompatibleGuestShaderStorage(packed,packed_integer),"Readonly numeric sharing admitted converted signed packed10 values");
    puts("PASS:44 readonly typeless numeric GPU pairs, original UID/allocation/class/history, selected mip/layer, RGB32 synthetic alpha, no private conversion or extra write permission; depth/expanded negatives");
}
} // namespace NativeDX12
