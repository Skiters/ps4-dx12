// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_image_ownership.h"
#include "video_core/renderer_d3d12/guest_image_readback.h"
#include "video_core/renderer_d3d12/guest_stage_bindings.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include <bit>
#include <cstring>
namespace NativeDX12 {
inline void RunDepthSubviewFixture(ID3D12Device* device){
    const auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
    unsigned executed{};
    for(const UINT format:{2u,4u})for(const bool paired:{false,true})for(const UINT samples:{1u,4u}){
        printf("CASE: depth subviews format=%u S=%u samples=%u\n",format,unsigned(paired),samples);fflush(stdout);
        const auto native_format=paired?DXGI_FORMAT_D32_FLOAT_S8X24_UINT:format==2?DXGI_FORMAT_D16_UNORM:DXGI_FORMAT_D32_FLOAT;
        if(samples>1){D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS q{native_format,samples,D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE,0};
            Check(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,&q,sizeof(q)));
            if(!q.NumQualityLevels){puts("SKIP: depth subviews MSAA hardware unavailable");continue;}}
        GuestTextureDesc parent;parent.width=13;parent.height=11;parent.pitch=16;parent.tile_mode=8;
        parent.depth_target=true;parent.data_format=format;parent.number_format=format==2?0:7;
        parent.samples=samples;parent.levels=samples>1?1:3;parent.layers=parent.view_layers=3;
        auto layout=LayoutGuestTexture(parent);auto stencil_shape=parent;stencil_shape.stencil_address=0x80000;
        auto sl=LayoutGuestTexture(DescribeGuestStencil(stencil_shape));
        const u64 dc=(layout.guest_bytes+65535)&~u64(65535),sc=paired?(sl.guest_bytes+65535)&~u64(65535):0;
        struct Backing {HANDLE section{};std::byte* guest{};std::byte* physical{};
            ~Backing(){if(physical)UnmapViewOfFile(physical);if(guest)UnmapViewOfFile(guest);if(section)CloseHandle(section);}} backing;
        const u64 new_sc=(sl.guest_bytes+65535)&~u64(65535);
        const u64 capacity=dc+sc+new_sc;backing.section=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,DWORD(capacity>>32),DWORD(capacity),nullptr);
        require(backing.section!=nullptr,"Subview backing section failed");
        backing.guest=static_cast<std::byte*>(MapViewOfFile(backing.section,FILE_MAP_ALL_ACCESS,0,0,SIZE_T(capacity)));
        backing.physical=static_cast<std::byte*>(MapViewOfFile(backing.section,FILE_MAP_ALL_ACCESS,0,0,SIZE_T(capacity)));
        require(backing.guest&&backing.physical,"Subview backing aliases failed");memset(backing.physical,0x17,size_t(capacity));
        parent.address=reinterpret_cast<u64>(backing.guest);parent.stencil_address=paired?parent.address+dc:0;
        const auto bits=[&](UINT phase,UINT m,UINT l,UINT s,UINT x,UINT y){return format==2?
            1u+(phase*997u+m*79u+l*373u+s*127u+x*11u+y*13u)%65530u:
            std::bit_cast<u32>(float(phase*2048u+m*512u+l*256u+s*64u+y*7u+x+1u)/65536.f);};
        const auto sbits=[](UINT m,UINT l,UINT s,UINT x,UINT y){return (37u+m*11u+l*17u+s*31u+y*7u+x*3u)%256u;};
        const auto make_depth=[&](UINT phase){std::vector<std::byte> data(size_t(layout.guest_bytes),std::byte{0xa3});
            for(UINT m=0;m<parent.levels;++m)for(UINT l=0;l<3;++l)for(UINT s=0;s<samples;++s)
            for(UINT y=0;y<std::max(parent.height>>m,1u);++y)for(UINT x=0;x<std::max(parent.width>>m,1u);++x){
                const u32 value=bits(phase,m,l,s,x,y);memcpy(data.data()+GuestPixelOffset(parent,layout,m,l,x,y,s),&value,layout.bytes_per_pixel);}return data;};
        const auto initial=make_depth(1);std::vector<std::byte> initial_s(paired?size_t(sl.guest_bytes):0,std::byte{0xb4});
        if(paired)for(UINT m=0;m<parent.levels;++m)for(UINT l=0;l<3;++l)for(UINT s=0;s<samples;++s)
        for(UINT y=0;y<std::max(parent.height>>m,1u);++y)for(UINT x=0;x<std::max(parent.width>>m,1u);++x){
            const u32 value=sbits(m,l,s,x,y);memcpy(initial_s.data()+GuestPixelOffset(DescribeGuestStencil(parent),sl,m,l,x,y,s),&value,1);}
        GuestTextureCache cache(device);
        struct FixtureRasterizer final:Rasterizer {using Rasterizer::Rasterizer;std::function<void(VAddr,u64)> written;
            void NotifyGpuBufferWrite(VAddr a,u64 n)override{if(written)written(a,n);}} renderer(device);
        renderer.MapMemory(parent.address,capacity);
        // Production memory manager reports committed guest permissions before
        // host PageWatch guards are armed; GPU access is independent of guards.
        renderer.NotifyHostMapping(parent.address,capacity,true,true);
        const auto gpu=[&](const auto& data){const u64 n=(data.size()+65535)&~u64(65535);
            auto upload=Buffer(device,n,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
            auto result=Buffer(device,n,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
            void* mapped{};D3D12_RANGE empty{};Check(upload.handle->Map(0,&empty,&mapped));memset(mapped,0x6b,size_t(n));
            memcpy(mapped,data.data(),data.size());upload.handle->Unmap(0,nullptr);
            renderer.RecordingList()->CopyBufferRegion(result.handle.Get(),0,upload.handle.Get(),0,n);
            result.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            renderer.RetainUntilFinish([upload](u64){(void)upload;});return result;};
        auto source=gpu(initial);Resource source_s;if(paired)source_s=gpu(initial_s);
        auto& image=cache.Upload(parent,{},renderer.RecordingList(),renderer.CompletedFence(),{},true,&source,false,false,
            GuestTextureCache::UploadPurpose::Preparation,paired?&source_s:nullptr);
        GuestTextureCache::Entry* live{};unsigned faults{};std::function<void(GuestTextureCache::Entry&)> retire;
        PageWatch watch([&](u64,bool writing){require(writing&&live,"Unexpected subview PageWatch fault");++faults;
            auto* old=std::exchange(live,nullptr);old->MarkLogicalDirty();retire(*old);});
        renderer.SetCpuPageRelease([&](u64 a,u64 n){return watch.UntrackReadOnly(a,n,true);});
        retire=[&](auto& old){ReleaseGuestImageOwnership(cache,old,[&](u64 a,u64 n,const auto& keep){renderer.ReleaseUnusedReadOnlyWatches(a,n,keep);});};
        const auto acquire=[&](auto& old){const bool ok=AcquireGuestImageOwnership(cache,old,watch,
            [&](u64 a,u64 n){return a>=parent.address&&a-parent.address<=capacity&&n<=capacity-(a-parent.address);},
            [&](u64 a,u64 n){return renderer.GpuOwnedBufferProtectionRanges(a,n);});if(ok)live=&old;return ok;};
        cache.SetIndependentImageRetention([](const auto& d,const auto& l){return d.writable&&!d.depth_target&&!d.stencil_address&&!l.expanded_bytes_per_pixel&&(d.samples==1||GuestTextureCache::CanTransferIndependentMultisample(d,l));});
        cache.SetOwnershipAcquirer(acquire);cache.SetIndependentVersionTransfer({},acquire,retire);
        cache.SetPendingRetirement([&]{renderer.Finish();cache.Collect(renderer.CompletedFence());renderer.RecordingList();});
        require(cache.Own(image),"Subview source ownership failed");
        auto a=parent;a.depth_target=false;a.stencil_address=0;a.levels=a.layers=a.view_layers=1;
        auto b=a;b.levels=std::min(2u,parent.levels);b.layers=2;b.first_layer=1;b.first_mip=samples>1?0:1;
        auto da=a;da.depth_target=true;da.stencil_address=parent.stencil_address;
        auto db=b;db.depth_target=true;db.stencil_address=parent.stencil_address;
        const auto identity=image.image_uid;const auto allocation=image.texture.handle;
        auto pin_a=cache.PinBinding(image),pin_b=cache.PinBinding(image);
        for(const auto& view:{a,b,da,db}){
            auto vl=LayoutGuestTexture(view);require(cache.FindValidIndependentImage(view,vl)==&image&&
                cache.FindIndependentUploadSource(view,vl)==&image&&cache.HasOwnedShaderStorage(view),"Subview lost independent parent lookup");
            const auto storage=cache.ResolveDepthViewStorage(view);require(storage.levels==parent.levels&&storage.layers==3&&
                storage.first_layer==view.first_layer&&storage.first_mip==view.first_mip,"Subview changed full parent geometry");}
        auto invalid=b;invalid.address+=256;require(!CompatibleGuestDepthSubview(parent,layout,invalid,LayoutGuestTexture(invalid)),"Subview accepted rebased alias");
        invalid=b;invalid.layers=4;require(!CompatibleGuestDepthSubview(parent,layout,invalid,LayoutGuestTexture(invalid)),"Subview accepted growth");
        invalid=b;invalid.writable=true;require(!CompatibleGuestDepthSubview(parent,layout,invalid,LayoutGuestTexture(invalid)),"Subview accepted writable color");
        invalid=db;invalid.stencil_address=paired?0:parent.address+dc;
        require(!CompatibleGuestDepthSubview(parent,layout,invalid,LayoutGuestTexture(invalid)),"Subview accepted aspect replacement");
        const GuestTextureCache::BufferResidency ancestor{{parent.address,layout.guest_bytes}};
        image.pending_buffer_residency=ancestor;unsigned publications{};
        cache.SetBufferResidencyPublisher([&](auto ranges){++publications;require(std::equal(ranges.begin(),ranges.end(),ancestor.begin(),ancestor.end()),"Subview receipt extent changed");});
        require(&cache.Upload(a,std::span<const std::byte>(backing.physical,size_t(layout.guest_bytes)),renderer.RecordingList(),renderer.CompletedFence(),{},false,nullptr,false,false,
            GuestTextureCache::UploadPurpose::Preparation)==&image&&!publications,"Subview preparation replaced/published I");
        require(&cache.Upload(b,std::span<const std::byte>(backing.physical,size_t(layout.guest_bytes)),renderer.RecordingList(),renderer.CompletedFence())==&image&&publications==1,"Subview binding lost parent receipt");
        cache.Upload(a,std::span<const std::byte>(backing.physical,size_t(layout.guest_bytes)),renderer.RecordingList(),renderer.CompletedFence());require(publications==1&&image.pending_buffer_residency.empty(),"Subview consumed receipt twice");
        require(image.desc==parent&&image.image_uid==identity&&image.texture.handle==allocation,"Subview mutated allocation identity");
        cache.SetBufferResidencyPublisher({});
        // Independent typed SRV oracle: requested scalar/array dimensions read
        // native mip/layer/sample values, never deriving expected values by detile.
        const std::string shader=std::string(samples>1?"#define MSAA 1\n":"#define MSAA 0\n")+
            (format==2?"#define D16 1\n":"#define D16 0\n")+(paired?"#define STENCIL 1\n":"#define STENCIL 0\n")+R"(
#if MSAA
Texture2DMS<float4> ad:register(t0);Texture2DMSArray<float4> bd:register(t1);
Texture2DMS<uint4> ast:register(t2);Texture2DMSArray<uint4> bst:register(t3);
#else
Texture2D<float4> ad:register(t0);Texture2DArray<float4> bd:register(t1);
Texture2D<uint4> ast:register(t2);Texture2DArray<uint4> bst:register(t3);
#endif
RWByteAddressBuffer output:register(u0);cbuffer Shape:register(b0){uint width,height,samples,which,base;};
[numthreads(8,8,1)]void main(uint3 p:SV_DispatchThreadID){if(p.x>=width||p.y>=height||p.z>=samples)return;
#if MSAA
float d=which?bd.Load(int3(p.xy,0),p.z).x:ad.Load(p.xy,p.z).x;
#else
float d=which?bd.Load(int4(p.xy,0,0)).x:ad.Load(int3(p.xy,0)).x;
#endif
uint s=0;
#if STENCIL
#if MSAA
s=which?bst.Load(int3(p.xy,0),p.z).x:ast.Load(p.xy,p.z).x;
#else
s=which?bst.Load(int4(p.xy,0,0)).x:ast.Load(int3(p.xy,0)).x;
#endif
#endif
uint at=4*(base+2*((p.z*height+p.y)*width+p.x));
#if D16
output.Store(at,uint(round(saturate(d)*65535.0)));
#else
output.Store(at,asuint(d));
#endif
output.Store(at+4,s);})";
        auto code=CompileMultisampleShader(shader.c_str(),"cs_5_1");
        D3D12_DESCRIPTOR_RANGE ranges[2]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,4,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,4}};
        D3D12_ROOT_PARAMETER parameters[2]{};parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[0].DescriptorTable={2,ranges};
        parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;parameters[1].Constants={0,0,5};
        D3D12_ROOT_SIGNATURE_DESC rd{2,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};ComPtr<ID3DBlob> signature,errors;ComPtr<ID3D12RootSignature> root;
        Check(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&errors));Check(device->CreateRootSignature(0,signature->GetBufferPointer(),signature->GetBufferSize(),IID_PPV_ARGS(&root)));
        D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.CS={code->GetBufferPointer(),code->GetBufferSize()};ComPtr<ID3D12PipelineState> pipeline;Check(device->CreateComputePipelineState(&pd,IID_PPV_ARGS(&pipeline)));
        const auto probe=[&](auto& entry,UINT phase){
            std::vector<u32> expected;for(const auto& view:{a,b})for(UINT s=0;s<samples;++s)
            for(UINT y=0;y<std::max(parent.height>>view.first_mip,1u);++y)for(UINT x=0;x<std::max(parent.width>>view.first_mip,1u);++x){
                expected.push_back(bits(phase,view.first_mip,view.first_layer,s,x,y));expected.push_back(paired?sbits(view.first_mip,view.first_layer,s,x,y):0);}
            const auto n=expected.size()*4;auto output=GuestTiler::Scratch(device,n),readback=Buffer(device,n,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            auto descriptors=std::make_shared<Descriptors>(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,5,true);
            UINT index{};for(const auto& view:{a,b}){auto srv=entry.Srv(view);device->CreateShaderResourceView(entry.texture.handle.Get(),&srv,descriptors->Cpu(index));
                srv.Format=DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;srv.Shader4ComponentMapping=D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(1,1,1,1);
                if(samples==1){if(view.layers==1)srv.Texture2D.PlaneSlice=1;else srv.Texture2DArray.PlaneSlice=1;}
                device->CreateShaderResourceView(paired?entry.texture.handle.Get():nullptr,&srv,descriptors->Cpu(2+index));++index;}
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.Format=DXGI_FORMAT_R32_TYPELESS;uav.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;uav.Buffer.NumElements=UINT(n/4);uav.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
            device->CreateUnorderedAccessView(output.handle.Get(),nullptr,&uav,descriptors->Cpu(4));
            auto* list=renderer.RecordingList();entry.texture.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);ID3D12DescriptorHeap* heaps[]{descriptors->Heap()};list->SetDescriptorHeaps(1,heaps);list->SetComputeRootSignature(root.Get());list->SetPipelineState(pipeline.Get());list->SetComputeRootDescriptorTable(0,descriptors->Gpu(0));
            u32 base_word{},which{};for(const auto& view:{a,b}){const UINT shape[]{std::max(parent.width>>view.first_mip,1u),std::max(parent.height>>view.first_mip,1u),samples,which++,base_word};
                list->SetComputeRoot32BitConstants(1,5,shape,0);list->Dispatch((shape[0]+7)/8,(shape[1]+7)/8,samples);output.UavBarrier(list);base_word+=2*shape[0]*shape[1]*samples;}
            output.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);list->CopyBufferRegion(readback.handle.Get(),0,output.handle.Get(),0,n);
            renderer.RetainUntilFinish([output,readback,descriptors,root,pipeline](u64){(void)output;(void)readback;(void)descriptors;(void)root;(void)pipeline;});
            const auto fence=renderer.Flush();cache.Submitted(fence);renderer.Finish();void* mapped{};D3D12_RANGE range{0,n},empty{};Check(readback.handle->Map(0,&range,&mapped));
            const bool same=!memcmp(mapped,expected.data(),n);readback.handle->Unmap(0,&empty);require(same,"Subview typed native SRV pixel oracle failed");cache.Collect(renderer.CompletedFence());renderer.ValidateDebugMessages();};
        puts("PHASE: initial native probe");fflush(stdout);probe(image,1);
        auto expected_d=initial;
        {auto descriptors=std::make_shared<Descriptors>(device,D3D12_DESCRIPTOR_HEAP_TYPE_DSV,2,false);UINT index{};
            for(const auto& view:{da,db}){const float value=index?.75f:.125f;auto dsv=image.Dsv(view);device->CreateDepthStencilView(image.texture.handle.Get(),&dsv,descriptors->Cpu(index++));
                image.texture.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_DEPTH_WRITE);renderer.RecordingList()->ClearDepthStencilView(descriptors->Cpu(index-1),D3D12_CLEAR_FLAG_DEPTH,value,0,0,nullptr);
                const u32 raw=format==2?UINT(std::lround(value*65535.f)):std::bit_cast<u32>(value);
                for(UINT s=0;s<samples;++s)for(UINT y=0;y<std::max(parent.height>>view.first_mip,1u);++y)for(UINT x=0;x<std::max(parent.width>>view.first_mip,1u);++x)
                    memcpy(expected_d.data()+GuestPixelOffset(parent,layout,view.first_mip,view.first_layer,x,y,s),&raw,layout.bytes_per_pixel);}
            renderer.RetainUntilFinish([descriptors](u64){(void)descriptors;});}
        const auto check_full=[&](auto& entry,const auto& expected){auto readback=std::make_shared<GuestImageReadback>(device,entry,renderer.RecordingList());
            renderer.RetainUntilFinish([readback](u64){(void)readback;});const auto fence=renderer.Flush();cache.Submitted(fence);readback->Submitted(fence);renderer.Finish();
            auto actual=initial,actual_s=initial_s;readback->Resolve(renderer.CompletedFence(),actual,actual_s);require(actual==expected&&actual_s==initial_s,"Subview full parent native contents/S preservation failed");cache.Collect(renderer.CompletedFence());renderer.ValidateDebugMessages();};
        puts("PHASE: DSV full parent readback");fflush(stdout);check_full(image,expected_d);pin_a.reset();pin_b.reset();
        require(std::all_of(backing.physical,backing.physical+capacity,[](auto v){return v==std::byte{0x17};}),"Subview native operation published into B0");
        if(samples==1){
            // Real CPU fault retires contents; cached extent must still describe
            // the full parent. Pinned RefreshImage refreshes D only, preserving S.
            *reinterpret_cast<volatile unsigned char*>(backing.guest)=0x96;
            require(faults==1&&!live&&!image.gpu_owned&&!cache.HasOwnedShaderStorage(b)&&!cache.FindValidIndependentImage(b,LayoutGuestTexture(b)),"Subview dirty parent revived stale I");
            const auto storage=cache.ResolveDepthViewStorage(db);require(storage.levels==3&&storage.layers==3,"Subview dirtying lost allocation extent");
            auto cpu=make_depth(3);memcpy(backing.physical,cpu.data(),cpu.size());
            puts("PHASE: CPU full parent refresh");fflush(stdout);
            auto& refreshed=cache.Upload(storage,std::span<const std::byte>(backing.physical,size_t(layout.guest_bytes)),renderer.RecordingList(),renderer.CompletedFence(),paired?std::span<const std::byte>(backing.physical+dc,size_t(sl.guest_bytes)):std::span<const std::byte>{});
            probe(refreshed,3);check_full(refreshed,cpu);require(cache.Own(refreshed),"Refreshed subview ownership failed");
            auto held=cache.PinBinding(refreshed);*reinterpret_cast<volatile unsigned char*>(backing.guest)=0x95;
            const auto replacement_data=make_depth(5);auto replacement_gpu=gpu(replacement_data);
            struct Gate {ComPtr<ID3D12Fence> fence;~Gate(){if(fence)fence->Signal(1);}} gate;
            Check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&gate.fence)));renderer.WaitExternalFence(gate.fence.Get(),1);
            puts("PHASE: pinned GPU B refresh");fflush(stdout);
            auto& replacement=cache.Upload(cache.ResolveDepthViewStorage(db),{},renderer.RecordingList(),renderer.CompletedFence(),{},true,&replacement_gpu,false,true);
            require(replacement.texture.handle!=refreshed.texture.handle&&replacement.desc.levels==3&&replacement.desc.layers==3,"Pinned depth refresh reused/mis-sized native allocation");
            const auto fence=renderer.Flush();cache.Submitted(fence);cache.Collect(renderer.CompletedFence());
            require(renderer.CompletedFence()<fence&&(!paired||!replacement.native_plane_sources.empty()),"Native S source lifetime ended before GPU completion");
            Check(gate.fence->Signal(1));renderer.Finish();probe(replacement,5);check_full(replacement,replacement_data);held.reset();
            // Only exact-base GPU invalidation is in the pinned original. The
            // interior writer must keep I even though its B bytes differ.
            renderer.SetBufferOwnership([&](VAddr a,u64 n){return watch.TrackOwned(a,n);},
                [&](VAddr a,std::span<const std::byte> bytes){memcpy(backing.physical+(a-parent.address),bytes.data(),bytes.size());return true;},[&](VAddr a,u64 n){const auto pages=GuestProtectionPages(a,n);require(bool(pages),"Invalid buffer watch retirement");
                    (void)watch.ChangeMode(pages->address,pages->size,true);require(watch.UntrackReadOnly(pages->address,pages->size),"Buffer watch retirement failed");});
            renderer.written=[&](VAddr address,u64 size){if(size)for(const auto& old:cache.Entries())if(old->desc.address==address){
                old->MarkLogicalDirty();if(old->gpu_owned){if(live==old.get())live=nullptr;retire(*old);}}};
            puts("PHASE: ordered B setup");fflush(stdout);
            require(cache.Own(replacement),"Ordered parent ownership failed");
            const Rasterizer::ShaderBufferRequest full{parent.address,dc,false,true,true};
            renderer.PrepareShaderBuffers(std::span(&full,1));
            const UINT fill=format==2?0x44444444u:0x3f000000u;
            renderer.FillPreparedShaderBuffer(parent.address,dc,fill,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            for(const bool interior:{true,false}){
                const OrderedGuestBuffer buffers[]{{false,false,false,0,4,4}};
                const OrderedGuestImage images[]{{a,LayoutGuestTexture(a),false,false},{b,LayoutGuestTexture(b),false,false}};
                const GuestStageBindingSpan stages[]{{0,0,0,1},{0,1,1,2}};
                std::vector<Rasterizer::ShaderBufferRequest> requests{{parent.address+(interior?4:0),4,true,true,true}};
                printf("PHASE: ordered interior=%u\n",unsigned(interior));fflush(stdout);
                auto prepared=PrepareOrderedGuestStages(device,renderer,cache,stages,requests,buffers,images,true,
                    [](VAddr,u64)->VAddr{throw std::runtime_error("Ordered depth unexpectedly requested private CPU stream");});
                require(prepared.images[0]&&prepared.images[0]==prepared.images[1]&&prepared.images[0]->desc.levels==3&&prepared.images[0]->desc.layers==3,
                    "Ordered depth views lost shared full parent allocation");
                auto& bound=*prepared.images[0];
                if(interior){require(bound.texture.handle==replacement.texture.handle,"Interior GPU writer replaced independent I");probe(bound,5);check_full(bound,replacement_data);}
                else{
                    auto expected=initial;
                    for(UINT m=0;m<parent.levels;++m)for(UINT l=0;l<3;++l)for(UINT y=0;y<std::max(parent.height>>m,1u);++y)for(UINT x=0;x<std::max(parent.width>>m,1u);++x)
                        memcpy(expected.data()+GuestPixelOffset(parent,layout,m,l,x,y),&fill,layout.bytes_per_pixel);
                    check_full(bound,expected);expected_d=expected;
                }
            }
            renderer.written={};

        }
        // Original ResolveDepthOverlap recreates a different depth aspect at
        // max(requested,cached) resources. A one-mip/one-layer request must not
        // capture a byte prefix of the three-layer/mip native source.
        require(live&&live->gpu_owned,"Recreation lost preceding live owner");auto* preceding=live;
        auto requested=parent;requested.stencil_address=parent.address+dc+sc;
        requested.levels=requested.layers=requested.view_layers=1;
        const auto recreated=cache.ResolveDepthViewStorage(requested);
        require(recreated.levels==parent.levels&&recreated.layers==parent.layers&&recreated.view_layers==1&&
            recreated.stencil_address==requested.stencil_address,"Depth recreation shrank original maximum resource extent");
        require(cache.FindIndependentUploadSource(recreated,LayoutGuestTexture(recreated))==preceding,
            "Depth recreation lost the full native I source");
        auto bad=requested;bad.pitch+=16;
        require(cache.ResolveDepthViewStorage(bad)==bad,"Depth recreation promoted incompatible block geometry");
        std::vector<std::byte> seed_d(size_t(layout.guest_bytes),std::byte{0x83});
        for(UINT m=0;m<parent.levels;++m)for(UINT l=0;l<parent.layers;++l)for(UINT sample=0;sample<samples;++sample)
        for(UINT y=0;y<std::max(parent.height>>m,1u);++y)for(UINT x=0;x<std::max(parent.width>>m,1u);++x){
            const u32 value=bits(7,m,l,sample,x,y);memcpy(seed_d.data()+GuestPixelOffset(parent,layout,m,l,x,y,sample),&value,layout.bytes_per_pixel);}
        std::vector<std::byte> seed_s(size_t(sl.guest_bytes),std::byte{0x93});
        auto new_d=gpu(seed_d),new_s=gpu(seed_s);unsigned captures{};
        cache.SetIndependentImageTransfer([&](auto& old,const auto& target,const auto& target_layout){
            require(&old==preceding&&target==recreated&&target_layout.guest_bytes==layout.guest_bytes,
                "Recreation capture lost full parent target descriptor");
            return cache.CaptureIndependentImageVersion(old,target,target_layout,renderer.RecordingList(),renderer.CompletedFence(),
                [&](u64 address,u64 bytes){++captures;
                    if(address==parent.address){require(bytes==layout.guest_bytes,"Recreation D seed was truncated");return new_d;}
                    require(address==requested.stencil_address&&bytes==sl.guest_bytes,"Recreation S seed was truncated");return new_s;});});
        puts("PHASE: aspect recreation keeps source resource extent");fflush(stdout);
        auto& result=cache.Upload(recreated,std::span<const std::byte>(backing.physical,size_t(layout.guest_bytes)),
            renderer.RecordingList(),renderer.CompletedFence(),std::span<const std::byte>(backing.physical+dc+sc,size_t(sl.guest_bytes)),
            false,nullptr,false,false,GuestTextureCache::UploadPurpose::Preparation);
        require(&result!=preceding&&result.desc.levels==parent.levels&&result.desc.layers==parent.layers&&
            result.texture.handle->GetDesc().MipLevels==parent.levels&&result.texture.handle->GetDesc().DepthOrArraySize==parent.layers&&
            result.gpu_owned&&!preceding->gpu_owned&&captures==2&&result.pending_buffer_residency.empty(),
            "Recreation lost full allocation/ownership or invented growth residency");
        auto recreated_readback=std::make_shared<GuestImageReadback>(device,result,renderer.RecordingList());
        renderer.RetainUntilFinish([recreated_readback](u64){(void)recreated_readback;});
        const auto recreate_fence=renderer.Flush();cache.Submitted(recreate_fence);recreated_readback->Submitted(recreate_fence);renderer.Finish();
        auto expected_recreated=seed_d;
        for(UINT m=0;m<parent.levels;++m)for(UINT l=0;l<parent.layers;++l)for(UINT sample=0;sample<samples;++sample)
        for(UINT y=0;y<std::max(parent.height>>m,1u);++y)for(UINT x=0;x<std::max(parent.width>>m,1u);++x){
            const auto offset=GuestPixelOffset(parent,layout,m,l,x,y,sample);
            memcpy(expected_recreated.data()+offset,expected_d.data()+offset,layout.bytes_per_pixel);}
        auto actual_recreated=seed_d,actual_recreated_s=seed_s;
        recreated_readback->Resolve(renderer.CompletedFence(),actual_recreated,actual_recreated_s);
        require(actual_recreated==expected_recreated&&actual_recreated_s==seed_s,
            "Recreated depth lost native parent mip/layer/sample values or copied old S instead of new seed");
        cache.Collect(renderer.CompletedFence());renderer.ValidateDebugMessages();cache.SetIndependentImageTransfer({});

        // Source snapshots, including all capacity padding, must remain immutable.
        const auto check_input=[&](Resource& input,const auto& expected){const auto n=input.handle->GetDesc().Width;
            auto readback=Buffer(device,n,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);input.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            renderer.RecordingList()->CopyBufferRegion(readback.handle.Get(),0,input.handle.Get(),0,n);renderer.RetainUntilFinish([readback](u64){(void)readback;});renderer.Finish();
            void* mapped{};D3D12_RANGE range{0,SIZE_T(n)},empty{};Check(readback.handle->Map(0,&range,&mapped));bool same=true;for(u64 i=0;i<n;++i)same&=static_cast<const std::byte*>(mapped)[i]==(i<expected.size()?expected[size_t(i)]:std::byte{0x6b});
            readback.handle->Unmap(0,&empty);require(same,"Depth refresh mutated borrowed GPU input/padding");};
        check_input(source,initial);if(paired)check_input(source_s,initial_s);
        if(live){retire(*live);live=nullptr;}cache.SetIndependentVersionTransfer({},{},{});cache.SetOwnershipAcquirer({});renderer.SetCpuPageRelease({});renderer.ValidateDebugMessages();++executed;
    }
    require(executed>=4,"Depth subview single-sample coverage missing");
    printf("PASS: %u depth subview cases; typed scalar/array SRVs, selected DSV writes, full parent contents/S, deferred B receipts, real PageWatch, dirty extent, forced replacement/fence leases, immutable GPU inputs, full-resource aspect recreation\n",executed);
}
} // namespace NativeDX12
