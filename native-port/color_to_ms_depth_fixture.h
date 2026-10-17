// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_image_ownership.h"
#include "video_core/renderer_d3d12/guest_image_readback.h"
#include "video_core/renderer_d3d12/multisample_readback.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include <bit>
#include <cmath>

namespace NativeDX12 {
inline void RunColorToMsDepthFixture(ID3D12Device* device,bool srgb=false,bool wide=false,UINT pool_geometry=0,bool retain_extent=false){
    const auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    unsigned cases{};
    const auto counts=wide?std::array<UINT,2>{8,16}:std::array<UINT,2>{2,4};
    for(UINT color_kind=srgb?2:0;color_kind<(srgb?3u:wide?1u:2u);++color_kind)for(UINT format:{2u,4u})
    for(bool stencil:{false,true})for(UINT samples:counts){
        if((pool_geometry||retain_extent)&&(color_kind!=0||format!=4||samples!=4))continue;
        if(wide){D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS quality{stencil?DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
                format==2?DXGI_FORMAT_D16_UNORM:DXGI_FORMAT_D32_FLOAT,samples,D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE,0};
            Check(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,&quality,sizeof(quality)));
            if(!quality.NumQualityLevels){printf("SKIP: hardware depth samples=%u fmt=%u S=%u unavailable\n",samples,format,stencil);continue;}}
        GuestTextureDesc source;source.width=13;source.height=11;source.pitch=16;
        source.layers=source.view_layers=2;source.levels=pool_geometry?2:1;source.tile_mode=8;source.writable=true;
        source.data_format=color_kind==0?14:color_kind==1?11:10;source.number_format=color_kind==2?9:7;
        // The upstream helper creates its own mip/layer-zero view. Guest
        // binding selectors/swizzles must not leak into this copy operation.
        source.first_mip=pool_geometry?1:0;source.first_layer=1;source.view_layers=1;source.swizzle={7,6,5,4};
        GuestTextureDesc target;target.width=7;target.height=pool_geometry==2?5:11;target.pitch=pool_geometry==1?8:16;
        target.layers=2;target.view_layers=retain_extent?1:2;target.tile_mode=color_kind==1?0:8;
        target.samples=samples;target.depth_target=true;target.data_format=format;target.number_format=format==2?0:7;
        target.stencil_address=stencil?0x80000:0;
        const auto sl=LayoutGuestTexture(source),dl=LayoutGuestTexture(target);
        auto st=stencil?LayoutGuestTexture(DescribeGuestStencil(target)):GuestTextureLayout{};
        const auto capacity=(std::max(sl.guest_bytes,dl.guest_bytes)+65535)&~u64(65535);
        const auto sc=stencil?(st.guest_bytes+65535)&~u64(65535):0;
        struct Backing {HANDLE section{};std::byte *guest{},*physical{};
            ~Backing(){if(physical)UnmapViewOfFile(physical);if(guest)UnmapViewOfFile(guest);if(section)CloseHandle(section);}} backing;
        const u64 total=capacity+sc;
        backing.section=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,DWORD(total>>32),DWORD(total),nullptr);
        require(backing.section!=nullptr,"Channel transfer physical section failed");
        backing.guest=static_cast<std::byte*>(MapViewOfFile(backing.section,FILE_MAP_ALL_ACCESS,0,0,SIZE_T(total)));
        backing.physical=static_cast<std::byte*>(MapViewOfFile(backing.section,FILE_MAP_ALL_ACCESS,0,0,SIZE_T(total)));
        require(backing.guest&&backing.physical,"Channel transfer physical aliases failed");
        const auto base=reinterpret_cast<u64>(backing.guest);source.address=target.address=base;
        target.stencil_address=stencil?base+capacity:0;memset(backing.physical,0x17,size_t(total));
        const bool cache_overlap=source.pitch==target.pitch&&source.height==target.height&&
            uint64_t(sl.bytes_per_pixel)*source.samples==uint64_t(dl.bytes_per_pixel)*samples;
        printf("CASE: color=%u -> D%u S=%u samples=%u, scaled 13x11 -> %ux%u pitch=%u cache-overlap=%u\n",source.data_format,format==2?16:32,stencil,samples,target.width,target.height,target.pitch,cache_overlap);fflush(stdout);
        const auto component=[&](UINT x,UINT y,UINT c,UINT layer,UINT mip){return wide?
            float(c+1+layer*5+mip*7)/16.f:float(1+(x*3+y*7+c*53+layer*31+mip*17)%251)/256.f;};
        const auto byte=[](UINT x,UINT y,UINT c,UINT layer,UINT mip){return 1+(x*3+y*7+c*53+layer*31+mip*17)%251;};
        const auto sampled=[&](UINT x,UINT y,UINT c){
            if(wide&&c>=4)return 0.f; // Explicit native choice for undefined upstream channels.
            if(color_kind==1&&c>1)return c==3?1.f:0.f;
            if(color_kind!=2)return component(x,y,c,0,0);
            const float s=float(byte(x,y,c,0,0))/255.f;
            return c==3?s:s<=0.04045f?s/12.92f:std::pow((s+0.055f)/1.055f,2.4f);
        };
        std::vector<std::byte> color(size_t(sl.guest_bytes),std::byte{0xa3});
        for(UINT m=0;m<source.levels;++m)for(UINT l=0;l<source.layers;++l)
        for(UINT y=0;y<std::max(source.height>>m,1u);++y)for(UINT x=0;x<std::max(source.width>>m,1u);++x){
            auto* at=color.data()+GuestPixelOffset(source,sl,m,l,x,y);
            for(UINT c=0;c<(color_kind==1?2u:4u);++c){
                if(color_kind==2)at[c]=std::byte(byte(x,y,c,l,m));
                else{const float value=component(x,y,c,l,m);memcpy(at+4*c,&value,4);}
            }
        }
        std::vector<std::byte> depth(size_t(dl.guest_bytes),std::byte{0x83}),sbytes(size_t(st.guest_bytes),std::byte{0x91});
        const auto seed=[](UINT l,UINT s,UINT x,UINT y){return float(32768+l*1024+s*128+y*8+x)/65535.f;};
        const auto sbits=[](UINT l,UINT s,UINT x,UINT y){return (l*43+s*29+y*7+x*3+11)%256;};
        for(UINT l=0;l<target.layers;++l)for(UINT s=0;s<samples;++s)for(UINT y=0;y<target.height;++y)for(UINT x=0;x<target.width;++x){
            const auto value=format==2?u32(std::round(seed(l,s,x,y)*65535.f)):std::bit_cast<u32>(seed(l,s,x,y));
            memcpy(depth.data()+GuestPixelOffset(target,dl,0,l,x,y,s),&value,dl.bytes_per_pixel);
            if(stencil)sbytes[size_t(GuestPixelOffset(DescribeGuestStencil(target),st,0,l,x,y,s))]=std::byte(sbits(l,s,x,y));
        }
        GuestTextureCache cache(device);Rasterizer renderer(device);
        cache.SetIndependentImageRetention([](const auto&,const auto&){return true;});
        cache.SetBufferAliasRetention([](const auto&,const auto&){return true;});
        const auto gpu=[&](const std::vector<std::byte>& bytes){
            const auto n=(bytes.size()+65535)&~size_t(65535);auto up=Buffer(device,n,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
            auto out=Buffer(device,n,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
            void* data{};Check(up.handle->Map(0,nullptr,&data));memset(data,0x6b,n);memcpy(data,bytes.data(),bytes.size());up.handle->Unmap(0,nullptr);
            renderer.RecordingList()->CopyBufferRegion(out.handle.Get(),0,up.handle.Get(),0,n);
            out.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            renderer.RetainUntilFinish([up,out](u64){(void)up;(void)out;});return out;
        };
        auto old=gpu(color),b_depth=gpu(depth),b_stencil=stencil?gpu(sbytes):Resource{};
        auto& image=cache.Upload(source,{},renderer.RecordingList(),renderer.CompletedFence(),{},true,&old,
            true,false,GuestTextureCache::UploadPurpose::Preparation);image.NoteLogicalImageWrite();
        GuestTextureCache::Entry* live{};unsigned faults{};std::function<void(GuestTextureCache::Entry&)> retire;
        PageWatch watch([&](u64,bool writing){require(writing&&live,"Unexpected channel transfer CPU fault");++faults;
            auto* old_owner=std::exchange(live,nullptr);old_owner->MarkLogicalDirty();retire(*old_owner);});
        renderer.SetCpuPageRelease([&](u64 a,u64 n){return watch.UntrackReadOnly(a,n,true);});
        retire=[&](auto& e){ReleaseGuestImageOwnership(cache,e,[&](u64 a,u64 n,const auto& keep){renderer.ReleaseUnusedReadOnlyWatches(a,n,keep);});};
        const auto acquire=[&](auto& e){return AcquireGuestImageOwnership(cache,e,watch,
            [&](u64 a,u64 n){return a>=base&&a-base<=total&&n<=total-(a-base);},[](u64,u64){return std::vector<std::pair<u64,u64>>{};});};
        require(acquire(image),"Channel transfer could not acquire source I");
        if(retain_extent){
            auto requested=target;requested.layers=1;
            const auto storage=cache.ResolveDepthViewStorage(requested);
            require(storage==target,"Color/MS depth recreation did not preserve cached maximum resources/selectors");
            // All subsequent production captures, allocation, native draws and
            // independent full-layer/sample/S/padding oracles use this storage.
            target=storage;
            auto bad=requested;bad.pitch+=16;
            require(cache.ResolveDepthViewStorage(bad)==bad,"Color/MS extent promotion bypassed original block geometry");
        }
        GuestTextureCache::BufferResidency receipt{{base,sl.guest_bytes}},published;unsigned publications{},captures{};
        image.pending_buffer_residency=receipt;cache.SetBufferResidencyPublisher([&](auto r){++publications;published.assign(r.begin(),r.end());});
        const auto capture=[&](u64 a,u64 n){++captures;Resource* input{};
            if(a==base&&n==dl.guest_bytes)input=&b_depth;
            else if(stencil&&a==target.stencil_address&&n==st.guest_bytes)input=&b_stencil;
            require(input!=nullptr,"Channel initialization seed range mismatch");
            auto out=GuestTiler::Scratch(device,input->handle->GetDesc().Width);
            input->Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);out.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
            renderer.RecordingList()->CopyBufferRegion(out.handle.Get(),0,input->handle.Get(),0,input->handle->GetDesc().Width);
            out.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);return out;
        };
        auto bad=target;bad.samples=0;bool rejected=false;
        try{cache.CaptureIndependentImageVersion(image,bad,dl,renderer.RecordingList(),renderer.CompletedFence(),capture);}
        catch(const std::invalid_argument&){rejected=true;}
        require(rejected&&!captures&&image.gpu_owned,"Invalid sample geometry accepted or source mutated");
        auto integer=source;integer.number_format=4;
        require(!GuestMultisample::CanReinterpretColorAsMsDepth(integer,LayoutGuestTexture(integer),target,dl),
            "Integer color source accepted by float-sampler depth contract");
        bad=target;bad.address+=4;rejected=false;
        try{cache.CaptureIndependentImageVersion(image,bad,dl,renderer.RecordingList(),renderer.CompletedFence(),capture);}
        catch(const std::invalid_argument&){rejected=true;}
        require(rejected&&!captures&&image.gpu_owned,"Rebased channel overlap was accepted without a mapping plan");
        cache.SetIndependentVersionTransfer({},acquire,retire);
        cache.SetIndependentImageTransfer([&](auto& e,const auto& d,const auto& l){return cache.CaptureIndependentImageVersion(e,d,l,
            renderer.RecordingList(),renderer.CompletedFence(),capture);});
        require(cache.FindIndependentUploadSource(target,dl)==(cache_overlap?&image:nullptr),"Channel cache admitted a different physical block class or lost compatible I");
        struct Gate {ComPtr<ID3D12Fence> fence;~Gate(){if(fence)fence->Signal(1);}} gate;
        Check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&gate.fence)));renderer.WaitExternalFence(gate.fence.Get(),1);
        std::vector<std::byte> cpu_d(size_t(dl.guest_bytes),std::byte{0x17}),cpu_s(size_t(st.guest_bytes),std::byte{0x17});
        GuestTextureCache::Entry* converted_ptr{};std::shared_ptr<GuestImageReadback> pool_seed;
        if(cache_overlap){
            converted_ptr=&cache.Upload(target,cpu_d,renderer.RecordingList(),renderer.CompletedFence(),cpu_s,false,nullptr,
                false,false,GuestTextureCache::UploadPurpose::Preparation);
        }else{
            // A same-address allocation of another block size must preserve
            // the preceding I and initialize the new one from its own B seed.
            // It must not invoke the original cache-overlap blit callback.
            rejected=false;try{cache.CaptureIndependentImageVersion(image,target,dl,renderer.RecordingList(),renderer.CompletedFence(),capture);}
            catch(const std::invalid_argument&){rejected=true;}
            require(rejected&&!captures&&image.gpu_owned,"Different block class reached cache image capture");
            auto& fresh=cache.Upload(target,depth,renderer.RecordingList(),renderer.CompletedFence(),sbytes,false,nullptr,
                false,false,GuestTextureCache::UploadPurpose::Preparation);
            require(image.gpu_owned&&image.HasValidLogicalImage()&&!fresh.logical_gpu_modified&&
                !captures&&fresh.pending_buffer_residency.empty()&&fresh.color_depth_leases.empty(),
                "Pool collision inherited/copied/retired old I or fabricated residency");
            pool_seed=std::make_shared<GuestImageReadback>(device,fresh,renderer.RecordingList());
            renderer.RetainUntilFinish([pool_seed](u64){(void)pool_seed;});
            // Independently exercise the public production blit helper even
            // where the cache must not choose it implicitly. This explicit
            // command has a broader resource contract than ResolveOverlap.
            GuestMultisample transfer(device);
            auto lease=transfer.PrepareColorAsMsDepth(image.texture,fresh.texture,source,sl,target,dl);
            fresh.color_depth_leases.push_back(lease);image.pending=fresh.pending=true;
            lease->Record(renderer.RecordingList(),image.texture,fresh.texture);
            fresh.NoteLogicalImageWrite();fresh.pending_buffer_residency=image.pending_buffer_residency;
            require(acquire(fresh),"Explicit primitive target could not acquire ownership");retire(image);
            converted_ptr=&fresh;
        }
        auto& converted=*converted_ptr;live=&converted;
        require(converted.gpu_owned&&converted.buffer_alias_version&&converted.HasValidLogicalImage()&&!image.gpu_owned&&
            captures==(cache_overlap?1+unsigned(stencil):0)&&!publications&&converted.pending_buffer_residency==receipt&&converted.color_depth_leases.size()==1,
            "Channel transfer lost owner, deferred provenance or production draw lease");
        cache.PublishBufferResidency(converted);cache.PublishBufferResidency(converted);
        require(publications==1&&published==receipt,"Channel transfer published invented seed residency or lost ancestor receipt");
        // Independent native SRVs: no guest tiler or production transfer shader
        // computes the expected source-coordinate/component/sample values.
        const std::string shader=std::string(stencil?"#define STENCIL 1\n":"#define STENCIL 0\n")+R"(
