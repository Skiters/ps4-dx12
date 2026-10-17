// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include "video_core/renderer_d3d12/multisample_readback.h"
#include <bit>
#include <cstring>
#include <string>

namespace NativeDX12 {
inline void RunPairedDepthSnapshotFixture(ID3D12Device* device) {
    const auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
    Check(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,&options,sizeof(options)));
    unsigned executed{};
    for(const UINT data_format:{2u,4u})for(const UINT tile:{0u,8u})for(const UINT samples:{1u,4u}){
        if(samples>1){
            D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS quality{DXGI_FORMAT_D32_FLOAT_S8X24_UINT,samples,
                D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE,0};
            Check(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,&quality,sizeof(quality)));
            if(!quality.NumQualityLevels){
                printf("SKIP: paired MSAA depth/stencil hardware contract format=%u samples=%u\n",data_format,samples);continue;
            }
        }
        GuestTextureCache cache(device);Rasterizer renderer(device);
        GuestTextureDesc desc;desc.address=0x40000;desc.stencil_address=0x80000;
        desc.width=13;desc.height=11;desc.pitch=16;desc.layers=desc.view_layers=2;
        desc.depth_target=true;desc.data_format=data_format;desc.number_format=data_format==2?0:7;
        desc.samples=samples;desc.tile_mode=tile;
        const auto layout=LayoutGuestTexture(desc);const auto stencil_desc=DescribeGuestStencil(desc);
        const auto stencil_layout=LayoutGuestTexture(stencil_desc);
        require(GuestTextureCache::CanUploadGpuSnapshot(desc,layout),"Paired depth GPU input was structurally rejected");
        const auto depth_code=[](UINT phase,UINT layer,UINT sample,UINT x,UINT y){
            return 1u+(phase*8111u+layer*12347u+sample*7919u+y*13u+x*37u)%65533u;
        };
        const auto depth_bits=[&](UINT phase,UINT layer,UINT sample,UINT x,UINT y){
            return data_format==2?depth_code(phase,layer,sample,x,y):
                std::bit_cast<u32>(float(phase*1024u+layer*256u+sample*64u+y*13u+x+1u)/16384.0f);
        };
        const auto stencil_code=[](UINT phase,UINT layer,UINT sample,UINT x,UINT y){
            return (phase*53u+layer*17u+sample*31u+y*13u+x*11u)%256u;
        };
        const auto physical=[&](bool stencil,UINT phase){
            const auto& d=stencil?stencil_desc:desc;const auto& l=stencil?stencil_layout:layout;
            std::vector<std::byte> bytes(size_t(l.guest_bytes),stencil?std::byte{0xab}:std::byte{0xcd});
            for(UINT layer=0;layer<desc.layers;++layer)for(UINT sample=0;sample<samples;++sample)
            for(UINT y=0;y<desc.height;++y)for(UINT x=0;x<desc.width;++x){
                const u32 value=stencil?stencil_code(phase,layer,sample,x,y):depth_bits(phase,layer,sample,x,y);
                const auto at=GuestPixelOffset(d,l,0,layer,x,y,sample);
                memcpy(bytes.data()+at,&value,l.bytes_per_pixel);
            }
            return bytes;
        };
        const auto gpu_copy=[&](const std::vector<std::byte>& bytes){
            const auto capacity=(bytes.size()+65535)&~size_t(65535);
            auto upload=Buffer(device,capacity,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
            auto result=Buffer(device,capacity,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
            void* mapped{};D3D12_RANGE empty{};Check(upload.handle->Map(0,&empty,&mapped));
            memset(mapped,0x79,capacity);memcpy(mapped,bytes.data(),bytes.size());upload.handle->Unmap(0,nullptr);
            renderer.RetainUntilFinish([upload,result](u64){(void)upload;(void)result;});
            renderer.RecordingList()->CopyBufferRegion(result.handle.Get(),0,upload.handle.Get(),0,capacity);
            result.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);return result;
        };
        for(UINT mode=0;mode<3;++mode){
            // Both GPU; CPU depth + GPU stencil; GPU depth + CPU stencil.
            // Values change every reuse and each GPU B1 differs from CPU B0.
            const UINT cpu_phase=mode+1,gpu_phase=mode+5;
            auto depth_cpu=physical(false,cpu_phase),stencil_cpu=physical(true,cpu_phase);
            const auto original_depth_cpu=depth_cpu,original_stencil_cpu=stencil_cpu;
            const auto depth_gpu_bytes=physical(false,gpu_phase),stencil_gpu_bytes=physical(true,gpu_phase);
            auto depth_gpu=gpu_copy(depth_gpu_bytes),stencil_gpu=gpu_copy(stencil_gpu_bytes);
            const bool gpu_depth=mode!=1,gpu_stencil=mode!=2;
            if(mode==0){
                auto invalid=stencil_gpu;invalid.state=D3D12_RESOURCE_STATE_COPY_SOURCE;
                bool rejected=false;
                try{cache.Upload(desc,{},renderer.RecordingList(),renderer.CompletedFence(),{},true,&depth_gpu,
                    false,false,GuestTextureCache::UploadPurpose::Binding,&invalid);}catch(const std::invalid_argument&){rejected=true;}
                require(rejected&&cache.Entries().empty(),"Wrong-state stencil input changed the image cache");
                auto short_stencil=Buffer(device,4,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                rejected=false;
                try{cache.Upload(desc,depth_cpu,renderer.RecordingList(),renderer.CompletedFence(),{},true,nullptr,
                    false,false,GuestTextureCache::UploadPurpose::Binding,&short_stencil);}catch(const std::invalid_argument&){rejected=true;}
                require(rejected&&cache.Entries().empty(),"Short stencil input changed the image cache");
                rejected=false;
                try{cache.Upload(desc,{},renderer.RecordingList(),renderer.CompletedFence(),{},true,nullptr,
                    false,false,GuestTextureCache::UploadPurpose::Binding,&stencil_gpu);}catch(const std::invalid_argument&){rejected=true;}
                require(rejected&&cache.Entries().empty(),"Stencil snapshot hid a missing depth input");
                // Replace complete CPU equality proofs and UPLOAD input pools
                // with GPU planes, then switch each individual input back.
                cache.Upload(desc,depth_cpu,renderer.RecordingList(),renderer.CompletedFence(),stencil_cpu,true);
                const auto initialized=renderer.Flush();cache.Submitted(initialized);renderer.Finish();cache.Collect(renderer.CompletedFence());
            }
            auto& image=cache.Upload(desc,gpu_depth?std::span<const std::byte>{}:std::span<const std::byte>(depth_cpu),
                renderer.RecordingList(),renderer.CompletedFence(),
                gpu_stencil?std::span<const std::byte>{}:std::span<const std::byte>(stencil_cpu),true,
                gpu_depth?&depth_gpu:nullptr,false,false,GuestTextureCache::UploadPurpose::Binding,
                gpu_stencil?&stencil_gpu:nullptr);
            require(image.sampled_snapshot.empty()&&image.stencil_snapshot.empty(),"GPU plane was cached as a CPU equality witness");
            // Ordinary GPU-owned I is a legal private snapshot source. It
            // must not need a CPU download or independent multi-plane flags.
            image.NoteLogicalImageWrite();image.SetGpuOwned(true);
            require(!image.buffer_alias_version,"Private capture silently acquired independent ownership");
            struct Gate {ComPtr<ID3D12Fence> fence;~Gate(){if(fence)fence->Signal(1);}} gate;
            Check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&gate.fence)));
            renderer.WaitExternalFence(gate.fence.Get(),1);
            if(mode==0){
                bool rejected=false;
                try{cache.CaptureDepthPlane(image,2,depth_gpu,renderer.RecordingList(),renderer.CompletedFence());}
                catch(const std::invalid_argument&){rejected=true;}
                require(rejected,"Depth capture accepted a nonexistent plane");
                image.MarkLogicalDirty();rejected=false;
                try{cache.CaptureDepthPlane(image,0,depth_gpu,renderer.RecordingList(),renderer.CompletedFence());}
                catch(const std::runtime_error&){rejected=true;}
                require(rejected,"Depth capture resurrected an invalid I");image.logical_dirty=false;
            }
            auto captured_depth=cache.CaptureDepthPlane(image,0,depth_gpu,renderer.RecordingList(),renderer.CompletedFence());
            auto captured_stencil=cache.CaptureDepthPlane(image,1,stencil_gpu,renderer.RecordingList(),renderer.CompletedFence());
            for(const auto* copy:{&captured_depth,&captured_stencil}){
                const auto native=copy->handle->GetDesc();
                require(native.Width==device->GetResourceAllocationInfo(0,1,&native).SizeInBytes,
                    "Private plane width hides its actual native allocation from residency");
            }
            auto restored_desc=desc;restored_desc.address+=0x100000;restored_desc.stencil_address+=0x100000;
            auto& restored=cache.Upload(restored_desc,{},renderer.RecordingList(),renderer.CompletedFence(),{},true,
                &captured_depth,false,false,GuestTextureCache::UploadPurpose::Binding,&captured_stencil);
            require(image.gpu_owned&&!image.buffer_alias_version&&image.HasValidLogicalImage(),
                "Private plane capture changed source I ownership or logical validity");
            struct Probe {Resource result,upload,readback,source_readback;std::shared_ptr<Descriptors> descriptors;
                ComPtr<ID3D12RootSignature> root;ComPtr<ID3D12PipelineState> pipeline;};
            auto probe=std::make_shared<Probe>();
            const auto words=u64(desc.width)*desc.height*desc.layers*samples*4;
            const auto bytes=words*4+64;
            probe->result=GuestTiler::Scratch(device,bytes);
            probe->upload=Buffer(device,bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
            probe->readback=Buffer(device,bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            const auto depth_capacity=depth_gpu.handle->GetDesc().Width,stencil_capacity=stencil_gpu.handle->GetDesc().Width;
            require(captured_depth.handle->GetDesc().Width==depth_capacity&&captured_stencil.handle->GetDesc().Width==stencil_capacity,
                "Private plane capacity differs from its complete seed envelope");
            probe->source_readback=Buffer(device,2*(depth_capacity+stencil_capacity),
                D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            void* mapped{};D3D12_RANGE empty{};Check(probe->upload.handle->Map(0,&empty,&mapped));
            memset(mapped,0xa7,size_t(bytes));probe->upload.handle->Unmap(0,nullptr);
            std::string source=std::string(samples>1?"#define MSAA 1\n":"#define MSAA 0\n")+
                (data_format==2?"#define D16 1\n":"#define D16 0\n")+R"(
#if MSAA
Texture2DMSArray<float4> depth : register(t0);
Texture2DMSArray<uint4> stencil : register(t1);
Texture2DMSArray<float4> restored_depth : register(t2);
Texture2DMSArray<uint4> restored_stencil : register(t3);
#else
Texture2DArray<float4> depth : register(t0);
Texture2DArray<uint4> stencil : register(t1);
Texture2DArray<float4> restored_depth : register(t2);
Texture2DArray<uint4> restored_stencil : register(t3);
#endif
RWByteAddressBuffer output : register(u0);
cbuffer Shape : register(b0) { uint width; uint height; uint samples; uint layers; };
[numthreads(8,8,1)] void main(uint3 p:SV_DispatchThreadID) {
    if(p.x>=width || p.y>=height || p.z>=samples*layers) return;
    uint layer=p.z/samples, sample=p.z%samples;
#if MSAA
    float z=depth.Load(int3(p.xy,layer),sample).x;
    uint s=stencil.Load(int3(p.xy,layer),sample).x;
    float rz=restored_depth.Load(int3(p.xy,layer),sample).x;
    uint rs=restored_stencil.Load(int3(p.xy,layer),sample).x;
#else
    float z=depth.Load(int4(p.xy,layer,0)).x;
    uint s=stencil.Load(int4(p.xy,layer,0)).x;
    float rz=restored_depth.Load(int4(p.xy,layer,0)).x;
    uint rs=restored_stencil.Load(int4(p.xy,layer,0)).x;
#endif
    uint at=16*((p.z*height+p.y)*width+p.x);
#if D16
    output.Store(at,uint(round(saturate(z)*65535.0)));
    output.Store(at+8,uint(round(saturate(rz)*65535.0)));
#else
    output.Store(at,asuint(z));
    output.Store(at+8,asuint(rz));
#endif
    output.Store(at+4,s);
    output.Store(at+12,rs);
})";
            auto code=CompileMultisampleShader(source.c_str(),"cs_5_1");
            D3D12_DESCRIPTOR_RANGE ranges[2]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,4,0,0,0},
                {D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,4}};
            D3D12_ROOT_PARAMETER parameters[2]{};parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            parameters[0].DescriptorTable={2,ranges};parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            parameters[1].Constants={0,0,4};
            D3D12_ROOT_SIGNATURE_DESC root{2,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
            ComPtr<ID3DBlob> signature,errors;Check(D3D12SerializeRootSignature(&root,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&errors));
            Check(device->CreateRootSignature(0,signature->GetBufferPointer(),signature->GetBufferSize(),IID_PPV_ARGS(&probe->root)));
            D3D12_COMPUTE_PIPELINE_STATE_DESC state{};state.pRootSignature=probe->root.Get();state.CS={code->GetBufferPointer(),code->GetBufferSize()};
            Check(device->CreateComputePipelineState(&state,IID_PPV_ARGS(&probe->pipeline)));
            probe->descriptors=std::make_shared<Descriptors>(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,5,true);
            for(UINT index=0;index<4;++index){const auto plane=index%2;D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
                srv.Format=plane?DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
                srv.Shader4ComponentMapping=plane?D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(1,1,1,1):D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                if(samples>1){srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;srv.Texture2DMSArray={0,desc.layers};}
                else{srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DARRAY;srv.Texture2DArray={0,1,0,desc.layers,plane,0};}
                device->CreateShaderResourceView((index<2?image:restored).texture.handle.Get(),&srv,probe->descriptors->Cpu(index));}
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.Format=DXGI_FORMAT_R32_TYPELESS;uav.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;
            uav.Buffer.NumElements=UINT(bytes/4);uav.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
            device->CreateUnorderedAccessView(probe->result.handle.Get(),nullptr,&uav,probe->descriptors->Cpu(4));
            renderer.RetainUntilFinish([probe](u64){(void)probe;});
            auto* list=renderer.RecordingList();probe->result.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
            list->CopyBufferRegion(probe->result.handle.Get(),0,probe->upload.handle.Get(),0,bytes);
            probe->result.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            image.texture.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            restored.texture.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            ID3D12DescriptorHeap* heaps[]{probe->descriptors->Heap()};list->SetDescriptorHeaps(1,heaps);
            list->SetComputeRootSignature(probe->root.Get());list->SetPipelineState(probe->pipeline.Get());
            list->SetComputeRootDescriptorTable(0,probe->descriptors->Gpu(0));
            const UINT shape[]{desc.width,desc.height,samples,desc.layers};list->SetComputeRoot32BitConstants(1,4,shape,0);
            list->Dispatch((desc.width+7)/8,(desc.height+7)/8,desc.layers*samples);
            probe->result.UavBarrier(list);probe->result.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
            list->CopyBufferRegion(probe->readback.handle.Get(),0,probe->result.handle.Get(),0,bytes);
            depth_gpu.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
            stencil_gpu.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
            list->CopyBufferRegion(probe->source_readback.handle.Get(),0,depth_gpu.handle.Get(),0,depth_capacity);
            list->CopyBufferRegion(probe->source_readback.handle.Get(),depth_capacity,stencil_gpu.handle.Get(),0,stencil_capacity);
            depth_gpu.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            stencil_gpu.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            captured_depth.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
            captured_stencil.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
            list->CopyBufferRegion(probe->source_readback.handle.Get(),depth_capacity+stencil_capacity,
                captured_depth.handle.Get(),0,depth_capacity);
            list->CopyBufferRegion(probe->source_readback.handle.Get(),2*depth_capacity+stencil_capacity,
                captured_stencil.handle.Get(),0,stencil_capacity);
            captured_depth.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            captured_stencil.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            const auto fence=renderer.Flush();cache.Submitted(fence);
            cache.Collect(renderer.CompletedFence());
            const bool held=renderer.CompletedFence()<fence&&!image.buffer_sync_resources.empty();
            captured_depth={};captured_stencil={}; // Entry and queue leases must retain the copies.
            Check(gate.fence->Signal(1));renderer.Finish();
            require(held,"Depth plane source/copy leases retired before the held GPU fence");
            std::vector<u32> expected(size_t(bytes/4),0xa7a7a7a7);
            for(UINT layer=0;layer<desc.layers;++layer)for(UINT sample=0;sample<samples;++sample)
            for(UINT y=0;y<desc.height;++y)for(UINT x=0;x<desc.width;++x){
                const auto at=4*((u64(layer)*samples+sample)*desc.height*desc.width+y*desc.width+x);
                expected[at]=depth_bits(gpu_depth?gpu_phase:cpu_phase,layer,sample,x,y);
                expected[at+1]=stencil_code(gpu_stencil?gpu_phase:cpu_phase,layer,sample,x,y);
                expected[at+2]=expected[at];expected[at+3]=expected[at+1];
            }
            D3D12_RANGE range{0,SIZE_T(bytes)};Check(probe->readback.handle->Map(0,&range,&mapped));
            const bool valid=!memcmp(mapped,expected.data(),size_t(bytes));probe->readback.handle->Unmap(0,&empty);
            if(!valid)throw std::runtime_error("Paired depth GPU result mismatch: format="+std::to_string(data_format)+
                " tile="+std::to_string(tile)+" samples="+std::to_string(samples)+" mode="+std::to_string(mode));
            auto expected_depth_capture=depth_gpu_bytes,expected_stencil_capture=stencil_gpu_bytes;
            expected_depth_capture.resize(size_t(depth_capacity),std::byte{0x79});
            expected_stencil_capture.resize(size_t(stencil_capacity),std::byte{0x79});
            for(UINT layer=0;layer<desc.layers;++layer)for(UINT sample=0;sample<samples;++sample)
            for(UINT y=0;y<desc.height;++y)for(UINT x=0;x<desc.width;++x){
                const auto d_at=GuestPixelOffset(desc,layout,0,layer,x,y,sample);
                const auto s_at=GuestPixelOffset(stencil_desc,stencil_layout,0,layer,x,y,sample);
                const u32 d_value=depth_bits(gpu_depth?gpu_phase:cpu_phase,layer,sample,x,y);
                const auto s_value=std::byte(stencil_code(gpu_stencil?gpu_phase:cpu_phase,layer,sample,x,y));
                memcpy(expected_depth_capture.data()+d_at,&d_value,layout.bytes_per_pixel);
                expected_stencil_capture[size_t(s_at)]=s_value;
            }
            range={0,SIZE_T(2*(depth_capacity+stencil_capacity))};
            Check(probe->source_readback.handle->Map(0,&range,&mapped));
            const auto* source_bytes=static_cast<const std::byte*>(mapped);bool sources_valid=true;
            for(u64 at=0;at<depth_capacity;++at)sources_valid&=source_bytes[at]==
                (at<depth_gpu_bytes.size()?depth_gpu_bytes[size_t(at)]:std::byte{0x79});
            for(u64 at=0;at<stencil_capacity;++at)sources_valid&=source_bytes[depth_capacity+at]==
                (at<stencil_gpu_bytes.size()?stencil_gpu_bytes[size_t(at)]:std::byte{0x79});
            const bool copies_valid=!memcmp(source_bytes+depth_capacity+stencil_capacity,expected_depth_capture.data(),expected_depth_capture.size())&&
                !memcmp(source_bytes+2*depth_capacity+stencil_capacity,expected_stencil_capture.data(),expected_stencil_capture.size());
            probe->source_readback.handle->Unmap(0,&empty);
            require(sources_valid&&depth_cpu==original_depth_cpu&&stencil_cpu==original_stencil_cpu,
                "Paired upload changed CPU/GPU plane inputs or source padding");
            require(copies_valid,"Private native I plane capture changed logical bits or its B1 padding");
            cache.Collect(renderer.CompletedFence());renderer.ValidateDebugMessages();++executed;
        }
    }
    require(executed>=12,"Paired single-sample depth controls did not execute");
    printf("PASS: %u paired native I captures and GPU uploads, D16/D32, samples/layers, mixed CPU/GPU inputs, exact private padding and held-fence leases; PS stencil ref=%u\n",executed,options.PSSpecifiedStencilRefSupported);
}
} // namespace NativeDX12
