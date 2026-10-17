// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_image_ownership.h"
#include "video_core/renderer_d3d12/guest_image_readback.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include <bit>
#include <cstring>
namespace NativeDX12 {
inline void RunDirtyCachedDepthFixture(ID3D12Device* device,bool color_source=false,bool color_target=false,int late_change=0,bool writable_target=false,
    UINT color_format=0,UINT color_number=0,bool sampled_color_source=false){
    const auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
    unsigned executed{};
    for(const UINT format:{2u,4u})for(const UINT tile:{0u,8u})for(const bool modified:{false,true})for(const bool growth:{false,true}){
        if(color_format&&format!=((color_format==3||color_format>=16)?2u:4u))continue;
        if(late_change&&(color_source||color_target||growth))continue;
        printf("CASE: dirty cached %s fmt=%u tile=%u GpuModified=%u growth=%u\n",color_source?"color-to-depth":"depth",format,tile,unsigned(modified),unsigned(growth));fflush(stdout);
        GuestTextureDesc source;source.address=0x40000;source.stencil_address=0x80000;source.width=13;source.height=11;source.pitch=16;
        source.depth_target=true;source.data_format=format;source.number_format=format==2?0:7;source.tile_mode=tile;
        source.levels=1;source.layers=source.view_layers=growth?3:2;
        auto target=source;target.levels=growth?2:1;target.layers=growth?1:2;target.view_layers=1;
        if(color_source){source.depth_target=false;source.render_target=!sampled_color_source;source.number_format=4;}
        if(color_source&&color_format){source.data_format=color_format;source.number_format=color_number;}
        if(color_target||writable_target){
            // Original ColorBuffer requests one mip; RT expansion grows layers.
            source.layers=source.view_layers=growth?1:2;target.levels=1;target.layers=growth?3:2;
            target.depth_target=false;target.render_target=!writable_target;target.writable=writable_target;target.number_format=7;
            if(color_format){target.data_format=color_format;target.number_format=color_number;}
        }
        const bool uint_color=color_format&&color_number==4;
        const bool packed16=color_format>=16&&color_format<=19;
        const bool raw_packed=packed16&&writable_target;
        const auto written_color_word=[&](bool clear)->u32{
            if(!color_format)return format==2?(clear?0x3400u:0x3800u):std::bit_cast<u32>(clear?0.25f:0.5f);
            if(uint_color){const u32 r=clear?3:17,g=clear?5:31,b=clear?7:47,a=clear?2:1;
                switch(color_format){case 3:return r|(g<<8);case 5:return r|(g<<16);
                case 9:return r|(g<<10)|(b<<20)|(a<<30);case 10:return r|(g<<8)|(b<<16)|(a<<24);}}
            // Exact UNORM endpoints avoid imposing a cross-vendor tie-rounding
            // rule on newly authored draw commands; copied words stay arbitrary.
            switch(color_format){case 3:return clear?0:255;case 5:return clear?0:65535;
            case 6:return clear?0:0x3c0; // UF11 R=1, G/B=0, no alpha lane.
            case 16:return clear?0:0xf800;case 17:return clear?0x8000:0xfc00;
            case 18:return clear?1:0xf801;case 19:return clear?15:255;
            case 9:return 0xc0000000u|(clear?0:1023);case 10:return 0xff000000u|(clear?0:255);}
            throw std::runtime_error("Missing independent color write oracle");
        };
        const auto old_layout=LayoutGuestTexture(source),layout=LayoutGuestTexture(target);
        auto stencil_shape=target;stencil_shape.depth_target=true;stencil_shape.render_target=false;
        const auto sl=LayoutGuestTexture(DescribeGuestStencil(stencil_shape));
        stencil_shape.levels=source.levels;stencil_shape.layers=source.layers;stencil_shape.view_layers=source.layers;
        const auto old_s=LayoutGuestTexture(DescribeGuestStencil(stencil_shape));
        const u64 dc=(std::max(old_layout.guest_bytes,layout.guest_bytes)+65535)&~u64(65535);
        const u64 sc=(std::max(old_s.guest_bytes,sl.guest_bytes)+65535)&~u64(65535),capacity=dc+2*sc;
        struct Backing {HANDLE section{};std::byte *guest{},*physical{};
            ~Backing(){if(physical)UnmapViewOfFile(physical);if(guest)UnmapViewOfFile(guest);if(section)CloseHandle(section);}} backing;
        backing.section=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,DWORD(capacity>>32),DWORD(capacity),nullptr);
        require(backing.section!=nullptr,"Dirty recreation section failed");
        backing.guest=static_cast<std::byte*>(MapViewOfFile(backing.section,FILE_MAP_ALL_ACCESS,0,0,SIZE_T(capacity)));
        backing.physical=static_cast<std::byte*>(MapViewOfFile(backing.section,FILE_MAP_ALL_ACCESS,0,0,SIZE_T(capacity)));
        require(backing.guest&&backing.physical,"Dirty recreation aliases failed");memset(backing.physical,0x17,size_t(capacity));
        const u64 base=reinterpret_cast<u64>(backing.guest);source.address=target.address=base;
        source.stencil_address=growth&&!color_source?base+dc:0;target.stencil_address=(color_target||writable_target)?0:base+dc;
        const auto bits=[&](UINT phase,UINT m,UINT l,UINT x,UINT y){
            if(color_format==6){
                // Simultaneously legal finite D32 and exact finite UF11/UF10
                // channels; arbitrary copy words stay independent of the codec.
                const u32 r=((phase%13+1)<<6)|((x+phase*3)&63);
                const u32 g=(((l+m+phase)%13+1)<<6)|((y+phase*7)&63);
                return 0x3f000000u|r|(g<<11);
            }
            return format==2?
            1u+(phase*997u+m*79u+l*373u+x*11u+y*13u)%65530u:
            std::bit_cast<u32>(float(phase*2048u+m*512u+l*256u+y*7u+x+1u)/65536.f);};
        const auto sbits=[](UINT phase,UINT m,UINT l,UINT x,UINT y){return (phase*79u+m*11u+l*17u+y*7u+x*3u)%256u;};
        const auto native_word=[&](UINT m,UINT l,UINT x,UINT y){
            return late_change==1?(format==2?0xffffu:std::bit_cast<u32>(1.0f)):bits(1,m,l,x,y);
        };
        const auto data=[&](const auto& desc,const auto& physical,bool stencil,UINT phase,std::byte padding){
            auto stencil_shape=desc;stencil_shape.depth_target=true;stencil_shape.render_target=false;
            if(!stencil_shape.stencil_address)stencil_shape.stencil_address=base+dc;
            std::vector<std::byte> out(size_t(physical.guest_bytes),padding);const auto interpreted=stencil?DescribeGuestStencil(stencil_shape):desc;
            for(UINT m=0;m<desc.levels;++m)for(UINT l=0;l<desc.layers;++l)
            for(UINT y=0;y<std::max(desc.height>>m,1u);++y)for(UINT x=0;x<std::max(desc.width>>m,1u);++x){
                const u32 value=stencil?sbits(phase,m,l,x,y):bits(phase,m,l,x,y);
                memcpy(out.data()+GuestPixelOffset(interpreted,physical,m,l,x,y),&value,physical.bytes_per_pixel);}return out;};
        const auto old_d=data(source,old_layout,false,1,std::byte{0xa3}),old_stencil=data(source,old_s,true,2,std::byte{0xb4});
        const auto current_d=data(target,layout,false,5,std::byte{0xa9}),current_s=data(target,sl,true,9,std::byte{0xb7});
        const auto written_d=data(target,layout,false,11,std::byte{0xc8}),written_s=data(target,sl,true,13,std::byte{0xd9});
        if(!modified){memcpy(backing.physical,old_d.data(),old_d.size());memcpy(backing.physical+dc,old_stencil.data(),old_stencil.size());}
        GuestTextureCache cache(device);Rasterizer renderer(device);
        const auto gpu=[&](const auto& bytes){const u64 n=(bytes.size()+65535)&~u64(65535);
            auto up=Buffer(device,n,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
            auto out=Buffer(device,n,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
            void* mapped{};D3D12_RANGE empty{};Check(up.handle->Map(0,&empty,&mapped));memset(mapped,0x6b,size_t(n));memcpy(mapped,bytes.data(),bytes.size());up.handle->Unmap(0,nullptr);
            renderer.RecordingList()->CopyBufferRegion(out.handle.Get(),0,up.handle.Get(),0,n);out.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            renderer.RetainUntilFinish([up,out](u64){(void)up;(void)out;});return out;};
        auto old_gpu=gpu(old_d),old_s_gpu=gpu(old_stencil);
        auto& image=modified?cache.Upload(source,{},renderer.RecordingList(),renderer.CompletedFence(),{},true,&old_gpu,false,false,
            GuestTextureCache::UploadPurpose::Preparation,source.stencil_address?&old_s_gpu:nullptr):
            cache.Upload(source,old_d,renderer.RecordingList(),renderer.CompletedFence(),old_stencil,false,nullptr,false,false,GuestTextureCache::UploadPurpose::Preparation);
        require(!sampled_color_source||!(image.texture.handle->GetDesc().Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
            "Sampled packed source unexpectedly requires native render-target support");
        GuestTextureCache::Entry* live{};unsigned faults{};std::function<void(GuestTextureCache::Entry&)> retire;
        PageWatch watch([&](u64,bool writing){require(writing&&live,"Unexpected dirty recreation CPU fault");++faults;
            auto* old=std::exchange(live,nullptr);old->MarkLogicalDirty();retire(*old);});
        renderer.SetCpuPageRelease([&](u64 a,u64 n){return watch.UntrackReadOnly(a,n,true);});
        retire=[&](auto& old){ReleaseGuestImageOwnership(cache,old,[&](u64 a,u64 n,const auto& keep){renderer.ReleaseUnusedReadOnlyWatches(a,n,keep);});};
        cache.SetIndependentImageRetention([](const auto& d,const auto& l){return d.writable&&!d.depth_target&&!d.stencil_address&&!l.expanded_bytes_per_pixel;});
        cache.SetBufferAliasRetention([](const auto& d,const auto& l){return d.writable&&!d.depth_target&&!d.stencil_address&&!l.expanded_bytes_per_pixel;});
        const auto acquire=[&](auto& old){const bool ok=AcquireGuestImageOwnership(cache,old,watch,
            [&](u64 a,u64 n){return a>=base&&a-base<=capacity&&n<=capacity-(a-base);},
            [](u64,u64){return std::vector<std::pair<u64,u64>>{};});if(ok)live=&old;return ok;};
        cache.SetOwnershipAcquirer(acquire);cache.SetIndependentVersionTransfer({},acquire,retire);
        cache.SetOwnershipResolver([](auto&){throw std::runtime_error("Dirty cached native copy downloaded old I into B");});
        if(modified)require(cache.Own(image),"Rendered source ownership failed");else require(acquire(image),"CPU-initialized source watch failed");
        require(image.cached_native_current&&image.logical_gpu_modified==modified&&!image.logical_dirty,"Source initialized identity/history incorrect");
        const auto htile=base+capacity-4096;
        cache.RegisterMeta(image,color_source?GuestTextureCache::MetaType::CMask:GuestTextureCache::MetaType::HTile,htile,0xf);
        auto source_pin=cache.PinBinding(image);
        *reinterpret_cast<volatile unsigned char*>(backing.guest)=0x96;
        require(faults==1&&!live&&!image.gpu_owned&&!image.buffer_alias_version&&image.logical_dirty&&image.cached_native_current,
            "CPU dirty store revived source I or lost cached allocation identity");
        memcpy(backing.physical,current_d.data(),current_d.size());
        if(target.stencil_address||source.stencil_address)
            memcpy(backing.physical+dc,current_s.data(),current_s.size());
        const std::vector<std::byte> physical_before(backing.physical,backing.physical+capacity);
        auto sampled=source;sampled.depth_target=false;sampled.stencil_address=0;
        require(!cache.FindValidIndependentImage(sampled,LayoutGuestTexture(sampled))&&!cache.HasOwnedShaderStorage(sampled)&&
            !cache.HasCurrentDepthContents(image)&&!cache.RevalidateReadonly(image),"Dirty cached native source became current readonly I");
        auto requested=target;if(!growth){requested.levels=requested.layers=1;requested.view_layers=1;}
        const auto storage=cache.ResolveDepthViewStorage(requested);
        require(storage==target&&cache.FindCachedDepthRecreationSource(storage,layout)==&image,
            "Dirty native source lost normalized recreation extent");
        require(!cache.FindCachedDepthRecreationSource(source,old_layout),"Same-resource dirty refresh incorrectly selected old native copy");
        if(growth&&!color_source){auto rebased=source;rebased.stencil_address=base+dc+sc;
            require(!cache.FindCachedDepthRecreationSource(rebased,old_layout),
                "A stencil address change alone triggered original aspect recreation");}
        auto seed_d=gpu(current_d),seed_s=gpu(current_s);unsigned reads{};
        const auto clone=[&](Resource& input){auto out=GuestTiler::Scratch(device,input.handle->GetDesc().Width);
            input.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);out.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
            renderer.RecordingList()->CopyBufferRegion(out.handle.Get(),0,input.handle.Get(),0,input.handle->GetDesc().Width);
            input.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);out.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            renderer.RetainUntilFinish([out](u64){(void)out;});return out;};
        const auto capture=[&](u64 a,u64 n){++reads;if(a==target.address){require(n==layout.guest_bytes,"Dirty D seed truncated");return clone(seed_d);}
            require(a==target.stencil_address&&n==sl.guest_bytes,"Dirty S seed truncated");return clone(seed_s);};
        bool rejected=false;try{cache.CaptureIndependentImageVersion(image,target,layout,renderer.RecordingList(),renderer.CompletedFence(),capture);}
            catch(const std::invalid_argument&){rejected=true;}require(rejected&&!reads,"Ordinary independent capture accepted dirty native I");
        rejected=false;try{cache.CaptureDepthPlane(image,0,seed_d,renderer.RecordingList(),renderer.CompletedFence());}
            catch(const std::runtime_error&){rejected=true;}require(rejected,"Ordinary native plane snapshot accepted dirty I");
        rejected=false;try{cache.CaptureCachedDepthRecreationVersion(image,source,old_layout,renderer.RecordingList(),renderer.CompletedFence(),capture);}
            catch(const std::invalid_argument&){rejected=true;}require(rejected&&!reads,"Cached copy bypassed same-resource D refresh");
        auto invalid=target;invalid.pitch+=16;rejected=false;
        try{cache.CaptureCachedDepthRecreationVersion(image,invalid,LayoutGuestTexture(invalid),renderer.RecordingList(),renderer.CompletedFence(),capture);}
            catch(const std::invalid_argument&){rejected=true;}require(rejected&&!reads,"Cached copy accepted another physical class");
        cache.SetCachedDepthRecreationTransfer([&](auto& old,const auto& d,const auto& l){
            return cache.CaptureCachedDepthRecreationVersion(old,d,l,renderer.RecordingList(),renderer.CompletedFence(),capture);});
        cache.SetIndependentImageTransfer([&](auto& old,const auto& d,const auto& l){
            return cache.CaptureIndependentImageVersion(old,d,l,renderer.RecordingList(),renderer.CompletedFence(),capture);});
        // A cached-native fast path must not accept malformed ordinary Upload inputs.
        auto invalid_gpu=seed_d;invalid_gpu.state=D3D12_RESOURCE_STATE_COPY_DEST;
        rejected=false;
        try{cache.Upload(storage,current_d,renderer.RecordingList(),renderer.CompletedFence(),current_s,false,&invalid_gpu);}
            catch(const std::invalid_argument&){rejected=true;}
        require(rejected&&!reads&&image.logical_dirty&&image.cached_native_current,
            "Cached recreation bypassed GPU snapshot validation or consumed dirty source");
        rejected=false;
        try{cache.Upload(storage,{},renderer.RecordingList(),renderer.CompletedFence(),current_s);}
            catch(const std::invalid_argument&){rejected=true;}
        require(rejected&&!reads&&image.logical_dirty&&image.cached_native_current,
            "Cached recreation bypassed short CPU input validation or consumed dirty source");
        unsigned prewarms{};
        cache.SetImageBufferSources([&](u64 a,u64 n){
            require((a==storage.address&&n==layout.guest_bytes)||
                (storage.stencil_address&&a==storage.stencil_address&&n==sl.guest_bytes),"Private binding plan prepared another source range");
            ++prewarms;
        },[&](u64 a,u64 n){return GuestTextureCache::BufferImageSource{capture(a,n),late_change==2};});
        unsigned publications{};cache.SetBufferResidencyPublisher([&](auto){++publications;});
        cache.SetTransferSubmissionPublisher([&]{renderer.NotifyOnSubmission([&](u64 fence){cache.Submitted(fence);});});
        struct Gate {ComPtr<ID3D12Fence> fence;~Gate(){if(fence)fence->Signal(1);}} gate;
        Check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&gate.fence)));renderer.WaitExternalFence(gate.fence.Get(),1);
        if(color_target||writable_target){
            require(!cache.FindIndependentUploadSource(storage,layout)&&cache.GetMetaInfo(htile)->owner_uid==image.image_uid,
                "Physical RT prewarm changed old I validity or metadata before actual image binding");
            auto& converted=cache.Upload(storage,current_d,renderer.RecordingList(),renderer.CompletedFence());
            auto converted_pin=cache.PinBinding(converted);
            require(converted.gpu_owned&&!converted.logical_dirty&&converted.logical_gpu_modified==modified&&
                cache.HasCurrentNativeContents(converted)&&!cache.HasCurrentDepthContents(converted)&&
                !image.cached_native_current&&image.logical_dirty&&!image.gpu_owned&&reads==1&&!publications&&
                !cache.GetMetaInfo(htile),"Actual RT binding lost native copy/original flags or metadata retirement");
            auto expected=current_d;
            for(UINT m=0;m<std::min(source.levels,target.levels);++m)for(UINT l=0;l<std::min(source.layers,target.layers);++l)
            for(UINT y=0;y<std::max(target.height>>m,1u);++y)for(UINT x=0;x<std::max(target.width>>m,1u);++x){
                const u32 value=bits(1,m,l,x,y);memcpy(expected.data()+GuestPixelOffset(target,layout,m,l,x,y),&value,layout.bytes_per_pixel);}
            auto source_native=std::make_shared<GuestImageReadback>(device,image,renderer.RecordingList());
            auto before_native=std::make_shared<GuestImageReadback>(device,converted,renderer.RecordingList());
            renderer.RetainUntilFinish([source_native,before_native](u64){(void)source_native;(void)before_native;});
            auto* list=renderer.RecordingList();
            if(!writable_target){
            auto vs=CompileMultisampleShader("float4 main(uint id:SV_VertexID):SV_Position { return float4(id==2?3:-1,id==1?3:-1,0,1); }","vs_5_1");
            auto ps=CompileMultisampleShader(uint_color?"uint4 main():SV_Target { return uint4(17,31,47,1); }":
                color_format?"float4 main():SV_Target { return float4(1,0,0,1); }":
                "float4 main():SV_Target { return float4(0.5,0,0,1); }","ps_5_1");
            D3D12_ROOT_SIGNATURE_DESC rs{};rs.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
            ComPtr<ID3DBlob> signature,errors;ComPtr<ID3D12RootSignature> draw_root;
            Check(D3D12SerializeRootSignature(&rs,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&errors));
            Check(device->CreateRootSignature(0,signature->GetBufferPointer(),signature->GetBufferSize(),IID_PPV_ARGS(&draw_root)));
            D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};pd.pRootSignature=draw_root.Get();pd.VS={vs->GetBufferPointer(),vs->GetBufferSize()};pd.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
            pd.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;pd.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;pd.RasterizerState.DepthClipEnable=TRUE;
            auto& blend=pd.BlendState.RenderTarget[0];blend.SrcBlend=blend.SrcBlendAlpha=D3D12_BLEND_ONE;
            blend.DestBlend=blend.DestBlendAlpha=D3D12_BLEND_ZERO;blend.BlendOp=blend.BlendOpAlpha=D3D12_BLEND_OP_ADD;
            blend.LogicOp=D3D12_LOGIC_OP_NOOP;blend.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;pd.SampleMask=UINT_MAX;
            pd.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;
            pd.DepthStencilState.StencilReadMask=pd.DepthStencilState.StencilWriteMask=0xff;
            pd.DepthStencilState.FrontFace=pd.DepthStencilState.BackFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_ALWAYS};
            pd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;pd.NumRenderTargets=1;pd.RTVFormats[0]=layout.format;pd.SampleDesc.Count=1;
            ComPtr<ID3D12PipelineState> draw_pipeline;Check(device->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&draw_pipeline)));
            auto rtv=std::make_shared<Descriptors>(device,D3D12_DESCRIPTOR_HEAP_TYPE_RTV,1,false);
            auto selected=storage;selected.first_mip=selected.first_layer=0;selected.view_layers=1;const auto view=converted.Rtv(selected);
            device->CreateRenderTargetView(converted.texture.handle.Get(),&view,rtv->Cpu(0));
            renderer.RetainUntilFinish([draw_pipeline,draw_root,rtv](u64){(void)draw_pipeline;(void)draw_root;(void)rtv;});
            converted.texture.Transition(list,D3D12_RESOURCE_STATE_RENDER_TARGET);
            const auto handle=rtv->Cpu(0);list->OMSetRenderTargets(1,&handle,FALSE,nullptr);list->SetGraphicsRootSignature(draw_root.Get());list->SetPipelineState(draw_pipeline.Get());
            const D3D12_VIEWPORT viewport{0,0,float(target.width),float(target.height),0,1};const D3D12_RECT scissor{1,2,4,5};
            list->RSSetViewports(1,&viewport);list->RSSetScissorRects(1,&scissor);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);list->DrawInstanced(3,1,0,0);
            converted.NoteLogicalImageWrite();require(converted.logical_gpu_modified&&cache.HasCurrentNativeContents(converted),"Actual RT draw did not update GpuModified0 history");
            // A clipped CMASK-sized tile clear must not refresh the rest from B.
            const float clear[]{uint_color?3.f:color_format?0.f:0.25f,uint_color?5.f:0.f,uint_color?7.f:0.f,uint_color?2.f:1.f};
            const D3D12_RECT rectangle{8,0,LONG(target.width),8};
            list->ClearRenderTargetView(handle,clear,1,&rectangle);converted.NoteLogicalImageWrite();
            }else{
                // A real clipped compute writer, not a clear or CPU substitute.
                const auto shader=std::string(raw_packed?"RWTexture2DArray<uint> output:register(u0);\n":uint_color?"RWTexture2DArray<uint4> output:register(u0);\n":"RWTexture2DArray<float4> output:register(u0);\n")+
                    "[numthreads(8,8,1)] void main(uint3 p:SV_DispatchThreadID){if(p.x<"+
                    std::to_string(target.width)+"&&p.y<"+std::to_string(target.height)+
                    (raw_packed?"){if(p.x>=8&&p.y<8)output[p]="+std::to_string(written_color_word(true))+"u;else if(p.x>=1&&p.x<4&&p.y>=2&&p.y<5)output[p]="+std::to_string(written_color_word(false))+"u;}}":
                    uint_color?"){if(p.x>=8&&p.y<8)output[p]=uint4(3,5,7,2);else if(p.x>=1&&p.x<4&&p.y>=2&&p.y<5)output[p]=uint4(17,31,47,1);}}":
                    color_format?"){if(p.x>=8&&p.y<8)output[p]=float4(0,0,0,1);else if(p.x>=1&&p.x<4&&p.y>=2&&p.y<5)output[p]=float4(1,0,0,1);}}":
                    "){if(p.x>=8&&p.y<8)output[p]=float4(0.25,0,0,1);else if(p.x>=1&&p.x<4&&p.y>=2&&p.y<5)output[p]=float4(0.5,0,0,1);}}");
                auto code=CompileMultisampleShader(shader.c_str(),"cs_5_1");
                D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,0};
                D3D12_ROOT_PARAMETER parameter{};parameter.ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                parameter.DescriptorTable={1,&range};D3D12_ROOT_SIGNATURE_DESC description{};description.NumParameters=1;description.pParameters=&parameter;
                ComPtr<ID3DBlob> signature,errors;ComPtr<ID3D12RootSignature> root;
                Check(D3D12SerializeRootSignature(&description,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&errors));
                Check(device->CreateRootSignature(0,signature->GetBufferPointer(),signature->GetBufferSize(),IID_PPV_ARGS(&root)));
                D3D12_COMPUTE_PIPELINE_STATE_DESC state{};state.pRootSignature=root.Get();state.CS={code->GetBufferPointer(),code->GetBufferSize()};
                ComPtr<ID3D12PipelineState> pipeline;Check(device->CreateComputePipelineState(&state,IID_PPV_ARGS(&pipeline)));
                auto descriptors=std::make_shared<Descriptors>(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,1,true);
                const auto slot=descriptors->Allocate(0);const auto uav=converted.Uav(storage,0);
                device->CreateUnorderedAccessView(converted.texture.handle.Get(),nullptr,&uav,descriptors->Cpu(slot));
                renderer.RetainUntilFinish([root,pipeline,descriptors](u64){(void)root;(void)pipeline;(void)descriptors;});
                converted.texture.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                ID3D12DescriptorHeap* heaps[]{descriptors->Heap()};list->SetDescriptorHeaps(1,heaps);
                list->SetComputeRootSignature(root.Get());list->SetPipelineState(pipeline.Get());list->SetComputeRootDescriptorTable(0,descriptors->Gpu(slot));
                list->Dispatch((target.width+7)/8,(target.height+7)/8,1);
                D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;barrier.UAV.pResource=converted.texture.handle.Get();list->ResourceBarrier(1,&barrier);
                converted.NoteLogicalImageWrite();require(converted.logical_gpu_modified&&cache.HasCurrentNativeContents(converted),"Actual storage dispatch lost GpuModified0 history");
            }
            for(UINT y=0;y<target.height;++y)for(UINT x=0;x<target.width;++x){
                const bool drawn=x>=1&&x<4&&y>=2&&y<5,cleared=x>=8&&y<8;
                if(drawn||cleared){const u32 value=written_color_word(cleared);
                    memcpy(expected.data()+GuestPixelOffset(target,layout,0,0,x,y),&value,layout.bytes_per_pixel);}}
            auto after_native=std::make_shared<GuestImageReadback>(device,converted,list);renderer.RetainUntilFinish([after_native](u64){(void)after_native;});
            auto shader=std::string(format==2?"#define WORD16 1\n":"#define WORD16 0\n")+R"(
