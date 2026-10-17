// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_image_ownership.h"
#include "video_core/renderer_d3d12/guest_image_readback.h"
#include "video_core/renderer_d3d12/multisample_readback.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include <bit>
#include <cstring>
#include <string>

namespace NativeDX12 {
inline void RunIndependentDepthGrowthFixture(ID3D12Device* device){
    const auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    unsigned executed{};
    for(const UINT format:{2u,4u})for(const UINT tile:{0u,8u})for(UINT variant=0;variant<4;++variant){
        const UINT samples=variant==3?4:1;
        if(samples>1){D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS q{DXGI_FORMAT_D32_FLOAT_S8X24_UINT,samples,
            D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE,0};
            Check(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,&q,sizeof(q)));
            if(!q.NumQualityLevels){puts("SKIP: independent depth growth MSAA hardware unavailable");continue;}}
        GuestTextureDesc source;source.address=0x40000;source.stencil_address=0x80000;
        source.width=13;source.height=11;source.pitch=16;source.data_format=format;source.number_format=format==2?0:7;
        source.depth_target=true;source.tile_mode=tile;source.samples=samples;
        source.levels=(variant==1||variant==2)?2:1;
        source.layers=source.view_layers=(variant==0||variant>=2)?2:1;
        auto target=source;target.levels=variant==3?1:variant==1?2:3;target.layers=target.view_layers=variant==0?2:3;
        printf("CASE: depth growth fmt=%u tile=%u samples=%u mip/layer=%u/%u -> %u/%u\n",
            format,tile,samples,source.levels,source.layers,target.levels,target.layers);fflush(stdout);
        const auto old_layout=LayoutGuestTexture(source),new_layout=LayoutGuestTexture(target);
        const auto old_s=LayoutGuestTexture(DescribeGuestStencil(source)),new_s=LayoutGuestTexture(DescribeGuestStencil(target));
        const auto d_capacity=(new_layout.guest_bytes+65535)&~u64(65535),s_capacity=(new_s.guest_bytes+65535)&~u64(65535);
        struct Backing {HANDLE section{};std::byte* guest{};std::byte* physical{};
            ~Backing(){if(physical)UnmapViewOfFile(physical);if(guest)UnmapViewOfFile(guest);if(section)CloseHandle(section);}} backing;
        const auto backing_bytes=d_capacity+s_capacity;
        backing.section=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,DWORD(backing_bytes>>32),DWORD(backing_bytes),nullptr);
        require(backing.section!=nullptr,"Growth physical section failed");
        backing.guest=static_cast<std::byte*>(MapViewOfFile(backing.section,FILE_MAP_ALL_ACCESS,0,0,SIZE_T(backing_bytes)));
        backing.physical=static_cast<std::byte*>(MapViewOfFile(backing.section,FILE_MAP_ALL_ACCESS,0,0,SIZE_T(backing_bytes)));
        require(backing.guest&&backing.physical,"Growth physical aliases failed");
        const auto base=reinterpret_cast<u64>(backing.guest);
        source.address=target.address=base;source.stencil_address=target.stencil_address=base+d_capacity;
        memset(backing.physical,0x17,size_t(backing_bytes));
        const auto bits=[&](UINT phase,UINT mip,UINT layer,UINT sample,UINT x,UINT y){
            return format==2?1u+(phase*997u+mip*53u+layer*137u+sample*311u+y*7u+x*11u)%65530u:
                std::bit_cast<u32>(float(phase*2048u+mip*64u+layer*256u+sample*128u+y*13u+x+1u)/65536.f);};
        const auto s_bits=[](UINT phase,UINT mip,UINT layer,UINT sample,UINT x,UINT y){
            return (phase*79u+mip*11u+layer*17u+sample*31u+y*7u+x*3u)%256u;};
        const auto guest=[&](const auto& desc,const auto& layout,bool stencil,UINT phase,std::byte padding){
            const auto interpreted=stencil?DescribeGuestStencil(desc):desc;
            std::vector<std::byte> out(size_t(layout.guest_bytes),padding);
            for(UINT m=0;m<desc.levels;++m)for(UINT l=0;l<desc.layers;++l)for(UINT s=0;s<desc.samples;++s)
            for(UINT y=0;y<std::max(desc.height>>m,1u);++y)for(UINT x=0;x<std::max(desc.width>>m,1u);++x){
                const auto value=stencil?s_bits(phase,m,l,s,x,y):bits(phase,m,l,s,x,y);
                memcpy(out.data()+GuestPixelOffset(interpreted,layout,m,l,x,y,s),&value,layout.bytes_per_pixel);
            }return out;
        };
        const auto src_d=guest(source,old_layout,false,1,std::byte{0xa3}),src_s=guest(source,old_s,true,2,std::byte{0xb4});
        const auto dst_d=guest(target,new_layout,false,7,std::byte{0x83}),dst_s=guest(target,new_s,true,9,std::byte{0x91});
        auto expected_d=dst_d;
        for(UINT m=0;m<source.levels;++m)for(UINT l=0;l<source.layers;++l)for(UINT s=0;s<samples;++s)
        for(UINT y=0;y<std::max(source.height>>m,1u);++y)for(UINT x=0;x<std::max(source.width>>m,1u);++x){
            const auto value=bits(1,m,l,s,x,y);
            memcpy(expected_d.data()+GuestPixelOffset(target,new_layout,m,l,x,y,s),&value,new_layout.bytes_per_pixel);
        }
        GuestTextureCache cache(device);Rasterizer renderer(device);
        const auto gpu=[&](const std::vector<std::byte>& data){
            const auto bytes=(data.size()+65535)&~size_t(65535);
            auto upload=Buffer(device,bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
            auto output=Buffer(device,bytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
            void* mapped{};D3D12_RANGE empty{};Check(upload.handle->Map(0,&empty,&mapped));
            memset(mapped,0x6b,bytes);memcpy(mapped,data.data(),data.size());upload.handle->Unmap(0,nullptr);
            renderer.RecordingList()->CopyBufferRegion(output.handle.Get(),0,upload.handle.Get(),0,bytes);
            output.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            renderer.RetainUntilFinish([upload,output](u64){(void)upload;(void)output;});return output;
        };
        auto old_d=gpu(src_d),old_stencil=gpu(src_s),seed_d=gpu(dst_d),seed_s=gpu(dst_s);
        auto& image=cache.Upload(source,{},renderer.RecordingList(),renderer.CompletedFence(),{},true,&old_d,
            false,false,GuestTextureCache::UploadPurpose::Preparation,&old_stencil);
        image.NoteLogicalImageWrite();
        GuestTextureCache::Entry* live{};unsigned faults{};std::function<void(GuestTextureCache::Entry&)> retire;
        PageWatch watch([&](u64,bool writing){require(writing&&live,"Unexpected growth CPU fault");++faults;
            auto* old=std::exchange(live,nullptr);old->MarkLogicalDirty();retire(*old);});
        renderer.SetCpuPageRelease([&](u64 a,u64 n){return watch.UntrackReadOnly(a,n,true);});
        retire=[&](auto& old){ReleaseGuestImageOwnership(cache,old,[&](u64 a,u64 n,const auto& keep){
            renderer.ReleaseUnusedReadOnlyWatches(a,n,keep);});};
        cache.SetIndependentImageRetention([](const auto& d,const auto& l){return d.writable&&!d.depth_target&&
            !d.stencil_address&&!l.expanded_bytes_per_pixel&&(d.samples==1||GuestTextureCache::CanTransferIndependentMultisample(d,l));});
        const auto acquire=[&](auto& old){return AcquireGuestImageOwnership(cache,old,watch,
            [&](u64 a,u64 n){return a>=base&&a-base<=backing_bytes&&n<=backing_bytes-(a-base);},
            [](u64,u64){return std::vector<std::pair<u64,u64>>{};});};
        require(acquire(image),"Growth source independent owner failed");
        const GuestTextureCache::BufferResidency ancestor{{source.address,old_layout.guest_bytes}},extra{{target.address,new_layout.guest_bytes}};
        auto wanted_receipts=ancestor;wanted_receipts.insert(wanted_receipts.end(),extra.begin(),extra.end());
        image.pending_buffer_residency=ancestor;GuestTextureCache::BufferResidency published;unsigned publications{};
        cache.SetBufferResidencyPublisher([&](auto ranges){++publications;published.assign(ranges.begin(),ranges.end());});
        Resource captured;unsigned reads{};
        const auto clone=[&](Resource& from){auto out=GuestTiler::Scratch(device,from.handle->GetDesc().Width);
            from.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);out.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
            renderer.RecordingList()->CopyBufferRegion(out.handle.Get(),0,from.handle.Get(),0,from.handle->GetDesc().Width);
            from.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            out.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            renderer.RetainUntilFinish([out](u64){(void)out;});return out;};
        const auto capture=[&](u64 a,u64 n){++reads;
            if(a==target.address&&n==new_layout.guest_bytes)return clone(seed_d);
            require(a==target.stencil_address&&n==new_s.guest_bytes,"Growth seed range mismatch");return clone(seed_s);};
        auto invalid=target;invalid.width+=1;
        bool rejected=false;try{cache.CaptureIndependentImageVersion(image,invalid,LayoutGuestTexture(invalid),
            renderer.RecordingList(),renderer.CompletedFence(),capture);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected&&reads==0&&image.gpu_owned,"Unmapped growth extent mutated/captured source");
        cache.SetIndependentVersionTransfer({},acquire,retire);
        cache.SetIndependentImageTransfer([&](auto& old,const auto& d,const auto& layout){
            auto result=cache.CaptureIndependentImageVersion(old,d,layout,renderer.RecordingList(),renderer.CompletedFence(),capture);
            captured=result.depth;result.seed_residency=extra;return result;});
        struct Gate {ComPtr<ID3D12Fence> fence;~Gate(){if(fence)fence->Signal(1);}} gate;
        Check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&gate.fence)));renderer.WaitExternalFence(gate.fence.Get(),1);
        require(cache.FindIndependentUploadSource(target,new_layout)==&image,"Growth lookup lost native I before capture");
        std::vector<std::byte> cpu_d(size_t(new_layout.guest_bytes),std::byte{0x17}),cpu_s(size_t(new_s.guest_bytes),std::byte{0x17});
        auto& grown=cache.Upload(target,cpu_d,renderer.RecordingList(),renderer.CompletedFence(),cpu_s,false,nullptr,
            false,false,GuestTextureCache::UploadPurpose::Preparation);live=&grown;
        require(&grown!=&image&&grown.HasValidLogicalImage()&&grown.gpu_owned&&grown.buffer_alias_version&&!image.gpu_owned&&
            reads==2&&!publications&&grown.pending_buffer_residency==wanted_receipts,"Growth lost owner/validity/deferred seed provenance");
        cache.Upload(target,cpu_d,renderer.RecordingList(),renderer.CompletedFence(),cpu_s);
        require(publications==1&&published==wanted_receipts&&grown.pending_buffer_residency.empty(),"Growth bind did not consume both receipts once");
        auto native=std::make_shared<GuestImageReadback>(device,grown,renderer.RecordingList());
        renderer.RetainUntilFinish([native](u64){(void)native;});
        struct Segment {Resource* resource;const std::vector<std::byte>* expected;u64 offset;};
        std::array<Segment,5> segments{{{&old_d,&src_d,0},{&old_stencil,&src_s,0},{&seed_d,&dst_d,0},{&seed_s,&dst_s,0},{&captured,&expected_d,0}}};
        u64 readback_bytes{};for(auto& part:segments){part.offset=readback_bytes;readback_bytes+=part.resource->handle->GetDesc().Width;}
        auto backing_readback=Buffer(device,readback_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        renderer.RetainUntilFinish([backing_readback](u64){(void)backing_readback;});
        for(auto& part:segments){part.resource->Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            renderer.RecordingList()->CopyBufferRegion(backing_readback.handle.Get(),part.offset,part.resource->handle.Get(),0,part.resource->handle->GetDesc().Width);
            part.resource->Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);}
        // Independent native typed mip/layer probe; no guest tiler/readback is
        // used to derive the expected native pixel values.
        const std::string shader=std::string(samples>1?"#define MSAA 1\n":"#define MSAA 0\n")+(format==2?"#define D16 1\n":"#define D16 0\n")+R"(