Texture2DMSArray<float4> D:register(t0);Texture2DMSArray<uint4> S:register(t1);RWByteAddressBuffer O:register(u0);
cbuffer Shape:register(b0){uint w,h,samples,layers;};
[numthreads(8,8,1)]void main(uint3 p:SV_DispatchThreadID){if(p.x>=w||p.y>=h||p.z>=samples*layers)return;
uint at=8*((p.z*h+p.y)*w+p.x);O.Store(at,asuint(D.Load(int3(p.xy,p.z/samples),p.z%samples).x));
#if STENCIL
O.Store(at+4,S.Load(int3(p.xy,p.z/samples),p.z%samples).x);
#else
O.Store(at+4,0);
#endif
})";
        auto code=CompileMultisampleShader(shader.c_str(),"cs_5_1");
        D3D12_DESCRIPTOR_RANGE ranges[2]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,2,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,2}};
        D3D12_ROOT_PARAMETER parameters[2]{};parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[0].DescriptorTable={2,ranges};
        parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;parameters[1].Constants={0,0,4};
        D3D12_ROOT_SIGNATURE_DESC signature{2,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};ComPtr<ID3DBlob> blob,errors;
        Check(D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors));ComPtr<ID3D12RootSignature> root;
        Check(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root)));
        D3D12_COMPUTE_PIPELINE_STATE_DESC ps{};ps.pRootSignature=root.Get();ps.CS={code->GetBufferPointer(),code->GetBufferSize()};ComPtr<ID3D12PipelineState> pipeline;
        Check(device->CreateComputePipelineState(&ps,IID_PPV_ARGS(&pipeline)));
        const u64 pixel_bytes=u64(target.width)*target.height*target.layers*samples*8;
        auto output=GuestTiler::Scratch(device,pixel_bytes),pixels=Buffer(device,pixel_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        auto descriptors=std::make_shared<Descriptors>(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,3,true);
        for(UINT plane=0;plane<2;++plane){D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
            srv.Format=plane?DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:stencil?DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
                format==2?DXGI_FORMAT_R16_UNORM:DXGI_FORMAT_R32_FLOAT;
            srv.Shader4ComponentMapping=plane?D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(1,1,1,1):D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;srv.Texture2DMSArray={0,target.layers};
            device->CreateShaderResourceView(plane&&!stencil?nullptr:converted.texture.handle.Get(),&srv,descriptors->Cpu(plane));}
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.Format=DXGI_FORMAT_R32_TYPELESS;uav.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;
        uav.Buffer.NumElements=UINT(pixel_bytes/4);uav.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
        device->CreateUnorderedAccessView(output.handle.Get(),nullptr,&uav,descriptors->Cpu(2));
        renderer.RetainUntilFinish([root,pipeline,descriptors,output,pixels](u64){(void)root;(void)pipeline;(void)descriptors;(void)output;(void)pixels;});
        auto* list=renderer.RecordingList();converted.texture.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        ID3D12DescriptorHeap* heaps[]{descriptors->Heap()};list->SetDescriptorHeaps(1,heaps);list->SetComputeRootSignature(root.Get());list->SetPipelineState(pipeline.Get());
        list->SetComputeRootDescriptorTable(0,descriptors->Gpu(0));const UINT shape[]{target.width,target.height,samples,target.layers};list->SetComputeRoot32BitConstants(1,4,shape,0);
        list->Dispatch((target.width+7)/8,(target.height+7)/8,target.layers*samples);output.UavBarrier(list);output.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
        list->CopyBufferRegion(pixels.handle.Get(),0,output.handle.Get(),0,pixel_bytes);
        auto native=std::make_shared<GuestImageReadback>(device,converted,list);renderer.RetainUntilFinish([native](u64){(void)native;});
        const u64 old_n=old.handle->GetDesc().Width,d_n=b_depth.handle->GetDesc().Width,s_n=stencil?b_stencil.handle->GetDesc().Width:0;
        auto inputs=Buffer(device,old_n+d_n+s_n,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        renderer.RetainUntilFinish([inputs](u64){(void)inputs;});u64 offset{};
        for(Resource* input:{&old,&b_depth,&b_stencil})if(input->handle){input->Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
            list->CopyBufferRegion(inputs.handle.Get(),offset,input->handle.Get(),0,input->handle->GetDesc().Width);offset+=input->handle->GetDesc().Width;}
        const auto fence=renderer.Flush();cache.Submitted(fence);native->Submitted(fence);if(pool_seed)pool_seed->Submitted(fence);cache.Collect(renderer.CompletedFence());
        require(renderer.CompletedFence()<fence&&converted.color_depth_leases.size()==1&&image.pending==false&&image.fence==fence,
            "Color draw lease or source allocation retired before real GPU completion");
        Check(gate.fence->Signal(1));renderer.Finish();
        if(pool_seed){auto actual_d=depth,actual_s=sbytes;pool_seed->Resolve(renderer.CompletedFence(),actual_d,actual_s);
            require(actual_d==depth&&actual_s==sbytes,"Different block class was not initialized from its own B contents");}
        // Standard native sample locations, in sixteenths. The pinned shader
        // uses sample-frequency UVs and source textureSize, not target coords.
        const UINT xy2[2][2]{{12,12},{4,4}},xy4[4][2]{{6,2},{14,6},{2,10},{10,14}};
        // Wide-sample cases use constant source planes, so the choice of UV
        // interpolation location cannot affect their channel/default oracle.
        const auto sample_xy=[&](UINT s){return wide?xy2[0]:samples==2?xy2[s]:xy4[s];};
        const auto bounds=[&](UINT l,UINT s,UINT x,UINT y){
            const auto* xy=sample_xy(s);
            const UINT sx=((16*x+xy[0])*source.width)/(16*target.width),sy=((16*y+xy[1])*source.height)/(16*target.height);
            const float ideal=l?seed(l,s,x,y):sampled(sx,sy,s);
            if(l||color_kind!=2||s==3)return std::pair{ideal,ideal};
            // D3D functional spec 3.2.3.7 measures sRGB precision on the
            // encoded side: half an 8-bit step, before integer rounding.
            // Map that interval through the independent ideal decoder.
            const auto decode=[](float c){return c<=0.04045f?c/12.92f:std::pow((c+0.055f)/1.055f,2.4f);};
            const float encoded=float(byte(sx,sy,s,0,0));
            return std::pair{decode(std::max(encoded-0.5f,0.f)/255.f),decode(std::min(encoded+0.5f,255.f)/255.f)};
        };
        auto expected=depth;void* data{};D3D12_RANGE range{0,SIZE_T(pixel_bytes)},empty{};Check(pixels.handle->Map(0,&range,&data));
        bool correct=true;std::vector<float> native_values(size_t(pixel_bytes/8));
        for(UINT l=0;l<target.layers;++l)for(UINT s=0;s<samples;++s)for(UINT y=0;y<target.height;++y)for(UINT x=0;x<target.width;++x){
            const auto* xy=sample_xy(s);
            const UINT sx=((16*x+xy[0])*source.width)/(16*target.width),sy=((16*y+xy[1])*source.height)/(16*target.height);
            float value=l?seed(l,s,x,y):sampled(sx,sy,s);
            const auto at=2*(((l*samples+s)*target.height+y)*target.width+x);
            const auto* words=static_cast<const u32*>(data);
            const float actual=std::bit_cast<float>(words[at]);
            native_values[at/2]=actual;
            const auto [low,high]=bounds(l,s,x,y);
            const float tolerance=format==2&&!stencil?0.6001f/65535.f:0.0000002f;
            if(!std::isfinite(actual)||actual<low-tolerance||actual>high+tolerance||words[at+1]!=(stencil?sbits(l,s,x,y):0)){
                if(correct)fprintf(stderr,"Channel oracle l=%u s=%u x=%u y=%u: %.9g expected %.9g, S=%u\n",l,s,x,y,actual,value,words[at+1]);correct=false;}
            const auto bits=format==2?u32(std::round(value*65535.f)):std::bit_cast<u32>(value);
            memcpy(expected.data()+GuestPixelOffset(target,dl,0,l,x,y,s),&bits,dl.bytes_per_pixel);
        }
        pixels.handle->Unmap(0,&empty);require(correct,"Color-channel native depth/sample/stencil oracle failed");
        auto actual_d=depth,actual_s=sbytes;native->Resolve(renderer.CompletedFence(),actual_d,actual_s);
        // D3D FLOAT->UNORM permits 0.6 integer ULP (functional spec 3.2.3.6).
        // Vulkan fundamentals Fixed-Point Data Conversions also permits either
        // adjacent integer. Check the independent mathematical value, not the
        // observed native output. In particular 0.5 may become 0x7fff OR 0x8000.
        // Keep exact byte checks for padding, S and the nonconverted inputs.
        // https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#FLOATtoUNORM
        if(format==2&&color_kind!=2)
        for(UINT l=0;l<target.layers;++l)for(UINT s=0;s<samples;++s)for(UINT y=0;y<target.height;++y)for(UINT x=0;x<target.width;++x){
            const auto at=GuestPixelOffset(target,dl,0,l,x,y,s);u16 actual{};memcpy(&actual,actual_d.data()+at,2);
            const auto* xy=sample_xy(s);const UINT sx=((16*x+xy[0])*source.width)/(16*target.width),sy=((16*y+xy[1])*source.height)/(16*target.height);
            const float ideal=(l?seed(l,s,x,y):sampled(sx,sy,s))*65535.f;
            require(std::abs(float(actual)-ideal)<=0.6001f,"D16 native write exceeds specified UNORM conversion bound");
            memcpy(actual_d.data()+at,expected.data()+at,2); // Compare untouched padding below, after the value assertion.
        }
        // sRGB has defined conversion accuracy rather than an exact bit oracle.
        if(color_kind!=2){
            if(actual_d!=expected){const auto i=size_t(std::mismatch(actual_d.begin(),actual_d.end(),expected.begin()).first-actual_d.begin());
                fprintf(stderr,"Guest mismatch byte=%zu actual=%u expected=%u\n",i,unsigned(actual_d[i]),unsigned(expected[i]));
                for(UINT l=0;l<target.layers;++l)for(UINT s=0;s<samples;++s)for(UINT y=0;y<target.height;++y)for(UINT x=0;x<target.width;++x){
                    const auto off=GuestPixelOffset(target,dl,0,l,x,y,s);if(i>=off&&i<off+dl.bytes_per_pixel){
                        u32 a{},e{};memcpy(&a,actual_d.data()+off,dl.bytes_per_pixel);memcpy(&e,expected.data()+off,dl.bytes_per_pixel);
                        const auto* xy=sample_xy(s);const UINT sx=((16*x+xy[0])*source.width)/(16*target.width),sy=((16*y+xy[1])*source.height)/(16*target.height);
                        fprintf(stderr,"Mismatch l=%u s=%u x=%u y=%u source=%u,%u native=%.9g source-value=%.9g words=%08x/%08x\n",l,s,x,y,sx,sy,native_values[((l*samples+s)*target.height+y)*target.width+x],sampled(sx,sy,s),a,e);
                    }
                }
            }
            require(actual_d==expected,"Channel guest readback lost logical pixels or padding");
        }
        else for(UINT l=0;l<target.layers;++l)for(UINT s=0;s<samples;++s)for(UINT y=0;y<target.height;++y)for(UINT x=0;x<target.width;++x){
            const auto at=GuestPixelOffset(target,dl,0,l,x,y,s);u32 a{};memcpy(&a,actual_d.data()+at,dl.bytes_per_pixel);
            const auto [low,high]=bounds(l,s,x,y);const float value=format==2?float(a)/65535.f:std::bit_cast<float>(a);
            const float tolerance=format==2?0.6001f/65535.f:0.0000002f;
            require(std::isfinite(value)&&value>=low-tolerance&&value<=high+tolerance,"sRGB typed depth conversion exceeds specified precision interval");
            memcpy(actual_d.data()+at,expected.data()+at,dl.bytes_per_pixel);
        }
        require(actual_d==expected&&actual_s==sbytes,"Channel transfer changed padding or stencil");
        range={0,SIZE_T(old_n+d_n+s_n)};Check(inputs.handle->Map(0,&range,&data));offset=0;bool intact=true;
        for(const auto* bytes:{&color,&depth,&sbytes})if(!bytes->empty()){
            const auto n=bytes==&color?old_n:bytes==&depth?d_n:s_n;
            for(u64 i=0;i<n;++i)intact&=static_cast<const std::byte*>(data)[offset+i]==(i<bytes->size()?(*bytes)[size_t(i)]:std::byte{0x6b});offset+=n;
        }
        inputs.handle->Unmap(0,&empty);require(intact,"Channel transfer mutated GPU inputs or native allocation padding");
        require(std::all_of(backing.physical,backing.physical+total,[](auto b){return b==std::byte{0x17};}),"Channel transfer published private I to B0");
        *reinterpret_cast<volatile unsigned char*>(backing.guest+(stencil?capacity:0))=0x96;
        require(faults==1&&!live&&!converted.gpu_owned,"Channel destination CPU write failed to retire owner");
        cache.SetIndependentVersionTransfer({},{},{});cache.SetIndependentImageTransfer({});cache.SetBufferResidencyPublisher({});renderer.SetCpuPageRelease({});
        cache.Collect(renderer.CompletedFence());require(converted.color_depth_leases.empty(),"Completed color draw lease was not released");
        renderer.ValidateDebugMessages();++cases;
    }
    require((pool_geometry||retain_extent)?cases==2:wide?cases>0:cases==(srgb?8u:16u),"Color-channel cases missing");
    printf("PASS: %u typed color-to-MS-depth cases; channels/defaults/sRGB, sample UVs, mip/layer-zero selection, unchanged S/B/padding, deferred receipts, CPU guards and real fence leases\n",cases);
}
} // namespace NativeDX12