Texture2DArray<float4> input:register(t0);RWByteAddressBuffer output:register(u0);
cbuffer Shape:register(b0){uint width,height,layers,base;};
[numthreads(8,8,1)]void main(uint3 p:SV_DispatchThreadID){if(p.x>=width||p.y>=height||p.z>=layers)return;
float f=input.Load(int4(p,0)).x;
#if WORD16
uint word=f32tof16(f);
#else
uint word=asuint(f);
#endif
output.Store(4*(base+(p.z*height+p.y)*width+p.x),word);})";
            if(packed16){
                const auto declarations=raw_packed?"Texture2DArray<uint> input:register(t0);":"Texture2DArray<float4> input:register(t0);";
                std::string pack;
                if(raw_packed)pack="uint word=input.Load(int4(p,0))&65535u;";
                else if(color_format==16)pack="uint3 c=(uint3)round(input.Load(int4(p,0)).xyz*float3(31,63,31));uint word=(c.x<<11)|(c.y<<5)|c.z;";
                else if(color_format<=18){pack="uint4 c=(uint4)round(input.Load(int4(p,0))*float4(31,31,31,1));uint word=(c.w<<15)|(c.x<<10)|(c.y<<5)|c.z;";
                    if(color_format==18)pack+="word=((word<<1)|(word>>15))&65535u;";}
                else pack="uint4 c=(uint4)round(input.Load(int4(p,0))*15.0);uint word=(c.z<<12)|(c.y<<8)|(c.x<<4)|c.w;";
                shader=std::string(declarations)+"RWByteAddressBuffer output:register(u0);cbuffer Shape:register(b0){uint width,height,layers,base;};"
                    "[numthreads(8,8,1)]void main(uint3 p:SV_DispatchThreadID){if(p.x>=width||p.y>=height||p.z>=layers)return;"+
                    pack+"output.Store(4*(base+(p.z*height+p.y)*width+p.x),word);}";
            }else if(color_format==6){
                shader=R"(Texture2DArray<float4> input:register(t0);RWByteAddressBuffer output:register(u0);
cbuffer Shape:register(b0){uint width,height,layers,base;};
[numthreads(8,8,1)]void main(uint3 p:SV_DispatchThreadID){if(p.x>=width||p.y>=height||p.z>=layers)return;
float3 c=input.Load(int4(p,0)).xyz;
// Every tested UF11/10 input is exactly representable as F16; no assumed
// rounding of arbitrary F32, NaN payload or denormal is part of this oracle.
uint word=(f32tof16(c.x)>>4)|((f32tof16(c.y)>>4)<<11)|((f32tof16(c.z)>>5)<<22);
output.Store(4*(base+(p.z*height+p.y)*width+p.x),word);})";
            }else if(color_format){
                // Independent typed-channel packing, separate from guest retile.
                const auto convert=uint_color?"uint4 c=input.Load(int4(p,0));\n":
                    color_format==5?"uint4 c=(uint4)round(input.Load(int4(p,0))*65535.0);\n":
                    color_format==9?"uint4 c=(uint4)round(input.Load(int4(p,0))*float4(1023,1023,1023,3));\n":
                    "uint4 c=(uint4)round(input.Load(int4(p,0))*255.0);\n";
                const auto packed=color_format==3?"c.x|(c.y<<8)":color_format==5?"c.x|(c.y<<16)":
                    color_format==9?"c.x|(c.y<<10)|(c.z<<20)|(c.w<<30)":"c.x|(c.y<<8)|(c.z<<16)|(c.w<<24)";
                shader=std::string(uint_color?"Texture2DArray<uint4> input:register(t0);":"Texture2DArray<float4> input:register(t0);")+
                    "RWByteAddressBuffer output:register(u0);cbuffer Shape:register(b0){uint width,height,layers,base;};"
                    "[numthreads(8,8,1)]void main(uint3 p:SV_DispatchThreadID){if(p.x>=width||p.y>=height||p.z>=layers)return;"+
                    convert+"output.Store(4*(base+(p.z*height+p.y)*width+p.x),"+packed+");}";
            }
            auto cs=CompileMultisampleShader(shader.c_str(),"cs_5_1");
            D3D12_DESCRIPTOR_RANGE ranges[2]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,1}};
            D3D12_ROOT_PARAMETER parameters[2]{};parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[0].DescriptorTable={2,ranges};parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;parameters[1].Constants={0,0,4};
            D3D12_ROOT_SIGNATURE_DESC cr{2,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};ComPtr<ID3D12RootSignature> root;
            ComPtr<ID3DBlob> signature,errors;
            Check(D3D12SerializeRootSignature(&cr,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&errors));Check(device->CreateRootSignature(0,signature->GetBufferPointer(),signature->GetBufferSize(),IID_PPV_ARGS(&root)));
            D3D12_COMPUTE_PIPELINE_STATE_DESC cp{};cp.pRootSignature=root.Get();cp.CS={cs->GetBufferPointer(),cs->GetBufferSize()};ComPtr<ID3D12PipelineState> pipeline;Check(device->CreateComputePipelineState(&cp,IID_PPV_ARGS(&pipeline)));
            std::vector<u32> pixels;
            for(UINT m=0;m<target.levels;++m)for(UINT l=0;l<target.layers;++l)for(UINT y=0;y<std::max(target.height>>m,1u);++y)for(UINT x=0;x<std::max(target.width>>m,1u);++x){
                const bool drawn=!m&&!l&&x>=1&&x<4&&y>=2&&y<5,cleared=!m&&!l&&x>=8&&y<8;
                pixels.push_back(drawn||cleared?written_color_word(cleared):bits(m<source.levels&&l<source.layers?1:5,m,l,x,y));}
            const u64 bytes=pixels.size()*4;auto output=GuestTiler::Scratch(device,bytes),typed=Buffer(device,bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            renderer.RetainUntilFinish([output,typed,root,pipeline](u64){(void)output;(void)typed;(void)root;(void)pipeline;});
            converted.texture.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);u32 base_word{};
            for(UINT m=0;m<target.levels;++m){auto descriptors=std::make_shared<Descriptors>(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,2,true);renderer.RetainUntilFinish([descriptors](u64){(void)descriptors;});
                D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=layout.format;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DARRAY;srv.Texture2DArray={m,1,0,target.layers,0,0};device->CreateShaderResourceView(converted.texture.handle.Get(),&srv,descriptors->Cpu(0));
                D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.Format=DXGI_FORMAT_R32_TYPELESS;uav.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;uav.Buffer.NumElements=UINT(bytes/4);uav.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;device->CreateUnorderedAccessView(output.handle.Get(),nullptr,&uav,descriptors->Cpu(1));
                ID3D12DescriptorHeap* heaps[]{descriptors->Heap()};list->SetDescriptorHeaps(1,heaps);list->SetComputeRootSignature(root.Get());list->SetPipelineState(pipeline.Get());list->SetComputeRootDescriptorTable(0,descriptors->Gpu(0));
                const UINT shape[]{std::max(target.width>>m,1u),std::max(target.height>>m,1u),target.layers,base_word};list->SetComputeRoot32BitConstants(1,4,shape,0);list->Dispatch((shape[0]+7)/8,(shape[1]+7)/8,target.layers);output.UavBarrier(list);base_word+=shape[0]*shape[1]*target.layers;}
            output.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);list->CopyBufferRegion(typed.handle.Get(),0,output.handle.Get(),0,bytes);
            struct Segment {Resource* resource;const std::vector<std::byte>* expected;u64 offset;};
            std::array<Segment,4> segments{{{&old_gpu,&old_d,0},{&old_s_gpu,&old_stencil,0},{&seed_d,&current_d,0},{&seed_s,&current_s,0}}};
            u64 total{};for(auto& part:segments){part.offset=total;total+=part.resource->handle->GetDesc().Width;}
            auto buffers=Buffer(device,total,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);renderer.RetainUntilFinish([buffers](u64){(void)buffers;});
            for(auto& part:segments){part.resource->Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);list->CopyBufferRegion(buffers.handle.Get(),part.offset,part.resource->handle.Get(),0,part.resource->handle->GetDesc().Width);}
            const auto fence=renderer.Flush();cache.Submitted(fence);source_native->Submitted(fence);before_native->Submitted(fence);after_native->Submitted(fence);cache.Collect(renderer.CompletedFence());
            require(renderer.CompletedFence()<fence&&!image.buffer_sync_resources.empty(),"RT conversion source retired before held fence");Check(gate.fence->Signal(1));renderer.Finish();
            auto actual_source=old_d,actual_source_s=old_stencil;source_native->Resolve(renderer.CompletedFence(),actual_source,actual_source_s);
            auto actual_before=current_d;before_native->Resolve(renderer.CompletedFence(),actual_before);
            auto before_expected=current_d;
            for(UINT m=0;m<std::min(source.levels,target.levels);++m)for(UINT l=0;l<std::min(source.layers,target.layers);++l)for(UINT y=0;y<std::max(target.height>>m,1u);++y)for(UINT x=0;x<std::max(target.width>>m,1u);++x){const u32 value=bits(1,m,l,x,y);memcpy(before_expected.data()+GuestPixelOffset(target,layout,m,l,x,y),&value,layout.bytes_per_pixel);}
            auto actual=current_d;after_native->Resolve(renderer.CompletedFence(),actual);
            const auto diagnose=[](const char* name,const auto& a,const auto& b){
                for(size_t i=0;i<a.size();++i)if(a[i]!=b[i]){printf("MISMATCH %s byte=%zu actual=%02x expected=%02x\n",name,i,
                    unsigned(a[i]),unsigned(b[i]));break;}
            };
            diagnose("source D",actual_source,old_d);diagnose("source S",actual_source_s,old_stencil);
            diagnose("target before",actual_before,before_expected);diagnose("target after",actual,expected);
            require(actual_source==old_d&&actual_source_s==old_stencil&&actual_before==before_expected&&actual==expected,
                "Native depth->RT copy/draw/clipped clear lost old words, target-only seed, source S or padding");
            void* mapped{};D3D12_RANGE range{0,SIZE_T(bytes)},empty{};Check(typed.handle->Map(0,&range,&mapped));const bool typed_ok=!memcmp(mapped,pixels.data(),size_t(bytes));typed.handle->Unmap(0,&empty);require(typed_ok,"Independent typed RT pixel oracle failed");
            range={0,SIZE_T(total)};Check(buffers.handle->Map(0,&range,&mapped));bool intact=true;for(const auto& part:segments)for(u64 i=0;i<part.resource->handle->GetDesc().Width;++i)intact&=static_cast<const std::byte*>(mapped)[part.offset+i]==(i<part.expected->size()?(*part.expected)[size_t(i)]:std::byte{0x6b});buffers.handle->Unmap(0,&empty);require(intact,"RT conversion changed physical GPU B/input capacity");
            require(!memcmp(backing.physical,physical_before.data(),size_t(capacity)),"Native RT copy/draw/clear published private I into physical B");
            cache.SetImageBufferSources({},{});cache.SetIndependentImageTransfer({});cache.SetCachedDepthRecreationTransfer({});cache.SetIndependentVersionTransfer({},{},{});cache.SetOwnershipAcquirer({});cache.SetOwnershipResolver({});cache.SetTransferSubmissionPublisher({});cache.SetBufferResidencyPublisher({});renderer.SetCpuPageRelease({});
            cache.Collect(renderer.CompletedFence());renderer.ValidateDebugMessages();++executed;continue;
        }
        const auto entries_before_plan=cache.Entries().size();
        auto binding_plan=cache.PlanCachedImageBinding(storage,layout);
        require(binding_plan&&prewarms==2&&!reads&&!publications&&cache.Entries().size()==entries_before_plan&&
            image.cached_native_current&&image.logical_dirty&&!image.gpu_owned&&
            cache.GetMetaInfo(htile)&&cache.GetMetaInfo(htile)->owner_uid==image.image_uid&&
            cache.GetMetaInfo(htile)->clear_mask==0xf&&!cache.FindIndependentUploadSource(storage,layout),
            "Private depth plan published future I/class/ownership/HTILE or captured pixels before binding");
        if(late_change==1){
            auto descriptors=std::make_shared<Descriptors>(device,D3D12_DESCRIPTOR_HEAP_TYPE_DSV,1,false);
            const auto dsv=image.Dsv(source,false,false);device->CreateDepthStencilView(image.texture.handle.Get(),&dsv,descriptors->Cpu(0));
            renderer.RetainUntilFinish([descriptors](u64){(void)descriptors;});
            image.texture.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_DEPTH_WRITE);
            renderer.RecordingList()->ClearDepthStencilView(descriptors->Cpu(0),D3D12_CLEAR_FLAG_DEPTH,1.0f,0,0,nullptr);
            require(cache.Own(image)&&image.HasValidLogicalImage(),"Late native writer did not become the actual source");
        }else if(late_change==2){
            // An earlier image bind creates this class, then a GPU B writer
            // dirties it. The real attachment bind must refresh D from B7,
            // preserve native S9 rather than B15 and retain consumed HTILE.
            auto& preceding=cache.Upload(storage,current_d,renderer.RecordingList(),renderer.CompletedFence(),current_s);
            cache.RegisterMeta(preceding,GuestTextureCache::MetaType::HTile,htile,preceding.htile_initial_mask);
            require(cache.ClearMeta(htile)&&cache.ConsumeMeta(htile,0)&&cache.ConsumeMeta(htile,1),"Late parent HTILE setup failed");
            auto changed_d=gpu(data(target,layout,false,7,std::byte{0xa9}));
            auto changed_s=gpu(data(target,sl,true,15,std::byte{0xb7}));
            for(auto pair:{std::pair{&seed_d,&changed_d},std::pair{&seed_s,&changed_s}}){
                pair.first->Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
                pair.second->Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
                renderer.RecordingList()->CopyBufferRegion(pair.first->handle.Get(),0,pair.second->handle.Get(),0,pair.first->handle->GetDesc().Width);
                pair.first->Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            }
            preceding.MarkLogicalDirty();retire(preceding);live=nullptr;
            require(cache.GetMetaInfo(htile)&&cache.GetMetaInfo(htile)->clear_mask==0xfffffffcu,
                "GPU B write changed existing parent metadata before actual binding");
        }
        auto* recreated=&cache.BindPlannedImage(*binding_plan,renderer.RecordingList(),renderer.CompletedFence());
        if(late_change)printf("STATE: late=%d target_owned=%u dirty=%u modified=%u current=%u old_owned=%u old_dirty=%u old_class=%u htile=%x reads=%u publishes=%u receipts=%zu\n",
            late_change,unsigned(recreated->gpu_owned),unsigned(recreated->logical_dirty),unsigned(recreated->logical_gpu_modified),
            unsigned(cache.HasCurrentDepthContents(*recreated)),unsigned(image.gpu_owned),unsigned(image.logical_dirty),
            unsigned(image.cached_native_current),recreated->htile_initial_mask,reads,publications,recreated->pending_buffer_residency.size());
        require(recreated&&recreated!=&image&&recreated->gpu_owned&&!recreated->logical_dirty&&
            recreated->logical_gpu_modified==(late_change==1||modified)&&cache.HasCurrentDepthContents(*recreated)&&
            !image.gpu_owned&&image.logical_dirty&&!image.cached_native_current&&recreated->htile_initial_mask==0&&
            reads==unsigned(late_change==2?3:2)&&publications==unsigned(late_change==2)&&recreated->pending_buffer_residency.empty(),"Cached copy lost native validity/original history/HTILE or published seed residency");
        require(late_change==2?(cache.GetMetaInfo(htile)&&cache.GetMetaInfo(htile)->owner_uid==recreated->image_uid&&
            cache.GetMetaInfo(htile)->clear_mask==0xfffffffcu):!cache.GetMetaInfo(htile),
            "Actual depth binding lost refresh metadata or retained freed recreation metadata");
        cache.RegisterMeta(*recreated,GuestTextureCache::MetaType::HTile,htile,recreated->htile_initial_mask);
        const auto fresh_meta=cache.GetMetaInfo(htile);
        require(fresh_meta&&fresh_meta->owner_uid==recreated->image_uid&&fresh_meta->clear_mask==(late_change==2?0xfffffffcu:0)&&!cache.ConsumeMeta(htile,0),
            "Recreated HTILE inherited a stale initial clear instead of preserving native copy contents");
        auto target_pin=cache.PinBinding(*recreated);
        const auto reads_after_bind=reads,publications_after_bind=publications;
        require(&cache.BindPlannedImage(*binding_plan,renderer.RecordingList(),renderer.CompletedFence())==recreated&&
            reads==reads_after_bind&&publications==publications_after_bind,"Repeated actual plan binding copied or published twice");
        require(&cache.Upload(storage,current_d,renderer.RecordingList(),renderer.CompletedFence(),current_s)==recreated,
            "Actual binding replaced prepared native recreation with current CPU B");
        auto prewrite_seed_d=clone(seed_d),prewrite_seed_s=clone(seed_s);
        auto prewrite_d=cache.CaptureDepthPlane(*recreated,0,prewrite_seed_d,renderer.RecordingList(),renderer.CompletedFence());
        auto prewrite_s=cache.CaptureDepthPlane(*recreated,1,prewrite_seed_s,renderer.RecordingList(),renderer.CompletedFence());
        // Later GPU writes mutate B, not the previously captured native I input.
        auto write_d=gpu(written_d),write_s=gpu(written_s);
        for(auto pair:{std::pair{&seed_d,&write_d},std::pair{&seed_s,&write_s}}){
            pair.first->Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);pair.second->Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            renderer.RecordingList()->CopyBufferRegion(pair.first->handle.Get(),0,pair.second->handle.Get(),0,pair.first->handle->GetDesc().Width);
            pair.first->Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);}
        recreated->MarkLogicalDirty();retire(*recreated);live=nullptr;
        require(!cache.HasCurrentDepthContents(*recreated)&&!cache.RevalidateReadonly(*recreated),"Later B write revived prepared current I");
        auto& bound=cache.Upload(storage,{},renderer.RecordingList(),renderer.CompletedFence(),{},true,&prewrite_d,false,false,
            GuestTextureCache::UploadPurpose::Preparation,&prewrite_s);
        require(bound.texture.handle!=recreated->texture.handle&&bound.logical_dirty&&!bound.cached_native_current,
            "Private prewrite snapshot replaced current cached-native identity");
        auto other=target;++other.levels;
        require(cache.FindCachedDepthRecreationSource(other,LayoutGuestTexture(other))==recreated,
            "Private snapshot displaced the actual cached native recreation source");
        const auto before_private_read=reads;rejected=false;
        try{cache.CaptureCachedDepthRecreationVersion(bound,other,LayoutGuestTexture(other),renderer.RecordingList(),renderer.CompletedFence(),capture);}
            catch(const std::invalid_argument&){rejected=true;}
        require(rejected&&reads==before_private_read,"Initialized private snapshot was admitted as cached native I");
        auto expected=late_change==2?data(target,layout,false,7,std::byte{0xa9}):current_d;
        for(UINT m=0;late_change!=2&&m<std::min(source.levels,target.levels);++m)for(UINT l=0;l<std::min(source.layers,target.layers);++l)
        for(UINT y=0;y<std::max(target.height>>m,1u);++y)for(UINT x=0;x<std::max(target.width>>m,1u);++x){
            const u32 value=native_word(m,l,x,y);memcpy(expected.data()+GuestPixelOffset(target,layout,m,l,x,y),&value,layout.bytes_per_pixel);}
        auto source_readback=std::make_shared<GuestImageReadback>(device,image,renderer.RecordingList());
        auto target_readback=std::make_shared<GuestImageReadback>(device,bound,renderer.RecordingList());
        renderer.RetainUntilFinish([source_readback,target_readback](u64){(void)source_readback;(void)target_readback;});
        // Independently typed native target pixels, without guest retile as oracle.
        const std::string shader=std::string(format==2?"#define D16 1\n":"#define D16 0\n")+R"(
