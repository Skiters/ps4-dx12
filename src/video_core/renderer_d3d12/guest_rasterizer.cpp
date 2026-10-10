#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_rasterizer.h"
#include "video_core/amdgpu/liverpool.h"
#include "guest_graphics_state.h"
#include "guest_viewports.h"
#include "guest_image_readback.h"
#include "guest_attachment_retirement.h"
#include "guest_image_operations.h"
#include "diagnostics.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#ifdef SHADPS4_NATIVE_DX12_CORE
#include "core/memory.h"
#endif
namespace NativeDX12 {
namespace {
bool PartialRenderingEnabled() {
    static const bool enabled=[] {
        const auto* option=std::getenv("SHADPS4_DX12_PARTIAL_RENDER");
        std::error_code error;
        const bool value=option?std::strcmp(option,"1")==0:
            std::filesystem::exists("partial-render.enabled",error);
        if(value){fprintf(stderr,"DX12 EXPERIMENTAL partial rendering: depth-only/large targets skipped; complex geometry sampled 1/8; visual correctness NOT guaranteed\n");fflush(stderr);}
        return value;
    }();
    return enabled;
}
Shader::Backend::DX12::SpirvConvertOptions NativeOptions() {
    Shader::Backend::DX12::SpirvConvertOptions o;o.runtime_viewport_flips=true;
    o.shader_model_minor=6;o.compute_wave_size=32;o.native_storage_buffer_uavs=true;return o;
}
bool Overlap(u64 a,u64 as,u64 b,u64 bs) {
    if(a>UINT64_MAX-as||b>UINT64_MAX-bs)throw std::out_of_range("Guest draw memory extent overflow");
    return a<b+bs&&b<a+as;
}
bool ImageOverlap(VAddr address,u64 size,const GuestTextureCache::Entry& image) {
    return Overlap(address,size,image.desc.address,image.layout.guest_bytes)||
        (image.desc.stencil_address&&Overlap(address,size,image.desc.stencil_address,image.stencil_layout.guest_bytes));
}
}
GuestRasterizer::GuestRasterizer(ID3D12Device* device,AmdGpu::Liverpool& gpu,
    const Shader::Profile& profile,std::wstring converter,std::wstring dxc)
    :Rasterizer(device),device_(device),gpu_(gpu),frontend_(profile),shaders_(std::move(converter),NativeOptions()),
     pipelines_(device,std::move(dxc),frontend_,shaders_),textures_(device) {
    D3D12_FEATURE_DATA_D3D12_OPTIONS13 blend_options{};
    native_alpha_factor_=SUCCEEDED(DX12_API_CALL(device->CheckFeatureSupport(
        D3D12_FEATURE_D3D12_OPTIONS13,&blend_options,sizeof(blend_options))))&&blend_options.AlphaBlendFactorSupported;
    EnableHostSpanCache(true);
    textures_.SetCopySubmission([this](const auto& record){return SubmitCopyCommands(record);});
    textures_.SetPendingRetirement([this]{
        Finish();const auto fence=Flush();if(fence)textures_.Submitted(fence);
        RecordingList();
    });
    if(!profile.needs_buffer_offsets)
        throw std::invalid_argument("Native guest backend requires compiler buffer offset adjustment");
#ifdef SHADPS4_NATIVE_DX12_CORE
    const auto* pages=&Core::Memory::Instance()->GetAddressSpace().BackingPages();
    const auto has=[pages](VAddr address,u64 size){
        if(!size||address>=(1ull<<40)||size>(1ull<<40)-address)return false;
        for(auto current=address;current<address+size;){auto* entry=pages->find(current>>14);
            if(!entry||!*entry)return false;current+=std::min<u64>(16384-current%16384,address+size-current);}
        return true;
    };
    SetBackingAccess(has,[pages,has](VAddr address,std::span<std::byte> result){
        if(!has(address,result.size()))return false;
        while(!result.empty()){const auto amount=std::min<u64>(16384-address%16384,result.size());
            memcpy(result.data(),*pages->find(address>>14)+address%16384,amount);address+=amount;result=result.subspan(amount);}
        return true;
    },[pages,has](VAddr address,std::span<const std::byte> source){
        if(!has(address,source.size()))return false;
        while(!source.empty()){const auto amount=std::min<u64>(16384-address%16384,source.size());
            memcpy(*pages->find(address>>14)+address%16384,source.data(),amount);address+=amount;source=source.subspan(amount);}
        return true;
    });
#endif
}
GuestRasterizer::~GuestRasterizer(){try{ResolveGpuBuffers(0,UINT64_MAX);ResolveMemory(0,UINT64_MAX);Finish();}catch(...) {/* retire before shader/texture ownership ends */}}
void GuestRasterizer::SetBackingAccess(std::function<bool(VAddr,u64)> has,
    std::function<bool(VAddr,std::span<std::byte>)> read,std::function<bool(VAddr,std::span<const std::byte>)> write){
    std::lock_guard lock(execution_mutex_);has_backing_=std::move(has);read_backing_=std::move(read);write_backing_=std::move(write);
    if(!page_watch_)page_watch_=std::make_unique<PageWatch>([this](VAddr address){
        std::lock_guard lock(execution_mutex_);ResolveGpuBuffers(address,1);ReleaseCpuBufferWatches(address,1);ReleaseImmutableWatch(address,1);ResolveMemory(address,1);
    });
    SetBufferOwnership([this](VAddr address,u64 size){
        // Own only whole committed pages, never unrelated PS4 suballocations.
        if(address%4096||size%4096||!has_backing_(address,size))return false;
        ReleaseImmutableWatch(address,size);ResolveMemory(address,size);
        return page_watch_->Track(address,size);
    },[this](VAddr address,std::span<const std::byte> data){return write_backing_(address,data);},
      [this](VAddr address,u64 size){page_watch_->Untrack(address,size);});
    SetBufferWriteWatch([this](VAddr address,u64 size){
        if(address%4096||size%4096||!has_backing_(address,size)||page_watch_->Contains(address))return false;
        return page_watch_->Track(address,size,true);
    });
    EnableBufferStreaming(true);
    EnableBufferArenas(true);
    textures_.SetImmutableWatch([this](auto& image){
        ReleaseCpuBufferWatches(image.desc.address,image.layout.guest_bytes);
        if(page_watch_&&has_backing_(image.desc.address,image.layout.guest_bytes)&&
            page_watch_->Track(image.desc.address,image.layout.guest_bytes,true))image.cpu_watched=true;
    },[this](auto& image){ReleaseImmutableWatch(image.desc.address,image.layout.guest_bytes);});
    textures_.SetOwnershipResolver([this](auto& image){
        ResolveImage(image);
        // Upload already received this command-list object. Resolution closes
        // and submits it; reopen it before Upload records further commands.
        RecordingList();
    });
    textures_.SetOwnershipAcquirer([this](auto& image){return OwnImage(image);});
}
bool GuestRasterizer::OwnImage(GuestTextureCache::Entry& image){
    if(image.gpu_owned)return true;
    // Ownership is a coherence rule, not a size optimization. A 640x360
    // color target is below 1 MiB; eagerly downloading it after every draw
    // defeats the low-resolution path. Track still rejects partial pages,
    // overlapping ranges and incompatible memory protection.
    if(!page_watch_||!has_backing_(image.desc.address,image.layout.guest_bytes)||
       (image.desc.stencil_address&&!has_backing_(image.desc.stencil_address,image.stencil_layout.guest_bytes)))return false;
    ResolveGpuBuffers(image.desc.address,image.layout.guest_bytes);
    if(image.desc.stencil_address)ResolveGpuBuffers(image.desc.stencil_address,image.stencil_layout.guest_bytes);
    ReleaseImmutableWatch(image.desc.address,image.layout.guest_bytes);
    if(image.desc.stencil_address)ReleaseImmutableWatch(image.desc.stencil_address,image.stencil_layout.guest_bytes);
    if(!ApiTrace::Call("MEMORY","GpuImagePageOwnership",[&]{return page_watch_->Track(image.desc.address,image.layout.guest_bytes);},image.desc.address,image.layout.guest_bytes))return false;
    try {
        if(image.desc.stencil_address&&!page_watch_->Track(image.desc.stencil_address,image.stencil_layout.guest_bytes)){
            page_watch_->Untrack(image.desc.address,image.layout.guest_bytes);return false;
        }
    }catch(...){page_watch_->Untrack(image.desc.address,image.layout.guest_bytes);throw;}
    image.gpu_owned=true;return true;
}
void GuestRasterizer::ResolveImage(GuestTextureCache::Entry& image){
    std::lock_guard lock(execution_mutex_);if(!image.gpu_owned)return;
    ApiTrace::Call("MEMORY","ResolveGpuOwnedImage",[&]{
        Rasterizer::Finish();
        std::vector<std::byte> result(size_t(image.layout.guest_bytes));
        if(!read_backing_(image.desc.address,result))throw std::runtime_error("GPU-owned image backing was removed before retirement");
        std::vector<std::byte> stencil;
        if(image.desc.stencil_address){stencil.resize(size_t(image.stencil_layout.guest_bytes));
            if(!read_backing_(image.desc.stencil_address,stencil))throw std::runtime_error("GPU-owned stencil backing was removed before retirement");}
        GuestImageReadback readback(device_.Get(),image,RecordingList());
        const auto fence=Flush();textures_.Submitted(fence);readback.Submitted(fence);Rasterizer::Finish();
        readback.Resolve(CompletedFence(),result,stencil);
        if(!write_backing_(image.desc.address,result))throw std::runtime_error("Cannot commit GPU-owned image through physical backing");
        if(image.desc.stencil_address&&!write_backing_(image.desc.stencil_address,stencil))throw std::runtime_error("Cannot commit GPU-owned stencil through physical backing");
        if(!image.desc.writable)image.RememberSnapshot(result,stencil);image.gpu_owned=false;
        page_watch_->Untrack(image.desc.address,image.layout.guest_bytes);
        if(image.desc.stencil_address)page_watch_->Untrack(image.desc.stencil_address,image.stencil_layout.guest_bytes);
    },image.desc.address,image.layout.guest_bytes);
}
void GuestRasterizer::ResolveMemory(VAddr address,u64 size){
    std::lock_guard lock(execution_mutex_);
    for(auto& image:textures_.Entries())if(image->gpu_owned&&ImageOverlap(address,size,*image))ResolveImage(*image);
}
void GuestRasterizer::ReleaseImmutableWatch(VAddr address,u64 size){
    std::lock_guard lock(execution_mutex_);
    ReleaseCpuBufferWatches(address,size);
    for(auto& image:textures_.Entries())if(image->cpu_watched&&Overlap(address,size,image->desc.address,image->layout.guest_bytes)){
        image->cpu_watched=false;page_watch_->Untrack(image->desc.address,image->layout.guest_bytes);
    }
}
void GuestRasterizer::CheckBacking(VAddr address,u64 size,bool writing){
    ResolveGpuBuffers(address,size);
    if(writing)ReleaseImmutableWatch(address,size);
    Rasterizer::CheckBacking(address,size,writing);
}
void GuestRasterizer::DiscardImages(VAddr address,u64 size){
    std::lock_guard lock(execution_mutex_);
    const bool overlaps=std::any_of(textures_.Entries().begin(),textures_.Entries().end(),[&](const auto& image){
        return ImageOverlap(address,size,*image);});
    if(!overlaps)return;
    Rasterizer::Finish();
    for(auto& image:textures_.Entries())if(image->gpu_owned&&ImageOverlap(address,size,*image)){
        image->gpu_owned=false;page_watch_->Untrack(image->desc.address,image->layout.guest_bytes);
        if(image->desc.stencil_address)page_watch_->Untrack(image->desc.stencil_address,image->stencil_layout.guest_bytes);}
    textures_.Invalidate(address,size,CompletedFence());
}
bool GuestRasterizer::ReadMemory(VAddr address,u64 size,bool assume){std::lock_guard lock(execution_mutex_);ResolveMemory(address,size);return Rasterizer::ReadMemory(address,size,assume);}
bool GuestRasterizer::InvalidateMemory(VAddr address,u64 size,bool assume){
    ReleaseImmutableWatch(address,size);
    // A real CPU access has already resolved the guard before the write. Keep
    // the independent fresh-file fast path when no image owns these pages.
    if(page_watch_&&page_watch_->Contains(address))ResolveMemory(address,size);
    return Rasterizer::InvalidateMemory(address,size,assume);
}
void GuestRasterizer::RegisterMemory(VAddr address,u64 size){ReleaseImmutableWatch(address,size);DiscardImages(address,size);Rasterizer::RegisterMemory(address,size);}
void GuestRasterizer::UnmapMemory(VAddr address,u64 size){std::lock_guard lock(execution_mutex_);ReleaseImmutableWatch(address,size);depth_metadata_.Remove(address,size);ResolveMemory(address,size);DiscardImages(address,size);Rasterizer::UnmapMemory(address,size);}
void GuestRasterizer::FillBuffer(VAddr address,u32 size,u32 value,bool gds){
    std::lock_guard lock(execution_mutex_);
    Rasterizer::FillBuffer(address,size,value,gds);
    if(!gds&&size)depth_metadata_.Clear(address);
}
void GuestRasterizer::PrepareMemoryProtection(VAddr address,u64 size){ResolveGpuBuffers(address,size);ReleaseImmutableWatch(address,size);ResolveMemory(address,size);DiscardImages(address,size);}
Resource* GuestRasterizer::CoherentDisplayTexture(VAddr address,UINT width,UINT height,UINT pitch){
    std::lock_guard lock(execution_mutex_);Finish();
    return textures_.CoherentDisplayTexture(address,width,height,pitch,CompletedFence());
}
Resource* GuestRasterizer::PrepareDisplayTexture(VAddr address,UINT width,UINT height,UINT pitch,
                                                  ID3D12CommandQueue* consumer){
    std::lock_guard lock(execution_mutex_);
    const auto submitted=Flush();
    if(submitted)textures_.Submitted(submitted);
    textures_.Collect(CompletedFence());
    auto* texture=textures_.CoherentDisplayTexture(address,width,height,pitch,
        submitted?submitted:CompletedFence());
    if(texture&&submitted)MakeQueueWait(consumer,submitted);
    return texture;
}
void GuestRasterizer::MaterializeColorClear(const AmdGpu::ColorBuffer& b,
    const GuestTextureDesc& d,const GuestTextureLayout& l) {
    if(!b.info.fast_clear||!b.cmask_base_address)return;
    const auto meta_bytes=GuestCmaskBytes(b);
    // Fast-clear enable is persistent state, not evidence of a new clear.
    // Inspect the small metadata plane before resolving the color surface;
    // expanded (0xff) metadata must leave the render target GPU-resident.
    if(!ReadMemory(b.CmaskAddress(),meta_bytes))
        throw std::runtime_error("Native CMASK backing is not mapped");
    ValidateHostMemory(b.CmaskAddress(),meta_bytes,true);
    if(!GuestColorClearPending(b,d,l,
        {reinterpret_cast<const std::byte*>(b.CmaskAddress()),size_t(meta_bytes)}))return;
    if(!ReadMemory(d.address,l.guest_bytes))
        throw std::runtime_error("Native color backing is not mapped");
    ValidateHostMemory(d.address,l.guest_bytes,true);
    static unsigned clear_traces{};
    if(std::getenv("SHADPS4_NATIVE_STARTUP_TRACE")&&clear_traces++<16){
        fprintf(stderr,"DX12 CMASK surface=%llx meta=%llx bytes=%llu first=%02x clear=%08x,%08x\n",
            (unsigned long long)d.address,(unsigned long long)b.CmaskAddress(),
            (unsigned long long)meta_bytes,unsigned(*reinterpret_cast<const unsigned char*>(b.CmaskAddress())),b.clear_word0,b.clear_word1);fflush(stderr);
    }
    if(MaterializeGuestColorClear(b,d,l,
        {reinterpret_cast<std::byte*>(d.address),size_t(l.guest_bytes)},
        {reinterpret_cast<std::byte*>(b.CmaskAddress()),size_t(meta_bytes)})) {
        InvalidateMemory(d.address,l.guest_bytes);
        InvalidateMemory(b.CmaskAddress(),meta_bytes);
    }
}
void GuestRasterizer::ResolveColorTargets(){
    std::lock_guard lock(execution_mutex_);
    const auto& r=gpu_.regs;const auto& source_buffer=r.color_buffers[0];
    const auto& destination_buffer=r.color_buffers[1];
    if(!source_buffer||!destination_buffer)throw std::runtime_error("Guest color resolve requires MRT0 and MRT1");
    if(source_buffer.NumSamples()!=1||destination_buffer.NumSamples()!=1)
        throw std::runtime_error("Native MSAA color resolve requires multisample guest tiling support");
    auto source_desc=DescribeGuestColorBuffer(source_buffer,gpu_.last_cb_extent[0]);
    auto destination_desc=DescribeGuestColorBuffer(destination_buffer,gpu_.last_cb_extent[1]);
    auto source_layout=LayoutGuestTexture(source_desc),destination_layout=LayoutGuestTexture(destination_desc);
    ValidateHostMemory(source_desc.address,source_layout.guest_bytes);
    ValidateHostMemory(destination_desc.address,destination_layout.guest_bytes,true);
    MaterializeColorClear(source_buffer,source_desc,source_layout);
    auto& source=textures_.Upload(source_desc,{reinterpret_cast<const std::byte*>(source_desc.address),
        size_t(source_layout.guest_bytes)},RecordingList(),CompletedFence());
    auto& destination=textures_.Upload(destination_desc,{reinterpret_cast<const std::byte*>(destination_desc.address),
        size_t(destination_layout.guest_bytes)},RecordingList(),CompletedFence());
    for(auto& entry:textures_.Entries())if(entry.get()!=&destination&&entry.get()!=&source&&entry->gpu_owned&&
        ImageOverlap(destination.desc.address,destination.layout.guest_bytes,*entry))ResolveImage(*entry);
    if(!OwnImage(destination))throw std::runtime_error("Cannot acquire guest resolve destination ownership");
    RecordGuestImageCopy(source,destination,RecordingList(),true);
}
void GuestRasterizer::Draw(bool indexed,u32 index_offset) {
    try {DrawInternal(indexed,index_offset,nullptr,0,0);}
    catch(const std::exception& e){fprintf(stderr,"Native DX12 Draw failed: %s\n",e.what());fflush(stderr);throw;}
}
void GuestRasterizer::DrawInternal(bool indexed,u32 index_offset,const DrawArguments* indirect,
    u16 vertex_sgpr,u16 instance_sgpr) {
    Diagnostics::Scope diagnostic(Diagnostics::Kind::Draw,"execution-lock");
    std::lock_guard lock(execution_mutex_);
    const auto& r=gpu_.regs;
    diagnostic.Detail("primitive=%u count=%u indexed=%u cb0=%llx",u32(r.primitive_type),r.num_indices,
        unsigned(indexed),(unsigned long long)r.color_buffers[0].Address());
    diagnostic.Stage("guest-state");
    if(r.color_control.mode==AmdGpu::ColorControl::OperationMode::EliminateFastClear||
       r.color_control.mode==AmdGpu::ColorControl::OperationMode::FmaskDecompress) {
        const auto& b=r.color_buffers[0];
        if(b) {
            const auto d=DescribeGuestColorBuffer(b,gpu_.last_cb_extent[0]);
            MaterializeColorClear(b,d,LayoutGuestTexture(d));
        }
        return;
    }
    if(r.color_control.mode==AmdGpu::ColorControl::OperationMode::Resolve){
        diagnostic.Stage("color-resolve");ResolveColorTargets();return;
    }
    const bool color_disabled=r.color_control.mode==AmdGpu::ColorControl::OperationMode::Disable;
    const bool copy_depth=r.depth_render_override.force_z_dirty&&r.depth_render_override.force_z_valid&&
        r.depth_buffer.DepthValid()&&r.depth_buffer.DepthWriteValid()&&
        r.depth_buffer.DepthAddress()!=r.depth_buffer.DepthWriteAddress();
    const bool copy_stencil=r.depth_render_override.force_stencil_dirty&&r.depth_render_override.force_stencil_valid&&
        r.depth_buffer.StencilValid()&&r.depth_buffer.StencilWriteValid()&&
        r.depth_buffer.StencilAddress()!=r.depth_buffer.StencilWriteAddress();
    if(color_disabled&&(copy_depth||copy_stencil)){
        diagnostic.Stage("depth-stencil-override-copy");
        auto source=DescribeGuestDepthBuffer(r.depth_buffer,r.depth_view,gpu_.last_db_extent);
        const auto depth_layout=LayoutGuestTexture(source);
        const auto copy_plane=[&](VAddr from,VAddr to,u64 bytes){
            ReadMemory(from,bytes);ValidateHostMemory(from,bytes);
            PrepareMemoryProtection(to,bytes);ValidateHostMemory(to,bytes,true);
            memmove(reinterpret_cast<void*>(to),reinterpret_cast<const void*>(from),size_t(bytes));
            Rasterizer::InvalidateMemory(to,bytes);
        };
        if(copy_depth)copy_plane(source.address,r.depth_buffer.DepthWriteAddress(),depth_layout.guest_bytes);
        if(copy_stencil){const auto stencil=DescribeGuestStencil(source);const auto layout=LayoutGuestTexture(stencil);
            copy_plane(stencil.address,r.depth_buffer.StencilWriteAddress(),layout.guest_bytes);}
        return;
    }
    static u32 traced_draws{};
    const bool trace=std::getenv("SHADPS4_NATIVE_STARTUP_TRACE")&&(++traced_draws<=16||traced_draws%512==0);
    if(trace){fprintf(stderr,"DX12 startup draw %u: primitive=%u count=%u cbmode=%u mask=%08x depth=%u stencil=%u\n",
        traced_draws,u32(r.primitive_type),r.num_indices,u32(r.color_control.mode),r.color_target_mask.raw,
        r.depth_control.depth_enable,r.depth_control.stencil_enable);fflush(stderr);}
    DrawArguments draw=indirect?*indirect:DrawArguments{r.num_indices,r.num_instances.NumInstances(),
        indexed?0:r.index_offset,0,indexed?s32(r.index_offset):0};
    if(!draw.count||!draw.instances||r.clipper_control.dx_rasterization_kill||r.primitive_type==AmdGpu::PrimitiveType::None)return;
    if(PartialRenderingEnabled()) {
        bool color=false,large=false;
        for(UINT i=0;i<8;++i)if(r.color_control.mode!=AmdGpu::ColorControl::OperationMode::Disable&&
            r.color_buffers[i]&&r.color_target_mask.GetMask(i)){
            color=true;
            const auto extent=gpu_.last_cb_extent[i];
            large|=extent.Valid()&&uint64_t(extent.width)*extent.height>2560ull*1440;
        }
        static uint64_t complex_draws{},skipped{};
        // Keep text/UI and screen compositing: they usually do not write
        // depth. Thin only substantial geometry that writes a depth surface.
        const bool thin=draw.count>128&&r.depth_control.depth_enable&&
            r.depth_control.depth_write_enable&&((++complex_draws%8)!=0);
        if(!color||large||thin){
            ApiTrace::Call("DIAGNOSTIC","PartialRender.SkipDraw",[]{},!color,large,thin,draw.count);
            if(++skipped==1||skipped%4096==0){fprintf(stderr,"DX12 partial rendering skipped=%llu draws\n",(unsigned long long)skipped);fflush(stderr);}
            return;
        }
    }
    const auto depth_tests=GuestDepthTests(r);
    const bool has_depth=depth_tests.HasAttachment();
    if(Diagnostics::Enabled()&&((r.depth_control.stencil_enable&&!depth_tests.stencil)||
        (r.depth_control.depth_enable&&!depth_tests.depth))) {
        static u32 reported{};
        if(reported++<8)fprintf(stderr,"DX12 guest DB gate: depth_enable=%u valid=%u stencil_enable=%u valid=%u z_base=%llx s_base=%llx z_format=%u s_format=%u\n",
            r.depth_control.depth_enable,unsigned(r.depth_buffer.DepthValid()),r.depth_control.stencil_enable,
            unsigned(r.depth_buffer.StencilValid()),(unsigned long long)r.depth_buffer.DepthAddress(),
            (unsigned long long)r.depth_buffer.StencilAddress(),u32(r.depth_buffer.z_info.format),u32(r.depth_buffer.stencil_info.format));
    }
    // Normal PS4 draws bind the read-base image as their depth target.  The
    // separate write bases are consumed only by the force-dirty/force-valid
    // DepthStencilCopy command handled above (the Vulkan backend follows the
    // same split).  Do not reject an otherwise ordinary draw merely because
    // those dormant registers contain a different address.
    auto viewport=BuildGuestViewports(r);if(viewport.viewports.empty())return;
    if(trace){const auto& v=viewport.viewports[0];const auto& s=viewport.scissors[0];
        fprintf(stderr,"DX12 startup draw %u: viewport=%g,%g,%g,%g scissor=%ld,%ld,%ld,%ld flip=%x cb0=%llx\n",traced_draws,
            v.TopLeftX,v.TopLeftY,v.Width,v.Height,s.left,s.top,s.right,s.bottom,viewport.flip_mask,(unsigned long long)r.color_buffers[0].Address());fflush(stderr);}
    const bool yflip=(viewport.flip_mask&1)!=0;
    bool mixed_viewport_winding=false;
    for(size_t i=1;i<viewport.viewports.size();++i)
        if(bool(viewport.flip_mask&(1u<<i))!=yflip)
            mixed_viewport_winding=true;
    diagnostic.Stage("shader-translation");
    auto guest=frontend_.BuildGraphics(gpu_,vertex_sgpr,instance_sgpr);
    diagnostic.Stage("attachment-validation");
    if(trace){fprintf(stderr,"DX12 startup draw %u: guest shaders translated\n",traced_draws);fflush(stderr);}
    std::vector<GuestResourceStage> stages;
    for(const auto* s:guest.stages)if(s) {
        if(s->info.has_fetch_shader)throw std::runtime_error("Native guest external fetch shader needs input assembly");
        stages.push_back({&s->info,s->bindings_start});
    }
    std::array<GuestTextureDesc,9> descriptions{};
    std::array<GuestTextureLayout,9> layouts{};
    std::array<bool,8> color_feedback{};
    std::array<DXGI_FORMAT,8> formats{};UINT target_count{};
    for(UINT i=0;i<8;++i)if(r.color_control.mode!=AmdGpu::ColorControl::OperationMode::Disable&&
        r.color_buffers[i]&&r.color_target_mask.GetMask(i)&&(guest.mrt_mask&(1u<<i))) {
        descriptions[i]=DescribeGuestColorBuffer(r.color_buffers[i],gpu_.last_cb_extent[i]);
        layouts[i]=LayoutGuestTexture(descriptions[i]);formats[i]=layouts[i].format;target_count=i+1;
        ValidateHostMemory(descriptions[i].address,layouts[i].guest_bytes,true);
        MaterializeColorClear(r.color_buffers[i],descriptions[i],layouts[i]);
        for(UINT j=0;j<i;++j)if(formats[j]!=DXGI_FORMAT_UNKNOWN&&
            Overlap(descriptions[i].address,layouts[i].guest_bytes,descriptions[j].address,layouts[j].guest_bytes))
            throw std::runtime_error("Aliased guest render targets require shared subresources");
        for(const auto& stage:stages) {
            for(const auto& image:stage.info->images) {
                const auto d=DescribeGuestTexture(image.GetSharp(*stage.info));const auto l=LayoutGuestTexture(d);
                if(!image.is_written&&Overlap(d.address,l.guest_bytes,descriptions[i].address,layouts[i].guest_bytes))
                    color_feedback[i]=true;
            }
        }
    }
    DXGI_FORMAT depth_format=DXGI_FORMAT_UNKNOWN;
    bool depth_feedback=false;
    GuestTextureDesc stencil_desc;GuestTextureLayout stencil_layout;
    std::span<std::byte> stencil_memory;
    if(has_depth) {
        descriptions[8]=DescribeGuestDepthBuffer(r.depth_buffer,r.depth_view,gpu_.last_db_extent);
        // Declared stencil allocation does not mean this draw accesses it.
        // Keep depth-only passes eligible for GPU page ownership. Re-enabling
        // stencil selects the combined view and Upload resolves the old depth
        // owner before initializing that view from coherent guest memory.
        if(!depth_tests.stencil)descriptions[8].stencil_address=0;
        layouts[8]=LayoutGuestTexture(descriptions[8]);
        ValidateHostMemory(descriptions[8].address,layouts[8].guest_bytes,depth_tests.depth&&r.depth_control.depth_write_enable);
        if(descriptions[8].stencil_address){
            stencil_desc=DescribeGuestStencil(descriptions[8]);stencil_layout=LayoutGuestTexture(stencil_desc);
            ValidateHostMemory(stencil_desc.address,stencil_layout.guest_bytes,bool(r.depth_control.stencil_enable));
            // The texture cache resolves incompatible image ownership before
            // CPU access. Retire buffer aliases without downloading an exact
            // GPU-owned depth/stencil attachment on every draw.
            Rasterizer::ReadMemory(stencil_desc.address,stencil_layout.guest_bytes);
            stencil_memory={reinterpret_cast<std::byte*>(stencil_desc.address),size_t(stencil_layout.guest_bytes)};
        }
        depth_format=descriptions[8].stencil_address?DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
            layouts[8].bytes_per_pixel==4?DXGI_FORMAT_D32_FLOAT:DXGI_FORMAT_D16_UNORM;
        for(UINT i=0;i<target_count;++i)if(formats[i]!=DXGI_FORMAT_UNKNOWN&&
            Overlap(descriptions[8].address,layouts[8].guest_bytes,descriptions[i].address,layouts[i].guest_bytes))
            throw std::runtime_error("Guest depth/color alias requires shared storage");
        for(const auto& stage:stages) {
            for(const auto& image:stage.info->images) {
                const auto d=DescribeGuestTexture(image.GetSharp(*stage.info));const auto l=LayoutGuestTexture(d);
                if(Overlap(d.address,l.guest_bytes,descriptions[8].address,layouts[8].guest_bytes)){
                    if(!image.is_written&&r.depth_view.z_read_only&&!r.depth_control.depth_write_enable&&
                       CompatibleGuestSampledStorage(descriptions[8],d))depth_feedback=true;
                }
            }
        }
    }
    auto state=BuildGuestGraphicsState(r,std::span(formats.data(),target_count),depth_format,
        r.ls_hs_config.hs_input_control_points,yflip,native_alpha_factor_);
    // Position conversion flips each viewport independently. D3D12 has one
    // front-face/cull mode per draw, so retain both faces for mixed winding.
    if(mixed_viewport_winding)state.pipeline.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
    std::error_code depth_probe_error;
    if(indexed&&target_count>1&&std::filesystem::exists("scene-depth-bypass.enabled",depth_probe_error)){
        state.pipeline.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;
        state.pipeline.DepthStencilState.StencilEnable=FALSE;
        state.depth_bounds=false;
    }
    if(state.discard)return;
    GuestTextureCache::Entry* feedback_depth{};
    if(depth_feedback){
        feedback_depth=&textures_.Upload(descriptions[8],
            std::span(reinterpret_cast<const std::byte*>(descriptions[8].address),size_t(layouts[8].guest_bytes)),
            RecordingList(),CompletedFence(),stencil_memory);
    }
    // D3D12 cannot place one subresource in RENDER_TARGET and SHADER_RESOURCE
    // states simultaneously.  Materialize the previous color contents and
    // evict its native attachment view before resource binding; the sampled
    // upload then becomes an immutable snapshot and the callback creates a
    // distinct RTV for this draw.  This is the native equivalent of Vulkan's
    // attachment-feedback layout without violating D3D12 state rules.
    for(UINT i=0;i<target_count;++i)if(color_feedback[i]){
        ResolveMemory(descriptions[i].address,layouts[i].guest_bytes);
        DiscardImages(descriptions[i].address,layouts[i].guest_bytes);
    }
    bool metadata_clear=false;
    if(has_depth){
        const auto meta=r.depth_htile_data_base.GetAddress();
        depth_metadata_.Register(meta);
        metadata_clear=depth_metadata_.Consume(meta,r.depth_view.slice_start);
    }
    const bool clear_depth=depth_tests.depth&&((r.depth_render_control.depth_clear_enable&&
        r.depth_control.depth_write_enable)||metadata_clear);
    if(metadata_clear&&Diagnostics::Enabled()){
        static unsigned reported{};
        if(reported++<64)fprintf(stderr,"DX12 HTile clear depth=%llx meta=%llx slice=%u value=%g\n",
            (unsigned long long)r.depth_buffer.DepthAddress(),(unsigned long long)r.depth_htile_data_base.GetAddress(),
            r.depth_view.slice_start,r.depth_clear);
    }
    const bool clear_stencil=depth_tests.stencil&&r.depth_render_control.stencil_clear_enable;
    const bool write_depth_stencil=state.WritesDepthStencil(clear_depth,clear_stencil);
    Resource index_storage;IndexBinding index_view{};
    const bool expand_primitive=GuestPrimitiveNeedsExpansion(r.primitive_type);
    const bool strip=state.topology==D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP||state.topology==D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
    const bool custom_strip_restart=indexed&&r.enable_primitive_restart&&strip&&
        r.primitive_restart_index!=(r.index_buffer_type.index_type==AmdGpu::IndexType::Index16?0xffffu:0xffffffffu);
    const bool rewrite_restart=indexed&&r.enable_primitive_restart&&
        (!strip||custom_strip_restart);
    if(custom_strip_restart){
        // Replacing the guest marker with the fixed D3D strip-cut value also
        // cuts legitimate 0xffff/0xffffffff vertex indices. Expand the strips
        // instead, preserving winding and resetting it at every restart.
        state.topology=state.topology==D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP?
            D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        state.pipeline.IBStripCutValue=D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    }
    if(indexed||expand_primitive) {
        const bool narrow=r.index_buffer_type.index_type==AmdGpu::IndexType::Index16;
        if(indexed&&!narrow&&r.index_buffer_type.index_type!=AmdGpu::IndexType::Index32)
            throw std::runtime_error("Invalid guest index element type");
        std::vector<u32> logical;logical.reserve(draw.count);
        if(indexed){
            const u64 bytes=u64(draw.count)*(narrow?2:4),offset=u64(index_offset)*(narrow?2:4);
            const auto base=r.index_base_address.Address<VAddr>();
            if(bytes>64*1024*1024||base>UINT64_MAX-offset)throw std::out_of_range("Guest index range overflow/budget");
            const auto address=base+offset;ReadMemory(address,bytes);ValidateHostMemory(address,bytes);
            std::vector<std::byte> source(size_t((bytes+3)&~u64(3)),std::byte{});
            memcpy(source.data(),reinterpret_cast<const void*>(address),size_t(bytes));
            const auto mode=r.index_buffer_type.swap_mode;
            if(mode==AmdGpu::IndexSwapMode::Swap16){for(size_t i=0;i+1<bytes;i+=2)std::swap(source[i],source[i+1]);}
            else if(mode==AmdGpu::IndexSwapMode::Swap32){for(size_t i=0;i+3<bytes;i+=4){std::swap(source[i],source[i+3]);std::swap(source[i+1],source[i+2]);}}
            else if(mode==AmdGpu::IndexSwapMode::SwapWord){for(size_t i=0;i+3<bytes;i+=4){std::swap(source[i],source[i+2]);std::swap(source[i+1],source[i+3]);}}
            for(u32 i=0;i<draw.count;++i){u32 value{};if(narrow){u16 v;memcpy(&v,source.data()+i*2,2);value=v;}else memcpy(&value,source.data()+i*4,4);logical.push_back(value);}
        }else for(u32 i=0;i<draw.count;++i)logical.push_back(draw.first+i);
        if(expand_primitive||rewrite_restart)
            logical=ExpandGuestPrimitive(r.primitive_type,logical,indexed&&r.enable_primitive_restart,r.primitive_restart_index);
        if(logical.empty())return;
        const bool output_narrow=!expand_primitive&&indexed&&narrow;
        std::vector<std::byte> converted((logical.size()*(output_narrow?2u:4u)+3)&~size_t(3),std::byte{});
        for(size_t i=0;i<logical.size();++i)if(output_narrow){const auto v=u16(logical[i]);memcpy(converted.data()+i*2,&v,2);}
            else memcpy(converted.data()+i*4,&logical[i],4);
        index_storage=Buffer(device_.Get(),converted.size(),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        void* mapped{};D3D12_RANGE none{};Check(DX12_API_CALL(index_storage.handle->Map(0,&none,&mapped)));
        memcpy(mapped,converted.data(),converted.size());DX12_API_CALL(index_storage.handle->Unmap(0,nullptr));
        index_view={&index_storage,0,UINT(logical.size()*(output_narrow?2u:4u)),output_narrow?DXGI_FORMAT_R16_UINT:DXGI_FORMAT_R32_UINT};
        draw.count=UINT(logical.size());draw.first=0;
        if(!indexed)draw.base_vertex=0;
    }
    diagnostic.Stage("pipeline-creation");
    const ShaderPipeline::StencilFaces stencil{state.stencil_read_mask_front,state.stencil_write_mask_front,
        state.stencil_read_mask_back,state.stencil_write_mask_back,state.separate_stencil_faces};
    const auto& pipeline=pipelines_.Graphics(guest,state.pipeline,state.depth_bounds,stencil);
    if(trace){fprintf(stderr,"DX12 startup draw %u: native pipeline ready\n",traced_draws);fflush(stderr);}
    Shader::PushData push{};const auto& c=r.viewport_control;const auto& v=r.viewports[0];
    push.xoffset=c.xoffset_enable?v.xoffset:0;push.xscale=c.xscale_enable?v.xscale:1;
    push.yoffset=c.yoffset_enable?v.yoffset:0;push.yscale=c.yscale_enable?v.yscale:1;
    GraphicsRuntime runtime{};runtime.first_vertex=indexed?u32(draw.base_vertex):draw.first;
    runtime.base_instance=draw.first_instance;runtime.indexed=indexed;
    runtime.yz_flip_mask=viewport.flip_mask;runtime.viewport_width=viewport.viewports[0].Width;
    runtime.viewport_height=viewport.viewports[0].Height;
    std::array<GuestTextureCache::Entry*,9> targets{};
    std::vector<std::unique_ptr<GuestImageReadback>> readbacks;
    static const auto probe_start=GetTickCount64();
    static unsigned probe_count{};
    std::error_code probe_error;
    const bool probe=GetTickCount64()-probe_start>=30000&&probe_count<512&&
        std::filesystem::exists("scene-probe.enabled",probe_error);
    ComPtr<ID3D12QueryHeap> probe_heap;
    Resource probe_result;
    if(probe){
        D3D12_QUERY_HEAP_DESC query{};query.Type=D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;query.Count=1;
        Check(device_->CreateQueryHeap(&query,IID_PPV_ARGS(&probe_heap)));
        probe_result=Buffer(device_.Get(),sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS),D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    }
    diagnostic.Stage("resource-upload-and-draw");
    ExecuteGuestGraphicsResources(device_.Get(),*this,pipeline,stages,push,runtime,[&](ResourceBindings& bindings) {
        if(index_view.resource)bindings.Retain(index_storage.handle.Get());
        std::array<ColorAttachment,8> attachments{};
        for(UINT i=0;i<target_count;++i)if(formats[i]!=DXGI_FORMAT_UNKNOWN) {
            auto& entry=textures_.Upload(descriptions[i],
                std::span(reinterpret_cast<const std::byte*>(descriptions[i].address),size_t(layouts[i].guest_bytes)),
                RecordingList(),CompletedFence());
            targets[i]=&entry;attachments[i]={&entry.texture,entry.Rtv()};
        }
        DepthAttachment depth{};
        if(has_depth) {
            auto& entry=feedback_depth?*feedback_depth:textures_.Upload(descriptions[8],
                std::span(reinterpret_cast<const std::byte*>(descriptions[8].address),size_t(layouts[8].guest_bytes)),
                RecordingList(),CompletedFence(),stencil_memory);
            // An enabled write overrides the view's read-only hint, just as
            // Vulkan's depth target view derives storage from DepthControl.
            // The hint remains useful for feedback/read-only passes.
            const bool read_only_depth=r.depth_view.z_read_only&&!r.depth_control.depth_write_enable;
            const bool stencil_writes=state.pipeline.DepthStencilState.StencilEnable&&
                (state.stencil_write_mask_front||state.stencil_write_mask_back);
            const bool read_only_stencil=!depth_tests.stencil||
                (r.depth_view.stencil_read_only&&!stencil_writes);
            targets[8]=&entry;depth={&entry.texture,entry.Dsv(read_only_depth,read_only_stencil),
                read_only_depth,read_only_stencil,
                clear_depth,r.depth_clear,clear_stencil,UINT8(r.stencil_clear)};
        }
        for(UINT i=0;i<targets.size();++i)if(targets[i]&&(i!=8||write_depth_stencil))OwnImage(*targets[i]);
        if(probe)RecordingList()->BeginQuery(probe_heap.Get(),D3D12_QUERY_TYPE_PIPELINE_STATISTICS,0);
        ExecuteGraphics(pipeline,bindings,state,std::span(attachments.data(),target_count),has_depth?&depth:nullptr,
            viewport.viewports,viewport.scissors,r.blend_constants,draw,{},index_view.resource?&index_view:nullptr,false);
        if(probe){
            RecordingList()->EndQuery(probe_heap.Get(),D3D12_QUERY_TYPE_PIPELINE_STATISTICS,0);
            RecordingList()->ResolveQueryData(probe_heap.Get(),D3D12_QUERY_TYPE_PIPELINE_STATISTICS,0,1,probe_result.handle.Get(),0);
        }
        for(UINT i=0;i<targets.size();++i)if(targets[i]&&!targets[i]->gpu_owned&&(i!=8||write_depth_stencil))
            readbacks.push_back(std::make_unique<GuestImageReadback>(device_.Get(),*targets[i],RecordingList()));
    },textures_,r.ta_bc_base.Address());
    if(probe){
        Finish();
        void* data{};D3D12_RANGE range{0,sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS)};
        Check(probe_result.handle->Map(0,&range,&data));
        const auto statistics=*static_cast<const D3D12_QUERY_DATA_PIPELINE_STATISTICS*>(data);
        D3D12_RANGE empty{};probe_result.handle->Unmap(0,&empty);
        static FILE* output=[](){std::filesystem::create_directories("user/log");return fopen(("user/log/scene-probe-"+std::to_string(GetCurrentProcessId())+".txt").c_str(),"w");}();
        if(output){
            const auto* vs=guest.stages[u32(Shader::SwStage::Vertex)];
            const auto* ps=guest.stages[u32(Shader::SwStage::Fragment)];
            const auto& vp=viewport.viewports[0];const auto& sc=viewport.scissors[0];
            fprintf(output,"sample=%u indexed=%u count=%u instances=%u vs=%llx ps=%llx cb=%llx targets=%u mrt=%x color_mask=%x depth=%u write=%u func=%u cull=%u flip=%x viewport=%g,%g,%g,%g scissor=%ld,%ld,%ld,%ld IA=%llu primitives=%llu VS=%llu clip_in=%llu clip_out=%llu PS=%llu\n",
                ++probe_count,unsigned(indexed),draw.count,draw.instances,
                (unsigned long long)(vs?vs->info.pgm_hash:0),(unsigned long long)(ps?ps->info.pgm_hash:0),
                (unsigned long long)r.color_buffers[0].Address(),target_count,guest.mrt_mask,r.color_target_mask.raw,
                unsigned(state.pipeline.DepthStencilState.DepthEnable),unsigned(state.pipeline.DepthStencilState.DepthWriteMask),
                unsigned(state.pipeline.DepthStencilState.DepthFunc),unsigned(state.pipeline.RasterizerState.CullMode),viewport.flip_mask,
                vp.TopLeftX,vp.TopLeftY,vp.Width,vp.Height,sc.left,sc.top,sc.right,sc.bottom,
                (unsigned long long)statistics.IAVertices,(unsigned long long)statistics.IAPrimitives,
                (unsigned long long)statistics.VSInvocations,(unsigned long long)statistics.CInvocations,
                (unsigned long long)statistics.CPrimitives,(unsigned long long)statistics.PSInvocations);
            fflush(output);
        }else ++probe_count;
    }
    if(trace){fprintf(stderr,"DX12 startup draw %u: native draw completed\n",traced_draws);fflush(stderr);}
    // Keep game-visible memory coherent until the shared native image cache is
    // integrated with guest memory faults. This path prioritizes correctness.
    diagnostic.Stage("attachment-writeback");
    try {
        if(!readbacks.empty()) {
            const auto fence=Flush();for(auto& rb:readbacks)rb->Submitted(fence);
            RetireGuestAttachmentReadbacks(*this,textures_,fence,targets);
            size_t n{};
            for(UINT i=0;i<targets.size();++i)if(targets[i]&&!targets[i]->gpu_owned&&(i!=8||write_depth_stencil))readbacks[n++]->Resolve(CompletedFence(),
                std::span(reinterpret_cast<std::byte*>(descriptions[i].address),size_t(layouts[i].guest_bytes)),i==8?stencil_memory:std::span<std::byte>{});
            for(UINT i=0;i<targets.size();++i)if(auto* target=targets[i];target&&!target->gpu_owned&&!target->desc.writable&&(i!=8||write_depth_stencil)) {
                const auto* memory=reinterpret_cast<const std::byte*>(target->desc.address);
                target->RememberSnapshot({memory,size_t(target->layout.guest_bytes)},i==8?stencil_memory:std::span<std::byte>{});
            }
        }
    }catch(...){try{Finish();}catch(...){}throw;}
    if(trace){fprintf(stderr,"DX12 startup draw %u: guest memory reconciled\n",traced_draws);fflush(stderr);}
}
void GuestRasterizer::DrawIndirect(bool indexed,VAddr address,u32 offset,u32 stride,
    u32 max_count,VAddr count_address,u16 vertex_sgpr,u16 instance_sgpr) {
    std::lock_guard lock(execution_mutex_);
    if(!max_count)return;
    const u32 command_bytes=indexed?20:16;
    if(stride<command_bytes||stride%4||address>UINT64_MAX-offset)
        throw std::out_of_range("Invalid native guest indirect draw layout");
    Finish();u32 count=max_count;
    if(count_address) {
        ReadMemory(count_address,4);ValidateHostMemory(count_address,4);
        u32 actual{};memcpy(&actual,reinterpret_cast<const void*>(count_address),4);count=std::min(count,actual);
    }
    if(!count)return;
    const u64 bytes=u64(count-1)*stride+command_bytes;const auto start=address+offset;
    if(bytes>64*1024*1024||start>UINT64_MAX-bytes)throw std::out_of_range("Native indirect draw argument budget/overflow");
    ReadMemory(start,bytes);ValidateHostMemory(start,bytes);
    std::vector<std::byte> snapshot(size_t(bytes),std::byte{});memcpy(snapshot.data(),reinterpret_cast<const void*>(start),size_t(bytes));
    for(u32 i=0;i<count;++i) {
        u32 raw[5]{};memcpy(raw,snapshot.data()+u64(i)*stride,command_bytes);
        DrawArguments draw{raw[0],raw[1],indexed?0:raw[2],indexed?raw[4]:raw[3],indexed?s32(raw[3]):0};
        DrawInternal(indexed,indexed?raw[2]:0,&draw,vertex_sgpr,instance_sgpr);
    }
}
bool GuestRasterizer::ComputeImageOperation(const Shader::Info& info,const u32 (&groups)[3]) {
    // Mirror Vulkan's IsComputeImageCopy/Clear recognition, with stricter
    // storage and ownership checks for native DX12 memory coherence.
    const auto& cs=gpu_.GetCsRegs();
    if(cs.num_thread_x.full!=64||groups[1]!=1||groups[2]!=1||
       cs.start_x||cs.start_y||cs.start_z||!info.images.empty())return false;
    // Native translation retains an extra read-only Flatbuf descriptor used
    // to fetch resource metadata. It is not a third guest data buffer.
    const Shader::BufferResource* guest_buffers[2]{};unsigned buffer_count{};
    for(const auto& buffer:info.buffers){
        if(buffer.IsSpecial()){if(buffer.buffer_type!=Shader::BufferType::Flatbuf||buffer.is_written)return false;continue;}
        if(buffer_count==2)return false;guest_buffers[buffer_count++]=&buffer;
    }
    if(buffer_count!=2)return false;
    const auto& a=*guest_buffers[0];const auto& b=*guest_buffers[1];
    const auto ba=a.GetSharp(info),bb=b.GetSharp(info);
    const auto find=[&](u64 address,u64 size,bool owned=false,const GuestTextureCache::Entry* like=nullptr){
        GuestTextureCache::Entry* result{};
        for(auto& entry:textures_.Entries())if(entry->desc.address==address&&entry->layout.guest_bytes==size){
            if((owned&&!entry->gpu_owned)||(like&&!GuestImageCopyCompatible(*like,*entry)))continue;
            if(owned&&result)return static_cast<GuestTextureCache::Entry*>(nullptr);
            if(!result||entry->gpu_owned||(!result->gpu_owned&&entry->last_use>result->last_use))result=entry.get();
        }
        return result;
    };
    bool copied=false;
    if(a.is_formatted&&b.is_formatted&&a.is_written!=b.is_written&&
       ba.GetSize()==bb.GetSize()&&u64(groups[0])*256==ba.GetSize()){
        auto* src=find(a.is_written?bb.base_address:ba.base_address,ba.GetSize(),true);
        if(!src)return false;
        auto* dst=find(a.is_written?ba.base_address:bb.base_address,ba.GetSize(),false,src);
        if(!dst)return false;
        for(auto& entry:textures_.Entries())if(entry.get()!=dst&&entry->gpu_owned&&ImageOverlap(dst->desc.address,dst->layout.guest_bytes,*entry))ResolveImage(*entry);
        if(!OwnImage(*dst))return false;
        if(!WholeGuestImage(*src)){
            // Texture CopyResource copies logical texels, not PS4 allocation
            // padding. Physical backing carries untouched padding even while
            // GPU ownership withholds access to the current logical texels.
            // Seed destination backing from it; later ResolveImage overlays
            // copied GPU texels while retaining the exact source padding.
            std::vector<std::byte> backing(size_t(src->layout.guest_bytes));
            if(!read_backing_(src->desc.address,backing)||!write_backing_(dst->desc.address,backing))
                throw std::runtime_error("Cannot preserve padded guest GPU image copy backing");
        }
        RecordGuestImageCopy(*src,*dst,RecordingList(),true);copied=true;
    }else if(!a.is_formatted&&!a.is_written&&b.is_formatted&&b.is_written&&ba.GetSize()==16&&
        u64(groups[0])*128*(AmdGpu::NumBitsPerBlock(bb.GetDataFmt())/8)==bb.GetSize()){
        if(!ReadMemory(ba.base_address,16))return false;
        ValidateHostMemory(ba.base_address,16);float values[4];memcpy(values,reinterpret_cast<const void*>(ba.base_address),16);
        // Bloodborne's verified whole-allocation zero-clear shader. A CPU
        // dword fill also clears tiled padding, unlike an RTV clear, and avoids
        // dispatch -> full GPU readback -> CPU copy for this constant result.
        const std::array<u32,4> zero{};
        if(info.pgm_hash==0x8b355b5a&&bb.GetSize()<=UINT32_MAX&&bb.GetSize()%4==0&&
           bb.base_address%4==0&&memcmp(values,zero.data(),16)==0){
            ResolveMemory(bb.base_address,bb.GetSize());
            Rasterizer::FillBuffer(bb.base_address,u32(bb.GetSize()),0,false);
            static u64 zero_clears{};
            if(++zero_clears<=8||zero_clears%1024==0){fprintf(stderr,"DX12 native zero clear count=%llu bytes=%llu\n",
                (unsigned long long)zero_clears,(unsigned long long)bb.GetSize());fflush(stderr);}
            return true;
        }
        auto* dst=find(bb.base_address,bb.GetSize());
        if(!dst||dst->layout.bytes_per_pixel*8!=AmdGpu::NumBitsPerBlock(bb.GetDataFmt())||
           !GuestImageClearCompatible(*dst))return false;
        if(!OwnImage(*dst))return false;
        auto heap=RecordGuestImageClear(device_.Get(),*dst,RecordingList(),values);
        RetainUntilFinish([heap=std::move(heap)](u64){});
    }else return false;
    RetainUntilFinish([this](u64 fence){if(fence){textures_.Submitted(fence);textures_.Collect(fence);}});
    static u64 copies{},clears{};const auto count=copied?++copies:++clears;
    if(count<=8||count%1024==0){fprintf(stderr,"DX12 native image %s count=%llu shader=%llx bytes=%llu\n",
        copied?"copy":"clear",(unsigned long long)count,(unsigned long long)info.pgm_hash,
        (unsigned long long)bb.GetSize());fflush(stderr);}
    return true;
}
void GuestRasterizer::Dispatch(const u32 (&groups)[3]) {
    Diagnostics::Scope diagnostic(Diagnostics::Kind::Dispatch,"execution-lock");
    diagnostic.Detail("groups=%u,%u,%u",groups[0],groups[1],groups[2]);
    std::lock_guard lock(execution_mutex_);
    // PS4 zero-dimensional commands perform no work, unlike D3D12's validated
    // nonempty path. Do not translate/read a shader for a no-op command.
    if(!groups[0]||!groups[1]||!groups[2])return;
    const auto& program=gpu_.GetCsRegs();
    ComputeInvocation invocation{{groups[0],groups[1],groups[2]},
        {program.start_x,program.start_y,program.start_z}};
    diagnostic.Stage("shader-translation");
    auto& guest=frontend_.BuildCompute(gpu_);
    if(Diagnostics::Enabled()&&groups[0]>=60000){
        static std::vector<u64> observed;
        if(observed.size()<32&&std::find(observed.begin(),observed.end(),guest.info.pgm_hash)==observed.end()){
            observed.push_back(guest.info.pgm_hash);
            fprintf(stderr,"DX12 compute candidate hash=%llx threads=%u groups=%u buffers=%zu images=%zu\n",
                (unsigned long long)guest.info.pgm_hash,program.num_thread_x.full,groups[0],guest.info.buffers.size(),guest.info.images.size());
            for(const auto& buffer:guest.info.buffers){const auto sharp=buffer.GetSharp(guest.info);
                fprintf(stderr,"DX12 compute buffer special=%u formatted=%u written=%u address=%llx bytes=%llu fmt=%u/%u\n",
                    unsigned(buffer.IsSpecial()),unsigned(buffer.is_formatted),unsigned(buffer.is_written),
                    (unsigned long long)sharp.base_address,(unsigned long long)sharp.GetSize(),unsigned(sharp.GetDataFmt()),unsigned(sharp.GetNumberFmt()));}
            fflush(stderr);
        }
    }
    diagnostic.Detail("groups=%u,%u,%u shader=%llu buffers=%zu images=%zu",groups[0],groups[1],groups[2],
        (unsigned long long)guest.id,guest.info.buffers.size(),guest.info.images.size());
    diagnostic.Stage("image-operation-recognition");
    if(ComputeImageOperation(guest.info,groups))return;
    diagnostic.Stage("copy-shader-hle");
    if(ExecuteGuestCopyShader(*this,guest.info,invocation))return;
    // Same bounded heuristic as Vulkan's IsComputeMetaClear: metadata readers
    // and XOR address encoders must execute normally. Plain metadata clears
    // update the mask; the next depth attachment materializes the clear.
    const bool reads_meta=std::any_of(guest.info.buffers.begin(),guest.info.buffers.end(),[&](const auto& b){
        return !b.IsSpecial()&&!b.is_written&&depth_metadata_.Contains(b.GetSharp(guest.info).base_address);
    });
    diagnostic.Stage("metadata-clear-recognition");
    if(!reads_meta&&!guest.info.has_bitwise_xor){
        bool cleared=false;
        for(const auto& b:guest.info.buffers)if(!b.IsSpecial()&&b.is_written)
            cleared|=depth_metadata_.Clear(b.GetSharp(guest.info).base_address);
        if(cleared)return;
    }
    if(PartialRenderingEnabled()&&
       std::any_of(guest.info.images.begin(),guest.info.images.end(),[](const auto& image){return image.is_written;})&&
       std::all_of(guest.info.images.begin(),guest.info.images.end(),[&](const auto& image){
           if(!image.is_written)return true;
           const auto d=DescribeGuestTexture(image.GetSharp(guest.info));
           // Preserve full-resolution composition and small lookup tables;
           // dropping them also hides menus and prevents visual diagnosis.
           return d.width>=128&&d.height>=128&&
               !(d.width>=1280&&d.width<=2560&&d.height>=720&&d.height<=1440);
       })&&
       std::none_of(guest.info.buffers.begin(),guest.info.buffers.end(),[](const auto& buffer){return buffer.is_written;})){
        // Diagnostic image-effect bypass. Preserve copy shaders and any
        // dispatch that writes buffers (counters/indirect work/game data).
        ApiTrace::Call("DIAGNOSTIC","PartialRender.SkipImageCompute",[]{},guest.id,groups[0],groups[1],groups[2]);
        static uint64_t skipped{};
        if(++skipped==1||skipped%1024==0){fprintf(stderr,"DX12 partial rendering skipped=%llu image-only dispatches\n",(unsigned long long)skipped);fflush(stderr);}
        return;
    }
    const bool scratch=std::any_of(guest.info.buffers.begin(),guest.info.buffers.end(),
        [](const auto& b){return b.buffer_type==Shader::BufferType::SharedMemory;});
    if(scratch) {
        u64 bytes=program.SharedMemSize();
        for(auto dim:groups){if(bytes>64*1024*1024/u64(dim))throw std::out_of_range("Native guest shared-memory dispatch budget");bytes*=dim;}
        invocation.shared_bytes=bytes;
    }
    diagnostic.Stage("pipeline-creation");
    const auto& pipeline=pipelines_.Compute(guest);
    diagnostic.Stage("resource-upload-dispatch-writeback");
    ExecuteGuestBufferCompute(device_.Get(),*this,pipeline,guest.info,guest.bindings_start,invocation,&textures_,
        gpu_.regs.ta_bc_base.Address());
}
void GuestRasterizer::DispatchDirect() {
    const auto& program=gpu_.GetCsRegs();const u32 groups[]{program.dim_x,program.dim_y,program.dim_z};
    try {Dispatch(groups);}
    catch(const std::exception& e){fprintf(stderr,"Native DX12 Dispatch failed: %s\n",e.what());fflush(stderr);throw;}
}
void GuestRasterizer::DispatchIndirect(VAddr address,u32 offset,u32 size) {
    std::lock_guard lock(execution_mutex_);
    if(size<12||address>UINT64_MAX-offset||address+offset>UINT64_MAX-12)
        throw std::out_of_range("Native indirect compute argument range");
    const auto source=address+offset;
    if(source%4||!ReadMemory(source,12))throw std::out_of_range("Native indirect compute arguments unmapped/unaligned");
    // Reuse the memory backend's committed/protection checks before CPU access.
    ValidateHostMemory(source,12);Finish();
    u32 groups[3];memcpy(groups,reinterpret_cast<const void*>(source),sizeof(groups));
    Dispatch(groups);
}
}