#if MSAA
Texture2DMSArray<float4> depth : register(t0);Texture2DMSArray<uint4> stencil : register(t1);
#else
Texture2DArray<float4> depth : register(t0);Texture2DArray<uint4> stencil : register(t1);
#endif
RWByteAddressBuffer output:register(u0);cbuffer Shape:register(b0){uint width,height,samples,layers,base;};
[numthreads(8,8,1)]void main(uint3 p:SV_DispatchThreadID){if(p.x>=width||p.y>=height||p.z>=samples*layers)return;
uint layer=p.z/samples,sample=p.z%samples;
#if MSAA
float z=depth.Load(int3(p.xy,layer),sample).x;uint s=stencil.Load(int3(p.xy,layer),sample).x;
#else
float z=depth.Load(int4(p.xy,layer,0)).x;uint s=stencil.Load(int4(p.xy,layer,0)).x;
#endif
uint at=4*(base+2*((p.z*height+p.y)*width+p.x));
#if D16
output.Store(at,uint(round(saturate(z)*65535.0)));
#else
output.Store(at,asuint(z));
#endif
output.Store(at+4,s);})";
        auto code=CompileMultisampleShader(shader.c_str(),"cs_5_1");
        D3D12_DESCRIPTOR_RANGE ranges[2]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,2,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,2}};
        D3D12_ROOT_PARAMETER parameters[2]{};parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[0].DescriptorTable={2,ranges};
        parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;parameters[1].Constants={0,0,5};
        D3D12_ROOT_SIGNATURE_DESC root_desc{2,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};ComPtr<ID3DBlob> signature,errors;
        Check(D3D12SerializeRootSignature(&root_desc,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&errors));ComPtr<ID3D12RootSignature> root;
        Check(device->CreateRootSignature(0,signature->GetBufferPointer(),signature->GetBufferSize(),IID_PPV_ARGS(&root)));
        D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline_desc{};pipeline_desc.pRootSignature=root.Get();pipeline_desc.CS={code->GetBufferPointer(),code->GetBufferSize()};
        ComPtr<ID3D12PipelineState> pipeline;Check(device->CreateComputePipelineState(&pipeline_desc,IID_PPV_ARGS(&pipeline)));
        std::vector<u32> expected_pixels;
        for(UINT m=0;m<target.levels;++m)for(UINT l=0;l<target.layers;++l)for(UINT s=0;s<samples;++s)
        for(UINT y=0;y<std::max(target.height>>m,1u);++y)for(UINT x=0;x<std::max(target.width>>m,1u);++x){
            expected_pixels.push_back(bits(m<source.levels&&l<source.layers?1:7,m,l,s,x,y));expected_pixels.push_back(s_bits(9,m,l,s,x,y));}
        const auto pixel_bytes=expected_pixels.size()*4;auto output=GuestTiler::Scratch(device,pixel_bytes);
        auto pixel_readback=Buffer(device,pixel_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        renderer.RetainUntilFinish([root,pipeline,output,pixel_readback](u64){(void)root;(void)pipeline;(void)output;(void)pixel_readback;});
        std::vector<std::shared_ptr<Descriptors>> views;
        grown.texture.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);u32 base_word{};
        for(UINT m=0;m<target.levels;++m){auto descriptors=std::make_shared<Descriptors>(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,3,true);views.push_back(descriptors);
            renderer.RetainUntilFinish([descriptors](u64){(void)descriptors;});
            for(UINT plane=0;plane<2;++plane){D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=plane?DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
                srv.Shader4ComponentMapping=plane?D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(1,1,1,1):D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                if(samples>1){srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;srv.Texture2DMSArray={0,target.layers};}
                else{srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DARRAY;srv.Texture2DArray={m,1,0,target.layers,plane,0};}
                device->CreateShaderResourceView(grown.texture.handle.Get(),&srv,descriptors->Cpu(plane));}
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.Format=DXGI_FORMAT_R32_TYPELESS;uav.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;
            uav.Buffer.NumElements=UINT(pixel_bytes/4);uav.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;device->CreateUnorderedAccessView(output.handle.Get(),nullptr,&uav,descriptors->Cpu(2));
            ID3D12DescriptorHeap* heaps[]{descriptors->Heap()};auto* list=renderer.RecordingList();list->SetDescriptorHeaps(1,heaps);
            list->SetComputeRootSignature(root.Get());list->SetPipelineState(pipeline.Get());list->SetComputeRootDescriptorTable(0,descriptors->Gpu(0));
            const UINT shape[]{std::max(target.width>>m,1u),std::max(target.height>>m,1u),samples,target.layers,base_word};
            list->SetComputeRoot32BitConstants(1,5,shape,0);list->Dispatch((shape[0]+7)/8,(shape[1]+7)/8,samples*target.layers);output.UavBarrier(list);
            base_word+=2*shape[0]*shape[1]*samples*target.layers;
        }
        output.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        renderer.RecordingList()->CopyBufferRegion(pixel_readback.handle.Get(),0,output.handle.Get(),0,pixel_bytes);
        const auto fence=renderer.Flush();cache.Submitted(fence);native->Submitted(fence);cache.Collect(renderer.CompletedFence());
        require(renderer.CompletedFence()<fence&&!image.buffer_sync_resources.empty(),"Growth source leases retired before held GPU fence");
        Check(gate.fence->Signal(1));renderer.Finish();
        void* mapped{};D3D12_RANGE range{0,SIZE_T(readback_bytes)},empty{};Check(backing_readback.handle->Map(0,&range,&mapped));
        bool intact=true;for(const auto& part:segments)for(u64 i=0;i<part.resource->handle->GetDesc().Width;++i)
            intact&=static_cast<const std::byte*>(mapped)[part.offset+i]==(i<part.expected->size()?(*part.expected)[size_t(i)]:std::byte{0x6b});
        backing_readback.handle->Unmap(0,&empty);require(intact,"Growth mapped guest contents, native padding or input seed changed");
        range={0,SIZE_T(pixel_bytes)};Check(pixel_readback.handle->Map(0,&range,&mapped));
        const bool pixels=!memcmp(mapped,expected_pixels.data(),pixel_bytes);pixel_readback.handle->Unmap(0,&empty);
        require(pixels,"Growth native mip/layer/sample pixel oracle failed");
        auto actual_d=dst_d,actual_s=dst_s;native->Resolve(renderer.CompletedFence(),actual_d,actual_s);
        require(actual_d==expected_d&&actual_s==dst_s,"Growth native guest readback lost target-only storage or copied old S");
        require(std::all_of(backing.physical,backing.physical+backing_bytes,[](auto b){return b==std::byte{0x17};}),"Growth published private I into physical B0");
        *reinterpret_cast<volatile unsigned char*>(backing.guest+d_capacity)=0x96;
        require(faults==1&&!live&&!grown.gpu_owned&&!grown.buffer_alias_version,"Growth S CPU write did not retire paired owner");
        cache.SetIndependentVersionTransfer({},{},{});cache.SetIndependentImageTransfer({});cache.SetBufferResidencyPublisher({});renderer.SetCpuPageRelease({});
        cache.Collect(renderer.CompletedFence());renderer.ValidateDebugMessages();++executed;
    }
    require(executed>=12,"Depth growth single-sample cases missing");
    printf("PASS: %u depth mip/layer growth cases, independent typed native pixels, target-only B1/S/padding, input immutability, deferred receipts, real page guards and held-fence leases\n",executed);
}
} // namespace NativeDX12