Texture2DArray<float4> depth:register(t0);Texture2DArray<uint4> stencil:register(t1);
RWByteAddressBuffer output:register(u0);cbuffer Shape:register(b0){uint width,height,layers,base;};
[numthreads(8,8,1)]void main(uint3 p:SV_DispatchThreadID){if(p.x>=width||p.y>=height||p.z>=layers)return;
float d=depth.Load(int4(p,0)).x;uint s=stencil.Load(int4(p,0)).x;uint at=4*(base+2*((p.z*height+p.y)*width+p.x));
#if D16
output.Store(at,uint(round(saturate(d)*65535.0)));
#else
output.Store(at,asuint(d));
#endif
output.Store(at+4,s);})";
        auto code=CompileMultisampleShader(shader.c_str(),"cs_5_1");
        D3D12_DESCRIPTOR_RANGE ranges[2]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,2,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,2}};
        D3D12_ROOT_PARAMETER parameters[2]{};parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[0].DescriptorTable={2,ranges};
        parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;parameters[1].Constants={0,0,4};
        D3D12_ROOT_SIGNATURE_DESC rd{2,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};ComPtr<ID3DBlob> signature,errors;ComPtr<ID3D12RootSignature> root;
        Check(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&errors));Check(device->CreateRootSignature(0,signature->GetBufferPointer(),signature->GetBufferSize(),IID_PPV_ARGS(&root)));
        D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.CS={code->GetBufferPointer(),code->GetBufferSize()};ComPtr<ID3D12PipelineState> pipeline;Check(device->CreateComputePipelineState(&pd,IID_PPV_ARGS(&pipeline)));
        std::vector<u32> pixels;
        for(UINT m=0;m<target.levels;++m)for(UINT l=0;l<target.layers;++l)
        for(UINT y=0;y<std::max(target.height>>m,1u);++y)for(UINT x=0;x<std::max(target.width>>m,1u);++x){
            pixels.push_back(late_change==2?bits(7,m,l,x,y):(m<source.levels&&l<source.layers?native_word(m,l,x,y):bits(5,m,l,x,y)));pixels.push_back(sbits(9,m,l,x,y));}
        const u64 pixel_bytes=pixels.size()*4;auto output=GuestTiler::Scratch(device,pixel_bytes),pixel_readback=Buffer(device,pixel_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        renderer.RetainUntilFinish([output,pixel_readback,root,pipeline](u64){(void)output;(void)pixel_readback;(void)root;(void)pipeline;});
        bound.texture.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);u32 base_word{};
        for(UINT m=0;m<target.levels;++m){auto views=std::make_shared<Descriptors>(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,3,true);
            renderer.RetainUntilFinish([views](u64){(void)views;});
            for(UINT plane=0;plane<2;++plane){D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=plane?DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
                srv.Shader4ComponentMapping=plane?D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(1,1,1,1):D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DARRAY;srv.Texture2DArray={m,1,0,target.layers,plane,0};device->CreateShaderResourceView(bound.texture.handle.Get(),&srv,views->Cpu(plane));}
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.Format=DXGI_FORMAT_R32_TYPELESS;uav.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;uav.Buffer.NumElements=UINT(pixel_bytes/4);uav.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
            device->CreateUnorderedAccessView(output.handle.Get(),nullptr,&uav,views->Cpu(2));ID3D12DescriptorHeap* heaps[]{views->Heap()};auto* list=renderer.RecordingList();list->SetDescriptorHeaps(1,heaps);list->SetComputeRootSignature(root.Get());list->SetPipelineState(pipeline.Get());list->SetComputeRootDescriptorTable(0,views->Gpu(0));
            const UINT shape[]{std::max(target.width>>m,1u),std::max(target.height>>m,1u),target.layers,base_word};list->SetComputeRoot32BitConstants(1,4,shape,0);list->Dispatch((shape[0]+7)/8,(shape[1]+7)/8,target.layers);output.UavBarrier(list);base_word+=2*shape[0]*shape[1]*target.layers;}
        output.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);renderer.RecordingList()->CopyBufferRegion(pixel_readback.handle.Get(),0,output.handle.Get(),0,pixel_bytes);
        struct Segment {Resource* input;const std::vector<std::byte>* expected;u64 offset;};
        std::array<Segment,6> segments{{{&old_gpu,&old_d,0},{&old_s_gpu,&old_stencil,0},{&seed_d,&written_d,0},{&seed_s,&written_s,0},{&prewrite_d,&expected,0},{&prewrite_s,&current_s,0}}};
        u64 total{};for(auto& part:segments){part.offset=total;total+=part.input->handle->GetDesc().Width;}
        auto buffer_readback=Buffer(device,total,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);renderer.RetainUntilFinish([buffer_readback](u64){(void)buffer_readback;});
        for(auto& part:segments){part.input->Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);renderer.RecordingList()->CopyBufferRegion(buffer_readback.handle.Get(),part.offset,part.input->handle.Get(),0,part.input->handle->GetDesc().Width);}
        const auto fence=renderer.Flush();cache.Submitted(fence);source_readback->Submitted(fence);target_readback->Submitted(fence);cache.Collect(renderer.CompletedFence());
        require(renderer.CompletedFence()<fence&&!image.buffer_sync_resources.empty(),"Cached source leases retired before held GPU fence");
        Check(gate.fence->Signal(1));renderer.Finish();
        auto actual_old=old_d,actual_old_s=old_stencil;source_readback->Resolve(renderer.CompletedFence(),actual_old,actual_old_s);
        auto actual=current_d,actual_s=current_s;target_readback->Resolve(renderer.CompletedFence(),actual,actual_s);
        auto expected_old=old_d;
        if(late_change==1)for(UINT m=0;m<source.levels;++m)for(UINT l=0;l<source.layers;++l)
        for(UINT y=0;y<std::max(source.height>>m,1u);++y)for(UINT x=0;x<std::max(source.width>>m,1u);++x){
            const u32 value=native_word(m,l,x,y);memcpy(expected_old.data()+GuestPixelOffset(source,old_layout,m,l,x,y),&value,old_layout.bytes_per_pixel);}
        const auto same=[&](const char* name,const auto& a,const auto& b){
            if(a==b)return true;
            const auto at=size_t(std::mismatch(a.begin(),a.end(),b.begin(),b.end()).first-a.begin());
            const auto base=at&~size_t(3);u32 av{},bv{};
            if(base<a.size())memcpy(&av,a.data()+base,std::min(size_t(4),a.size()-base));
            if(base<b.size())memcpy(&bv,b.data()+base,std::min(size_t(4),b.size()-base));
            printf("MISMATCH: %s at=%zu actual_word=%08x expected_word=%08x\n",name,at,av,bv);return false;
        };
        const bool source_d_ok=same("source-D",actual_old,expected_old),source_s_ok=same("source-S",actual_old_s,old_stencil);
        const bool target_d_ok=same("target-D",actual,expected),target_s_ok=same("target-S",actual_s,current_s);
        require(source_d_ok&&source_s_ok&&target_d_ok&&target_s_ok,"Dirty cached recreation/prewrite lost old native D, new B tail/S seed, source or padding");
        void* mapped{};D3D12_RANGE range{0,SIZE_T(pixel_bytes)},empty{};Check(pixel_readback.handle->Map(0,&range,&mapped));const bool typed=!memcmp(mapped,pixels.data(),size_t(pixel_bytes));pixel_readback.handle->Unmap(0,&empty);require(typed,"Dirty cached native typed pixel oracle failed");
        range={0,SIZE_T(total)};Check(buffer_readback.handle->Map(0,&range,&mapped));bool intact=true;
        for(const auto& part:segments)for(u64 i=0;i<part.input->handle->GetDesc().Width;++i)intact&=static_cast<const std::byte*>(mapped)[part.offset+i]==(i<part.expected->size()?(*part.expected)[size_t(i)]:std::byte{0x6b});
        buffer_readback.handle->Unmap(0,&empty);require(intact,"Dirty cached source/B mutation/prewrite buffer capacity oracle failed");
        require(!memcmp(backing.physical,physical_before.data(),size_t(capacity)),"Cached native recreation or later B write published private I into physical guest memory");
        cache.SetImageBufferSources({},{});cache.SetIndependentImageTransfer({});cache.SetCachedDepthRecreationTransfer({});cache.SetIndependentVersionTransfer({},{},{});cache.SetOwnershipAcquirer({});cache.SetOwnershipResolver({});cache.SetTransferSubmissionPublisher({});cache.SetBufferResidencyPublisher({});renderer.SetCpuPageRelease({});
        cache.Collect(renderer.CompletedFence());renderer.ValidateDebugMessages();++executed;
    }
    require(executed==unsigned(late_change||color_format?8:16),"Dirty cached depth coverage missing");
    if(late_change){printf("PASS: %u private depth late source cases mode=%d; no early I/meta/ownership publication, current native/B selection, S/HTILE preservation, typed/native/guest/capacity/padding and held-fence leases\n",executed,late_change);return;}
    if(color_target||writable_target){printf(writable_target?"PASS: %u cached depth-to-UAV cases; actual clipped Dispatch, typed/native/guest/capacity/padding/source and held-fence leases\n":"PASS: %u cached depth-to-RT cases; original flags, actual CPU faults/bind, lexicographic extent, native typed words, real clipped RT draw/clear, source-after-copy/S, B/padding/capacity, metadata and held-fence leases\n",executed);return;}
    printf("PASS: %u dirty cached %s cases; original GpuModified0/1, real CPU faults, old native D versus current B/new S, growth/aspect extent, typed pixels, post-transfer source, private prewrite after later GPU B writes, stale-I negatives, HTILE, exact capacity/padding and held-fence leases\n",executed,color_source?"color-to-D16/D32":"D16/D32");
}
} // namespace NativeDX12
