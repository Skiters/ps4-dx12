// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include "video_core/renderer_d3d12/multisample_readback.h"
#include "video_core/renderer_d3d12/guest_image_readback.h"
#include "video_core/renderer_d3d12/guest_image_ownership.h"
#include <bit>
#include <cstring>
#include <string>

namespace NativeDX12 {
inline void RunPairedDepthSnapshotFixture(ID3D12Device* device) {
    const auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
    Check(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,&options,sizeof(options)));
    unsigned executed{},compact_executed{},aspect_executed{};
    for(const UINT data_format:{2u,4u})for(const UINT tile:{0u,8u})for(const UINT samples:{1u,4u}){
        if(samples>1){
            D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS quality{DXGI_FORMAT_D32_FLOAT_S8X24_UINT,samples,
                D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE,0};
            Check(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,&quality,sizeof(quality)));
            if(!quality.NumQualityLevels){
                printf("SKIP: paired MSAA depth/stencil hardware contract format=%u samples=%u\n",data_format,samples);continue;
            }
        }
        struct Backing {
            size_t bytes{};
            HANDLE section{};std::byte* guest{};std::byte* physical{};
            ~Backing(){if(physical)UnmapViewOfFile(physical);if(guest)UnmapViewOfFile(guest);if(section)CloseHandle(section);}
        } backing;
        GuestTextureDesc desc;desc.address=0x40000;desc.stencil_address=0x80000;
        desc.width=13;desc.height=11;desc.pitch=16;desc.layers=desc.view_layers=2;
        desc.depth_target=true;desc.data_format=data_format;desc.number_format=data_format==2?0:7;
        desc.samples=samples;desc.tile_mode=tile;
        auto layout=LayoutGuestTexture(desc);
        auto stencil_layout=LayoutGuestTexture(DescribeGuestStencil(desc));
        const auto depth_stride=(layout.guest_bytes+65535)&~u64(65535);
        const auto stencil_stride=(stencil_layout.guest_bytes+65535)&~u64(65535);
        const auto converted_stencil_offset=2*(depth_stride+stencil_stride);
        backing.bytes=size_t(converted_stencil_offset+stencil_stride);
        backing.section=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,
            DWORD(u64(backing.bytes)>>32),DWORD(backing.bytes),nullptr);
        require(backing.section!=nullptr,"Paired fixture physical section failed");
        backing.guest=static_cast<std::byte*>(MapViewOfFile(backing.section,FILE_MAP_ALL_ACCESS,0,0,backing.bytes));
        backing.physical=static_cast<std::byte*>(MapViewOfFile(backing.section,FILE_MAP_ALL_ACCESS,0,0,backing.bytes));
        require(backing.guest&&backing.physical,"Paired fixture physical aliases failed");
        GuestTextureCache cache(device);Rasterizer renderer(device);
        const auto base=reinterpret_cast<u64>(backing.guest);
        desc.address=base;desc.stencil_address=base+depth_stride;
        const auto stencil_desc=DescribeGuestStencil(desc);
        layout=LayoutGuestTexture(desc);stencil_layout=LayoutGuestTexture(stencil_desc);
        require(layout.guest_bytes<=depth_stride&&stencil_layout.guest_bytes<=stencil_stride,"Paired physical backing geometry changed");
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
            memcpy(backing.physical,depth_cpu.data(),depth_cpu.size());
            memcpy(backing.physical+depth_stride,stencil_cpu.data(),stencil_cpu.size());
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
                // An unvalidated stencil boundary also prevents using depth:
                // the two aspects belong to this one logical native image.
                image.stencil_unprotected_cpu_pages.emplace(desc.stencil_address&~u64(4095),
                    std::vector<std::byte>{std::byte{0x17}});
                for(const auto plane:{0u,1u}){
                    rejected=false;
                    try{cache.CaptureDepthPlane(image,plane,plane?stencil_gpu:depth_gpu,
                        renderer.RecordingList(),renderer.CompletedFence());}
                    catch(const std::runtime_error&){rejected=true;}
                    require(rejected,"Native plane capture ignored an unvalidated stencil boundary");
                }
                image.stencil_unprotected_cpu_pages.clear();
            }
            auto captured_depth=cache.CaptureDepthPlane(image,0,depth_gpu,renderer.RecordingList(),renderer.CompletedFence());
            auto captured_stencil=cache.CaptureDepthPlane(image,1,stencil_gpu,renderer.RecordingList(),renderer.CompletedFence());
            for(const auto* copy:{&captured_depth,&captured_stencil}){
                const auto native=copy->handle->GetDesc();
                require(native.Width==device->GetResourceAllocationInfo(0,1,&native).SizeInBytes,
                    "Private plane width hides its actual native allocation from residency");
            }
            auto restored_desc=desc;restored_desc.address+=depth_stride+stencil_stride;
            restored_desc.stencil_address+=depth_stride+stencil_stride;
            auto& restored=cache.Upload(restored_desc,{},renderer.RecordingList(),renderer.CompletedFence(),{},true,
                &captured_depth,false,false,GuestTextureCache::UploadPurpose::Binding,&captured_stencil);
            std::unique_ptr<GuestLinearImageReadback> compact;
            if(samples==1)compact=std::make_unique<GuestLinearImageReadback>(device,image,renderer.RecordingList());
            else if(mode==0){
                bool rejected=false;
                try{GuestLinearImageReadback invalid(device,image,renderer.RecordingList());}
                catch(const std::invalid_argument&){rejected=true;}
                require(rejected,"Compact depth download accepted an unimplemented MSAA download domain");
            }
            require(image.gpu_owned&&!image.buffer_alias_version&&image.HasValidLogicalImage(),
                "Private plane capture changed source I ownership or logical validity");
            auto converted_desc=desc;converted_desc.stencil_address=base+converted_stencil_offset;
            const auto converted_cpu_stencil=physical(true,cpu_phase+13);
            const auto converted_gpu_stencil=physical(true,gpu_phase+11);
            memcpy(backing.physical+converted_stencil_offset,converted_cpu_stencil.data(),converted_cpu_stencil.size());
            auto converted_stencil_seed=gpu_copy(converted_gpu_stencil);
            const auto aspect_stencil_bytes=physical(true,gpu_phase+21);
            Resource aspect_stencil_seed;
            if(mode==0&&samples==1)aspect_stencil_seed=gpu_copy(aspect_stencil_bytes);
            unsigned cpu_faults{},conversions{};GuestTextureCache::Entry* live_owner{};
            std::function<void(GuestTextureCache::Entry&)> retire;
            PageWatch watch([&](u64,bool writing){
                require(writing&&live_owner,"Unexpected paired independent CPU fault");++cpu_faults;
                auto* old=std::exchange(live_owner,nullptr);old->MarkLogicalDirty();retire(*old);
            });
            renderer.SetCpuPageRelease([&](u64 address,u64 bytes){return watch.UntrackReadOnly(address,bytes,true);});
            retire=[&](auto& old){ReleaseGuestImageOwnership(cache,old,[&](u64 address,u64 bytes,const auto& retained){
                renderer.ReleaseUnusedReadOnlyWatches(address,bytes,retained);});};
            cache.SetIndependentImageRetention([](const auto& owner,const auto& physical){
                return owner.writable&&!owner.depth_target&&!owner.stencil_address&&physical.block_extent==1&&
                    !physical.expanded_bytes_per_pixel&&(owner.samples==1||
                    GuestTextureCache::CanTransferIndependentMultisample(owner,physical));
            });
            const auto d_pages=*GuestProtectionPages(desc.address,layout.guest_bytes);
            const auto s_pages=*GuestProtectionPages(desc.stencil_address,stencil_layout.guest_bytes);
            bool stencil_backing=true;
            const auto has_backing=[&](u64 address,u64 bytes){return address>=base&&address-base<=backing.bytes&&
                bytes<=backing.bytes-(address-base)&&(stencil_backing||address!=desc.stencil_address);};
            const auto acquire=[&](auto& target){return AcquireGuestImageOwnership(cache,target,watch,has_backing,
                [&](u64 address,u64 bytes){
                    std::vector<std::pair<u64,u64>> strong;
                    if(mode==2&&ShareGuestProtectionPages(address,bytes,desc.address,layout.guest_bytes))
                        strong.emplace_back(d_pages.address,d_pages.size);
                    return strong;
                });};
            const auto permissions=[&](u64 address){MEMORY_BASIC_INFORMATION info{};
                require(VirtualQuery(reinterpret_cast<void*>(address),&info,sizeof(info))!=0,"Paired permission query failed");return info.Protect;};
            if(mode==2){
                require(watch.TrackOwned(d_pages.address,d_pages.size)&&watch.TrackOwned(s_pages.address,s_pages.size),
                    "Paired preceding exclusive ownership failed");
            }else image.SetGpuOwned(false);
            if(mode==0){
                const auto saved=image.stencil_layout.mips[0].offset;image.stencil_layout.mips[0].offset+=1;
                require(!DescribeGuestImageOwnership(image),"Paired owner ignored mismatched S layout");
                image.stencil_layout.mips[0].offset=saved;
                stencil_backing=false;require(!acquire(image)&&!image.gpu_owned&&!image.buffer_alias_version&&
                    permissions(desc.address)==PAGE_READWRITE,"Missing S backing partially acquired D");stencil_backing=true;
            }
            require(acquire(image),"Paired independent production ownership failed");
            const auto owner=DescribeGuestImageOwnership(image);
            require(owner&&owner->stencil&&CanRetainGuestImageOwnership(cache,*owner)&&
                permissions(desc.address)==DWORD(mode==2?PAGE_NOACCESS:PAGE_READONLY)&&permissions(desc.stencil_address)==PAGE_READONLY,
                "Paired physical geometry or strong B protection changed");
            const auto clone=[&](Resource& from){
                const auto capacity=from.handle->GetDesc().Width;
                auto copy=Buffer(device,capacity,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
                from.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
                renderer.RecordingList()->CopyBufferRegion(copy.handle.Get(),0,from.handle.Get(),0,capacity);
                from.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                copy.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                renderer.RetainUntilFinish([copy](u64){(void)copy;});return copy;
            };
            cache.SetIndependentVersionTransfer([&](auto& old,u64 address,u64 bytes){
                require(address==desc.address&&bytes==layout.guest_bytes,"Unexpected paired legacy transfer range");
                auto seed=clone(depth_gpu);auto result=cache.RetileBufferVersion(old,seed,renderer.RecordingList(),renderer.CompletedFence());
                result.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);return result;
            },acquire,retire);
            cache.SetIndependentImageTransfer([&](auto& old,const auto& target,const auto& physical){
                ++conversions;return cache.CaptureIndependentImageVersion(old,target,physical,renderer.RecordingList(),renderer.CompletedFence(),
                    [&](u64 address,u64 bytes){
                        if(address==desc.address&&bytes==layout.guest_bytes)return clone(depth_gpu);
                        if(address==desc.stencil_address&&bytes==stencil_layout.guest_bytes&&aspect_stencil_seed.handle)
                            return clone(aspect_stencil_seed);
                        require(address==converted_desc.stencil_address&&bytes==stencil_layout.guest_bytes,"Unexpected paired S source range");
                        return clone(converted_stencil_seed);
                    });
            });
            const GuestTextureCache::BufferResidency chain{{desc.address,layout.guest_bytes}};
            image.pending_buffer_residency=chain;GuestTextureCache::BufferResidency published;
            cache.SetBufferResidencyPublisher([&](auto ranges){published.insert(published.end(),ranges.begin(),ranges.end());});
            auto& converted=cache.Upload(converted_desc,depth_cpu,renderer.RecordingList(),renderer.CompletedFence(),
                converted_cpu_stencil,false,nullptr,false,false,GuestTextureCache::UploadPurpose::Preparation);
            live_owner=&converted;
            require(conversions==1&& &converted!=&image&&converted.gpu_owned&&converted.buffer_alias_version&&
                converted.HasValidLogicalImage()&&!image.gpu_owned&&!image.buffer_alias_version&&
                converted.pending_buffer_residency==chain&&published.empty(),"Paired conversion lost authority, validity or deferred provenance");
            require(permissions(desc.address)==DWORD(mode==2?PAGE_NOACCESS:PAGE_READONLY)&&
                permissions(desc.stencil_address)==PAGE_READWRITE&&permissions(converted_desc.stencil_address)==PAGE_READONLY,
                "Paired source retirement lost destination/strong guards");
            cache.Upload(converted_desc,depth_cpu,renderer.RecordingList(),renderer.CompletedFence(),converted_cpu_stencil);
            require(conversions==1&&published==chain&&converted.pending_buffer_residency.empty(),
                "Paired preparation published early or actual binding replayed/lost provenance");
            struct Probe {Resource result,upload,readback,source_readback;std::shared_ptr<Descriptors> descriptors;
                ComPtr<ID3D12RootSignature> root;ComPtr<ID3D12PipelineState> pipeline;};
            auto probe=std::make_shared<Probe>();
            const auto words=u64(desc.width)*desc.height*desc.layers*samples*6;
            const auto bytes=words*4+64;
            probe->result=GuestTiler::Scratch(device,bytes);
            probe->upload=Buffer(device,bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
            probe->readback=Buffer(device,bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            const auto depth_capacity=depth_gpu.handle->GetDesc().Width,stencil_capacity=stencil_gpu.handle->GetDesc().Width;
            require(captured_depth.handle->GetDesc().Width==depth_capacity&&captured_stencil.handle->GetDesc().Width==stencil_capacity,
                "Private plane capacity differs from its complete seed envelope");
            probe->source_readback=Buffer(device,2*depth_capacity+3*stencil_capacity,
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
Texture2DMSArray<float4> converted_depth : register(t4);
Texture2DMSArray<uint4> converted_stencil : register(t5);
#else
Texture2DArray<float4> depth : register(t0);
Texture2DArray<uint4> stencil : register(t1);
Texture2DArray<float4> restored_depth : register(t2);
Texture2DArray<uint4> restored_stencil : register(t3);
Texture2DArray<float4> converted_depth : register(t4);
Texture2DArray<uint4> converted_stencil : register(t5);
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
    float cz=converted_depth.Load(int3(p.xy,layer),sample).x;
    uint cs=converted_stencil.Load(int3(p.xy,layer),sample).x;
#else
    float z=depth.Load(int4(p.xy,layer,0)).x;
    uint s=stencil.Load(int4(p.xy,layer,0)).x;
    float rz=restored_depth.Load(int4(p.xy,layer,0)).x;
    uint rs=restored_stencil.Load(int4(p.xy,layer,0)).x;
    float cz=converted_depth.Load(int4(p.xy,layer,0)).x;
    uint cs=converted_stencil.Load(int4(p.xy,layer,0)).x;
#endif
    uint at=24*((p.z*height+p.y)*width+p.x);
#if D16
    output.Store(at,uint(round(saturate(z)*65535.0)));
    output.Store(at+8,uint(round(saturate(rz)*65535.0)));
    output.Store(at+16,uint(round(saturate(cz)*65535.0)));
#else
    output.Store(at,asuint(z));
    output.Store(at+8,asuint(rz));
    output.Store(at+16,asuint(cz));
#endif
    output.Store(at+4,s);
    output.Store(at+12,rs);
    output.Store(at+20,cs);
})";
            auto code=CompileMultisampleShader(source.c_str(),"cs_5_1");
            D3D12_DESCRIPTOR_RANGE ranges[2]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,6,0,0,0},
                {D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,6}};
            D3D12_ROOT_PARAMETER parameters[2]{};parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            parameters[0].DescriptorTable={2,ranges};parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            parameters[1].Constants={0,0,4};
            D3D12_ROOT_SIGNATURE_DESC root{2,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
            ComPtr<ID3DBlob> signature,errors;Check(D3D12SerializeRootSignature(&root,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&errors));
            Check(device->CreateRootSignature(0,signature->GetBufferPointer(),signature->GetBufferSize(),IID_PPV_ARGS(&probe->root)));
            D3D12_COMPUTE_PIPELINE_STATE_DESC state{};state.pRootSignature=probe->root.Get();state.CS={code->GetBufferPointer(),code->GetBufferSize()};
            Check(device->CreateComputePipelineState(&state,IID_PPV_ARGS(&probe->pipeline)));
            probe->descriptors=std::make_shared<Descriptors>(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,7,true);
            for(UINT index=0;index<6;++index){const auto plane=index%2;D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
                srv.Format=plane?DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
                srv.Shader4ComponentMapping=plane?D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(1,1,1,1):D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                if(samples>1){srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;srv.Texture2DMSArray={0,desc.layers};}
                else{srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DARRAY;srv.Texture2DArray={0,1,0,desc.layers,plane,0};}
                device->CreateShaderResourceView((index<2?image:index<4?restored:converted).texture.handle.Get(),&srv,probe->descriptors->Cpu(index));}
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.Format=DXGI_FORMAT_R32_TYPELESS;uav.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;
            uav.Buffer.NumElements=UINT(bytes/4);uav.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
            device->CreateUnorderedAccessView(probe->result.handle.Get(),nullptr,&uav,probe->descriptors->Cpu(6));
            renderer.RetainUntilFinish([probe](u64){(void)probe;});
            auto* list=renderer.RecordingList();probe->result.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
            list->CopyBufferRegion(probe->result.handle.Get(),0,probe->upload.handle.Get(),0,bytes);
            probe->result.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            image.texture.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            restored.texture.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            converted.texture.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
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
            converted_stencil_seed.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
            list->CopyBufferRegion(probe->source_readback.handle.Get(),2*(depth_capacity+stencil_capacity),
                converted_stencil_seed.handle.Get(),0,stencil_capacity);
            converted_stencil_seed.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            captured_depth.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            captured_stencil.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            const auto fence=renderer.Flush();cache.Submitted(fence);
            if(compact){
                compact->Submitted(fence);
                std::vector<std::byte> pending(size_t(compact->Bytes()),std::byte{0xb6});
                bool rejected=false;
                try{compact->Resolve(renderer.CompletedFence(),pending);}
                catch(const std::logic_error&){rejected=true;}
                require(rejected&&std::all_of(pending.begin(),pending.end(),[](auto b){return b==std::byte{0xb6};}),
                    "Compact paired-depth readback ignored its held GPU fence");
            }
            cache.Collect(renderer.CompletedFence());
            const bool held=renderer.CompletedFence()<fence&&!image.buffer_sync_resources.empty();
            captured_depth={};captured_stencil={}; // Entry and queue leases must retain the copies.
            Check(gate.fence->Signal(1));renderer.Finish();
            require(held,"Depth plane source/copy leases retired before the held GPU fence");
            std::vector<u32> expected(size_t(bytes/4),0xa7a7a7a7);
            for(UINT layer=0;layer<desc.layers;++layer)for(UINT sample=0;sample<samples;++sample)
            for(UINT y=0;y<desc.height;++y)for(UINT x=0;x<desc.width;++x){
                const auto at=6*((u64(layer)*samples+sample)*desc.height*desc.width+y*desc.width+x);
                expected[at]=depth_bits(gpu_depth?gpu_phase:cpu_phase,layer,sample,x,y);
                expected[at+1]=stencil_code(gpu_stencil?gpu_phase:cpu_phase,layer,sample,x,y);
                expected[at+2]=expected[at];expected[at+3]=expected[at+1];
                expected[at+4]=expected[at];
                // Pinned CopyImage excludes S, even with matching D/S aspects.
                // The new native S keeps its explicit target GPU seed.
                expected[at+5]=stencil_code(gpu_phase+11,layer,sample,x,y);
            }
            D3D12_RANGE range{0,SIZE_T(bytes)};Check(probe->readback.handle->Map(0,&range,&mapped));
            const bool valid=!memcmp(mapped,expected.data(),size_t(bytes));probe->readback.handle->Unmap(0,&empty);
            if(!valid)throw std::runtime_error("Paired depth GPU result mismatch: format="+std::to_string(data_format)+
                " tile="+std::to_string(tile)+" samples="+std::to_string(samples)+" mode="+std::to_string(mode));
            if(compact){
                std::vector<std::byte> actual(size_t(compact->Bytes()),std::byte{0xb6}),wanted=actual;
                require(actual.size()==u64(desc.pitch)*desc.height*desc.layers*layout.bytes_per_pixel,
                    "Compact paired depth changed the original download extent");
                bool rejected=false;
                try{compact->Resolve(renderer.CompletedFence(),std::span(actual).first(actual.size()-1));}
                catch(const std::invalid_argument&){rejected=true;}
                require(rejected&&actual==wanted,"Short compact paired-depth span was accepted or changed");
                for(UINT layer=0;layer<desc.layers;++layer)for(UINT y=0;y<desc.height;++y)
                for(UINT x=0;x<desc.width;++x){
                    const auto value=depth_bits(gpu_depth?gpu_phase:cpu_phase,layer,0,x,y);
                    const auto at=((u64(layer)*desc.height+y)*desc.pitch+x)*layout.bytes_per_pixel;
                    memcpy(wanted.data()+at,&value,layout.bytes_per_pixel);
                }
                compact->Resolve(renderer.CompletedFence(),actual);
                require(actual==wanted,"Compact paired depth changed bits, pitch padding or native D16 stride");
                ++compact_executed;
            }
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
            range={0,SIZE_T(2*depth_capacity+3*stencil_capacity)};
            Check(probe->source_readback.handle->Map(0,&range,&mapped));
            const auto* source_bytes=static_cast<const std::byte*>(mapped);bool sources_valid=true;
            for(u64 at=0;at<depth_capacity;++at)sources_valid&=source_bytes[at]==
                (at<depth_gpu_bytes.size()?depth_gpu_bytes[size_t(at)]:std::byte{0x79});
            for(u64 at=0;at<stencil_capacity;++at)sources_valid&=source_bytes[depth_capacity+at]==
                (at<stencil_gpu_bytes.size()?stencil_gpu_bytes[size_t(at)]:std::byte{0x79});
            for(u64 at=0;at<stencil_capacity;++at)sources_valid&=source_bytes[2*(depth_capacity+stencil_capacity)+at]==
                (at<converted_gpu_stencil.size()?converted_gpu_stencil[size_t(at)]:std::byte{0x79});
            const bool copies_valid=!memcmp(source_bytes+depth_capacity+stencil_capacity,expected_depth_capture.data(),expected_depth_capture.size())&&
                !memcmp(source_bytes+2*depth_capacity+stencil_capacity,expected_stencil_capture.data(),expected_stencil_capture.size());
            probe->source_readback.handle->Unmap(0,&empty);
            require(sources_valid&&depth_cpu==original_depth_cpu&&stencil_cpu==original_stencil_cpu,
                "Paired upload changed CPU/GPU plane inputs or source padding");
            require(copies_valid,"Private native I plane capture changed logical bits or its B1 padding");
            require(!memcmp(backing.physical,original_depth_cpu.data(),original_depth_cpu.size())&&
                !memcmp(backing.physical+depth_stride,original_stencil_cpu.data(),original_stencil_cpu.size())&&
                !memcmp(backing.physical+converted_stencil_offset,converted_cpu_stencil.data(),converted_cpu_stencil.size()),
                "Paired independent conversion published private I into physical B");
            u64 cpu_stencil_offset=converted_stencil_offset;
            const std::vector<std::byte>* visible_stencil=&converted_cpu_stencil;
            if(mode==0&&samples==1){
                // Pinned CopyImageWithBuffer excludes S at both endpoints.
                // Check the intermediate native color words independently,
                // then require new S initialization from its explicit GPU B1
                // seed, distinct from the old I and physical CPU B0.
                auto color_desc=desc;color_desc.depth_target=false;color_desc.stencil_address=0;color_desc.render_target=true;
                auto& color=cache.Upload(color_desc,depth_cpu,renderer.RecordingList(),renderer.CompletedFence());
                live_owner=&color;
                require(color.gpu_owned&&color.buffer_alias_version&&color.HasValidLogicalImage()&&
                    !converted.gpu_owned&&conversions==2,"Depth/color aspect transfer lost its independent I");
                GuestLinearImageReadback color_native(device,color,renderer.RecordingList());
                auto& returned=cache.Upload(desc,depth_cpu,renderer.RecordingList(),renderer.CompletedFence(),stencil_cpu);
                live_owner=&returned;
                require(returned.gpu_owned&&returned.buffer_alias_version&&returned.HasValidLogicalImage()&&
                    !color.gpu_owned&&conversions==3,"Color/depth aspect transfer lost its paired owner");
                GuestImageReadback native_return(device,returned,renderer.RecordingList());
                const auto copied=renderer.Flush();cache.Submitted(copied);color_native.Submitted(copied);native_return.Submitted(copied);
                renderer.Finish();
                std::vector<std::byte> actual_color(size_t(color_native.Bytes()),std::byte{0xb6}),wanted_color=actual_color;
                for(UINT layer=0;layer<desc.layers;++layer)for(UINT y=0;y<desc.height;++y)for(UINT x=0;x<desc.width;++x){
                    const auto value=depth_bits(gpu_phase,layer,0,x,y);
                    const auto at=((u64(layer)*desc.height+y)*desc.pitch+x)*layout.bytes_per_pixel;
                    memcpy(wanted_color.data()+at,&value,layout.bytes_per_pixel);
                }
                color_native.Resolve(renderer.CompletedFence(),actual_color);
                require(actual_color==wanted_color,"Intermediate native color aspect lost depth bits or compact padding");
                auto actual_d=depth_cpu,actual_s=stencil_cpu;
                native_return.Resolve(renderer.CompletedFence(),actual_d,actual_s);
                require(actual_d==physical(false,gpu_phase)&&actual_s==aspect_stencil_bytes,
                    "Aspect-changing depth transfer copied stale S or lost native depth words");
                require(!memcmp(backing.physical,original_depth_cpu.data(),original_depth_cpu.size())&&
                    !memcmp(backing.physical+depth_stride,original_stencil_cpu.data(),original_stencil_cpu.size())&&
                    !memcmp(backing.physical+converted_stencil_offset,converted_cpu_stencil.data(),converted_cpu_stencil.size()),
                    "Depth/color aspect transfer published I into physical B");
                cpu_stencil_offset=depth_stride;visible_stencil=&stencil_cpu;++aspect_executed;
            }
            const auto first_stencil=GuestPixelOffset(stencil_desc,stencil_layout,0,0,0,0);
            require(*reinterpret_cast<volatile unsigned char*>(backing.guest+cpu_stencil_offset+first_stencil)==
                std::to_integer<unsigned char>((*visible_stencil)[size_t(first_stencil)])&&cpu_faults==0,
                "Independent paired CPU read exposed I instead of physical B0");
            auto* ending_owner=live_owner;
            *reinterpret_cast<volatile unsigned char*>(backing.guest+cpu_stencil_offset+first_stencil)=0x96;
            require(cpu_faults==1&&!live_owner&&!ending_owner->gpu_owned&&!ending_owner->buffer_alias_version&&ending_owner->logical_dirty&&
                permissions(desc.address)==DWORD(mode==2?PAGE_NOACCESS:PAGE_READWRITE)&&
                permissions(base+cpu_stencil_offset)==PAGE_READWRITE,
                "Paired CPU store lost ownership invalidation or a stronger B guard");
            if(mode==2)watch.Untrack(d_pages.address,d_pages.size);
            cache.SetIndependentVersionTransfer({},{},{});cache.SetIndependentImageTransfer({});
            cache.SetBufferResidencyPublisher({});renderer.SetCpuPageRelease({});
            cache.Collect(renderer.CompletedFence());renderer.ValidateDebugMessages();++executed;
        }
    }
    require(executed>=12,"Paired single-sample depth controls did not execute");
    require(compact_executed==12,"Compact paired depth controls did not execute");
    require(aspect_executed==4,"Depth/color aspect controls did not execute");
    printf("PASS: %u depth/color/depth aspect transitions with independent native color bits and fresh S initialization\n",aspect_executed);
    printf("PASS: %u compact D16/S8 and D32/S8 depth-only downloads, native footprint strides, padding and fence/extent negatives\n",compact_executed);
    printf("PASS: %u paired independent native conversions with real D/S page guards, strong B, deferred provenance and CPU-store retirement\n",executed);
    printf("PASS: %u paired native I captures and GPU uploads, D16/D32, samples/layers, mixed CPU/GPU inputs, exact private padding and held-fence leases; PS stencil ref=%u\n",executed,options.PSSpecifiedStencilRefSupported);
}
} // namespace NativeDX12
