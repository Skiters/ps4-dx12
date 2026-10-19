// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/scope_exit.h"
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include <bit>
#include <cstring>

namespace NativeDX12 {
inline std::vector<u32> ProbeShiftedDepthSrv(ID3D12Device* device,Rasterizer& renderer,
    GuestTextureCache::Entry& image,const GuestTextureDesc& view){
    const auto code=CompileMultisampleShader(R"(
Texture2DArray<float> image:register(t0); RWByteAddressBuffer output:register(u0);
cbuffer Shape:register(b0){uint width,height,layers;}
[numthreads(8,8,1)]void main(uint3 p:SV_DispatchThreadID){
 if(any(p>=uint3(width,height,layers)))return;
 if(all(p==0)){uint w,h,l,m;image.GetDimensions(0,w,h,l,m);output.Store4(0,uint4(w,h,l,m));}
 output.Store(16+4*((p.z*height+p.y)*width+p.x),asuint(image.Load(int4(p,0))));})","cs_5_1");
    const u64 bytes=16+u64(view.width)*view.height*view.view_layers*4;
    auto output=GuestTiler::Scratch(device,bytes);
    auto readback=Buffer(device,bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    Descriptors descriptors(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,2,true);
    auto srv=image.Srv(view);device->CreateShaderResourceView(image.texture.handle.Get(),&srv,descriptors.Cpu(0));
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.Format=DXGI_FORMAT_R32_TYPELESS;
    uav.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;uav.Buffer.NumElements=UINT(bytes/4);
    uav.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
    device->CreateUnorderedAccessView(output.handle.Get(),nullptr,&uav,descriptors.Cpu(1));
    D3D12_DESCRIPTOR_RANGE ranges[2]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0},
        {D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,1}};
    D3D12_ROOT_PARAMETER parameters[2]{};parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable={2,ranges};parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[1].Constants={0,0,3};D3D12_ROOT_SIGNATURE_DESC description{2,parameters};
    ComPtr<ID3DBlob> signature,errors;ComPtr<ID3D12RootSignature> root;
    Check(D3D12SerializeRootSignature(&description,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&errors));
    Check(device->CreateRootSignature(0,signature->GetBufferPointer(),signature->GetBufferSize(),IID_PPV_ARGS(&root)));
    D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();
    pd.CS={code->GetBufferPointer(),code->GetBufferSize()};ComPtr<ID3D12PipelineState> pipeline;
    Check(device->CreateComputePipelineState(&pd,IID_PPV_ARGS(&pipeline)));
    auto* list=renderer.RecordingList();image.texture.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    ID3D12DescriptorHeap* heaps[]{descriptors.Heap()};list->SetDescriptorHeaps(1,heaps);
    list->SetComputeRootSignature(root.Get());list->SetPipelineState(pipeline.Get());
    list->SetComputeRootDescriptorTable(0,descriptors.Gpu(0));
    const UINT shape[]{view.width,view.height,view.view_layers};list->SetComputeRoot32BitConstants(1,3,shape,0);
    list->Dispatch((view.width+7)/8,(view.height+7)/8,view.view_layers);output.UavBarrier(list);
    output.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
    list->CopyBufferRegion(readback.handle.Get(),0,output.handle.Get(),0,bytes);renderer.Finish();
    void* mapped{};D3D12_RANGE range{0,SIZE_T(bytes)},empty{};
    Check(readback.handle->Map(0,&range,&mapped));std::vector<u32> result(size_t(bytes/4));
    memcpy(result.data(),mapped,size_t(bytes));readback.handle->Unmap(0,&empty);return result;
}
inline void RunDirtyOwnedDepthFixture(ID3D12Device* device,bool color_source=false,int chance_overlap=0) {
    const auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
    GuestTextureCache cache(device);Rasterizer renderer(device);
    SCOPE_EXIT { renderer.Finish(); };
    GuestTextureDesc depth;depth.address=0x40000;depth.width=13;depth.height=11;depth.pitch=16;
    depth.depth_target=!color_source;depth.render_target=color_source;
    depth.data_format=4;depth.number_format=7;depth.tile_mode=0;
    if(chance_overlap>=2)depth.layers=depth.view_layers=2;
    if(chance_overlap==5||chance_overlap==7)depth.levels=2;
    const auto layout=LayoutGuestTexture(depth);
    std::vector<std::byte> old(size_t(layout.guest_bytes),std::byte{0xa3}),current=old;
    const u32 old_word=std::bit_cast<u32>(0.25f),new_word=std::bit_cast<u32>(0.75f);
    for(UINT mip=0;mip<depth.levels;++mip)for(UINT layer=0;layer<depth.layers;++layer)
    for(UINT y=0;y<std::max(depth.height>>mip,1u);++y)for(UINT x=0;x<std::max(depth.width>>mip,1u);++x){
        const auto at=GuestPixelOffset(depth,layout,mip,layer,x,y);
        const auto word=std::bit_cast<u32>(0.25f+0.25f*layer+0.125f*mip);
        memcpy(old.data()+at,&word,4);memcpy(current.data()+at,&new_word,4);
    }
    cache.SetIndependentImageRetention([](const auto&,const auto&){return true;});
    const auto acquire=[](auto& image){image.buffer_alias_version=true;image.SetGpuOwned(true);return true;};
    unsigned retired{},captured{};
    cache.SetOwnershipAcquirer(acquire);
    cache.SetOwnershipResolver([](auto&){throw std::runtime_error("Dirty depth downloaded stale I into current B");});
    cache.SetIndependentVersionTransfer({},acquire,[&](auto& image){
        ++retired;image.buffer_alias_version=false;image.SetGpuOwned(false);
    });
    cache.SetIndependentImageTransfer([&](auto&,const auto&,const auto&)->GuestTextureCache::IndependentImageVersion{
        ++captured;
        throw std::runtime_error("Readonly depth view must not convert or capture guest B");
    });
    auto& source=cache.Upload(depth,old,renderer.RecordingList(),renderer.CompletedFence());
    require(cache.Own(source),"Dirty source ownership failed");
    auto pin=cache.PinBinding(source);
    if(!chance_overlap)source.MarkLogicalDirty(); // Same retained-allocation state as a real B write invalidation.
    auto sampled=depth;sampled.depth_target=false;sampled.render_target=false;sampled.number_format=4;
    if(chance_overlap)sampled.address=chance_overlap>0?depth.address+256:depth.address-256;
    if(chance_overlap>=2){
        // A real slice boundary must NOT enter the chance-overlap fast path:
        // readonly binding needs native parent contents, not independent B.
        // Writable binding additionally needs a parent commit (still explicit).
        sampled.address=depth.address+layout.mips[0].slice_bytes;
        sampled.layers=sampled.view_layers=sampled.levels=1;
        if(chance_overlap!=4){sampled.sampled_depth=true;sampled.number_format=depth.number_format;}
        if(chance_overlap==5||chance_overlap==7){
            sampled.width=std::max(depth.width>>1,1u);sampled.height=std::max(depth.height>>1,1u);
            sampled.pitch=layout.mips[1].pitch;
            sampled.address=depth.address+layout.mips[1].offset;
            if(chance_overlap==7)sampled.address+=layout.mips[1].slice_bytes;
        }
    }
    if(chance_overlap==3){
        sampled.writable=true;bool rejected=false;
        try{(void)cache.Upload(sampled,current,renderer.RecordingList(),renderer.CompletedFence());}
        catch(const std::runtime_error& e){rejected=std::string(e.what())==
            "Independent depth/color overlap needs an exact physical transfer plan";}
        require(rejected&&!retired&&!captured&&source.gpu_owned&&!source.logical_dirty,
            "Real shifted depth slice bypassed required mapping or changed its source");
        renderer.Finish();renderer.ValidateDebugMessages();
        puts("PASS: writable shifted-depth-slice still requires explicit parent commit, never independent B");
        return;
    }
    GuestTextureCache::Entry* independent{};
    if(chance_overlap==6){
        independent=&cache.Upload(sampled,current,renderer.RecordingList(),renderer.CompletedFence(),{},true);
        require(cache.Own(*independent),"Independent child ownership failed");
        require(cache.FindDepthViewParent(sampled,LayoutGuestTexture(sampled))==nullptr,
            "Shifted depth parent replaced a newer independent child version");
    }
    const bool subresource=chance_overlap==2||chance_overlap==5;
    const auto storage=subresource?cache.ResolveDepthViewStorage(sampled):sampled;
    auto& target=cache.Upload(storage,current,renderer.RecordingList(),renderer.CompletedFence());
    if(independent)require(&target==independent,"Independent child was replaced by its parent");
    // Publishing a current same-class refresh supersedes cached_native_current;
    // the old pinned allocation must still retain its original GPU contents.
    if(chance_overlap==2||chance_overlap==5){
        require(!retired&&!captured&&source.gpu_owned&&source.buffer_alias_version&&
            source.HasValidLogicalImage()&& &target==&source&&target.cached_native_current,
            "Readonly depth slice replaced its parent or acquired independent ownership");
    }else if(chance_overlap){
        require(!retired&&!captured&&source.gpu_owned&&source.buffer_alias_version&&
            !source.logical_dirty&&source.cached_native_current&&target.cached_native_current,
            "Chance overlap copied/retired independent I or lost the separate current-B image");
    }else require(retired==1&&!captured&&!source.gpu_owned&&source.logical_dirty&&
        !source.cached_native_current&&target.cached_native_current,
        "Dirty source was copied, remained owned, or superseded current refresh");
    const auto read=[&](Resource& image,UINT subresource=0){
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT64 bytes{};const auto desc=image.handle->GetDesc();
        device->GetCopyableFootprints(&desc,subresource,1,0,&fp,nullptr,nullptr,&bytes);
        auto out=Buffer(device,bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        image.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION from{},to{};from.pResource=image.handle.Get();
        from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;from.SubresourceIndex=subresource;to.pResource=out.handle.Get();
        to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint=fp;
        renderer.RecordingList()->CopyTextureRegion(&to,0,0,0,&from,nullptr);
        return std::pair(std::move(out),fp);
    };
    const auto expected_view=std::bit_cast<u32>(chance_overlap==5?0.375f:0.5f);
    if(subresource){
        auto words=ProbeShiftedDepthSrv(device,renderer,target,sampled);
        require(words[0]==sampled.width&&words[1]==sampled.height&&words[2]==sampled.view_layers&&words[3]==1,
            "Shifted depth SRV exposed wrong logical dimensions/layer/mip count");
        require(std::all_of(words.begin()+4,words.end(),[&](u32 word){return word==expected_view;}),
            "GPU depth SRV read wrong parent mip/layer contents");
    }
    auto before=read(source.texture),after=read(target.texture,
        subresource?(chance_overlap==5?1:depth.levels):0);
    std::optional<decltype(before)> selected;
    if(subresource)selected.emplace(read(source.texture,depth.levels));
    renderer.Finish();
    std::vector<decltype(before)*> planes{&before,&after};
    if(selected)planes.push_back(&*selected);
    for(auto* plane:planes){
        void* mapped{};D3D12_RANGE range{0,SIZE_T(plane->first.handle->GetDesc().Width)},empty{};
        Check(plane->first.handle->Map(0,&range,&mapped));bool exact=true;
        for(UINT y=0;y<plane->second.Footprint.Height;++y)for(UINT x=0;x<plane->second.Footprint.Width;++x){u32 word{};
            memcpy(&word,static_cast<std::byte*>(mapped)+plane->second.Offset+y*plane->second.Footprint.RowPitch+4*x,4);
            exact&=word==(plane==&before?old_word:subresource?
                (plane==&after?expected_view:std::bit_cast<u32>(0.5f)):new_word);
        }
        plane->first.handle->Unmap(0,&empty);require(exact,"Dirty native history/current B GPU contents mismatch");
    }
    renderer.ValidateDebugMessages();
    if(chance_overlap==2||chance_overlap==5)puts("PASS: readonly shifted depth slice reads selected native mip/layer and preserves full parent ownership/content");
    else if(chance_overlap)printf("PASS: %s %s chance overlap preserves current I and samples independent B, without copy/retirement\n",
        color_source?"color":"depth",chance_overlap>0?"right":"left");
    else puts("PASS: dirty owned image retires stale I without capture/download, preserves old native contents and samples current B");
}
} // namespace NativeDX12
