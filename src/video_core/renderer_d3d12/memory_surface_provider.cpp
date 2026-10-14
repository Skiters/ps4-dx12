#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "memory_surface_provider.h"
#include "guest_texture_cache.h"
#include "texture_resources.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
namespace NativeDX12 {
namespace {
bool IsEightBit(Libraries::VideoOut::PixelFormat format) {
    using P=Libraries::VideoOut::PixelFormat;
    return format==P::A8B8G8R8Srgb||format==P::A8R8G8B8Srgb;
}
void Validate(const Libraries::VideoOut::BufferAttributeGroup& g,VAddr address) {
    using namespace Libraries::VideoOut;
    const auto& a=g.attrib;
    if(!address||!a.width||!a.height||a.width>16384||a.height>16384||a.pitch_in_pixel<a.width||a.pitch_in_pixel>32768)
        throw std::out_of_range("Invalid native VideoOut backing dimensions/address");
    if(a.pixel_format!=PixelFormat::A8B8G8R8Srgb&&a.pixel_format!=PixelFormat::A8R8G8B8Srgb&&
       a.pixel_format!=PixelFormat::A2R10G10B10&&a.pixel_format!=PixelFormat::A2R10G10B10Srgb&&
       a.pixel_format!=PixelFormat::A2R10G10B10Bt2020Pq&&a.pixel_format!=PixelFormat::A16R16G16B16Float)
        throw std::runtime_error("Unknown native VideoOut pixel format");
    if(a.tiling_mode!=TilingMode::Linear&&a.tiling_mode!=TilingMode::Tile)
        throw std::runtime_error("Unknown native VideoOut tiling mode");
}
}
MemorySurfaceProvider::MemorySurfaceProvider(ID3D12Device* d,ID3D12CommandQueue* q,Rasterizer& r,UINT w,UINT h)
    :device_(d),queue_(q),renderer_(r),width_(w),height_(h),completion_(d) {
    if(!d||!q||!w||!h||w>16384||h>16384)throw std::invalid_argument("Invalid native display provider");
    contexts_.resize(4);
    for(auto& context:contexts_){
        Check(DX12_API_CALL(d->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&context.allocator))));
        Check(DX12_API_CALL(d->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,context.allocator.Get(),nullptr,IID_PPV_ARGS(&context.commands))));
        Check(DX12_API_CALL(context.commands->Close()));
    }
}
MemorySurfaceProvider::~MemorySurfaceProvider(){try{Drain();}catch(...) {}}
void MemorySurfaceProvider::Drain(){
    u64 last{};for(const auto& context:contexts_)last=std::max(last,context.fence);
    if(last)completion_.Wait(last);
}
MemorySurfaceProvider::Context& MemorySurfaceProvider::AcquireContext(){
    const auto completed=completion_.Completed();
    // Release staging and snapshot references as soon as their copies retire,
    // independently of which allocator is selected next. This keeps the
    // immutable texture pool reusable without waiting for a full ring lap.
    for(auto& context:contexts_)if(context.fence&&context.fence<=completed){
        context.upload={};context.target.reset();
    }
    for(size_t offset=0;offset<contexts_.size();++offset){
        const auto index=(active_+offset)%contexts_.size();
        if(contexts_[index].fence>completed)continue;
        active_=(index+1)%contexts_.size();auto& context=contexts_[index];
        Check(DX12_API_CALL(context.allocator->Reset()));
        Check(DX12_API_CALL(context.commands->Reset(context.allocator.Get(),nullptr)));
        return context;
    }
    auto oldest=std::min_element(contexts_.begin(),contexts_.end(),
        [](const Context& left,const Context& right){return left.fence<right.fence;});
    completion_.Wait(oldest->fence);active_=(size_t(oldest-contexts_.begin())+1)%contexts_.size();
    oldest->upload={};oldest->target.reset();
    Check(DX12_API_CALL(oldest->allocator->Reset()));
    Check(DX12_API_CALL(oldest->commands->Reset(oldest->allocator.Get(),nullptr)));
    return *oldest;
}
void MemorySurfaceProvider::Register(const Libraries::VideoOut::BufferAttributeGroup& g,VAddr a){Validate(g,a);}
GuestSurfaceSnapshot MemorySurfaceProvider::Snapshot(const Libraries::VideoOut::BufferAttributeGroup& group,VAddr address) {
    Diagnostics::Scope diagnostic(Diagnostics::Kind::PrepareFrame,"execution-lock");
    std::lock_guard lock(renderer_.ExecutionMutex());
    using namespace Libraries::VideoOut;
    diagnostic.Stage("buffer-reconciliation");
    Validate(group,address);const auto& a=group.attrib;
    diagnostic.Detail("address=%llx size=%ux%u",(unsigned long long)address,a.width,a.height);
    diagnostic.Stage("cpu-detile-or-coherent-image");
    std::vector<std::byte> source;
    Resource* cached{};
    const UINT bytes_per_pixel=a.pixel_format==PixelFormat::A16R16G16B16Float?8u:4u;
    if(a.tiling_mode==TilingMode::Linear) {
        const u64 bytes=u64(a.pitch_in_pixel)*a.height*bytes_per_pixel;
        // Resolve GPU page ownership on this mutex's owner before any CPU
        // access. Parallel detile workers cannot acquire the owner's mutex
        // from a page fault while Snapshot waits for their completion.
        if(!renderer_.ReadMemory(address,bytes))throw std::runtime_error("Native linear display backing is unmapped");
        renderer_.ValidateHostMemory(address,bytes);source.resize(size_t(a.width)*a.height*bytes_per_pixel);
        for(UINT y=0;y<a.height;++y)memcpy(source.data()+size_t(y)*a.width*bytes_per_pixel,
            reinterpret_cast<const std::byte*>(address)+u64(y)*a.pitch_in_pixel*bytes_per_pixel,size_t(a.width)*bytes_per_pixel);
    }else {
        GuestTextureDesc d;d.address=address;d.width=a.width;d.height=a.height;d.pitch=a.pitch_in_pixel;d.tile_mode=10;
        if(a.pixel_format==PixelFormat::A2R10G10B10||a.pixel_format==PixelFormat::A2R10G10B10Srgb||
           a.pixel_format==PixelFormat::A2R10G10B10Bt2020Pq)d.data_format=9;
        else if(a.pixel_format==PixelFormat::A16R16G16B16Float){d.data_format=12;d.number_format=7;}
        const auto layout=LayoutGuestTexture(d);
        if(IsEightBit(a.pixel_format))cached=renderer_.PrepareDisplayTexture(address,a.width,a.height,a.pitch_in_pixel,queue_.Get());
        if(!cached){
            if(!renderer_.ReadMemory(address,layout.guest_bytes))throw std::runtime_error("Native tiled display backing is unmapped");
            renderer_.ValidateHostMemory(address,layout.guest_bytes);
            source=DecodeGuestSubresource(d,layout,std::span(reinterpret_cast<const std::byte*>(address),size_t(layout.guest_bytes)),0,0);
        }
    }
    static unsigned traced_frames{};
    if(std::getenv("SHADPS4_NATIVE_STARTUP_TRACE")&&(++traced_frames<=5||traced_frames%60==0)){
        fprintf(stderr,"DX12 startup display %u: source=%ux%u target=%ux%u address=%llx path=%s\n",
            traced_frames,a.width,a.height,width_,height_,(unsigned long long)address,cached?"coherent-gpu-copy":"cpu-detile-upload");fflush(stderr);
    }
    diagnostic.Stage("snapshot-allocation-and-upload");
    // Retire completed copy references before inspecting snapshot use counts.
    // Otherwise a free texture still appears busy and the pool grows despite
    // its fence having completed (or reports exhaustion at the pool limit).
    auto& context=AcquireContext();auto* commands=context.commands.Get();
    const bool eight_bit=IsEightBit(a.pixel_format);
    const auto storage_format=eight_bit?DXGI_FORMAT_R8G8B8A8_TYPELESS:
        bytes_per_pixel==8?DXGI_FORMAT_R16G16B16A16_TYPELESS:DXGI_FORMAT_R10G10B10A2_TYPELESS;
    const auto view_format=eight_bit?DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        bytes_per_pixel==8?DXGI_FORMAT_R16G16B16A16_FLOAT:DXGI_FORMAT_R10G10B10A2_UNORM;
    std::shared_ptr<Resource> texture;
    for(auto& candidate:snapshots_)if(candidate.use_count()==1){
        const auto desc=DX12_API_CALL(candidate->handle->GetDesc());
        if(desc.Width!=a.width||desc.Height!=a.height||desc.Format!=storage_format)
            candidate=std::make_shared<Resource>(Texture2D(device_.Get(),a.width,a.height,storage_format,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST));
        texture=candidate;break;
    }
    if(!texture){
        if(snapshots_.size()>=34)throw std::runtime_error("Native immutable display texture pool exhausted");
        texture=std::make_shared<Resource>(Texture2D(device_.Get(),a.width,a.height,storage_format,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST));
        snapshots_.push_back(texture);
    }
    if(cached){
        cached->Transition(commands,D3D12_RESOURCE_STATE_COPY_SOURCE);
        texture->Transition(commands,D3D12_RESOURCE_STATE_COPY_DEST);
        DX12_API_CALL(commands->CopyResource(texture->handle.Get(),cached->handle.Get()));
    }else{
        context.upload=TextureTransfer::Create(device_.Get(),*texture,false);
        context.upload.WriteRows(source.data(),source.size(),u64(a.width)*bytes_per_pixel);context.upload.Upload(commands,*texture);
    }
    diagnostic.Stage("snapshot-gpu-completion");
    context.target=texture;
    Check(DX12_API_CALL(commands->Close()));context.fence=completion_.Submit(queue_.Get(),commands);
    // The display queue waited for the renderer before reading the cached
    // image. Put the reciprocal wait behind this immutable copy so subsequent
    // guest rendering may safely reuse the same non-simultaneous texture.
    if(cached)renderer_.WaitExternalFence(completion_.Fence(),context.fence);
    const bool swap_rb=a.pixel_format!=PixelFormat::A8B8G8R8Srgb;
    const UINT mapping=D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(swap_rb?2:0,1,swap_rb?0:2,
        D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1);
    return {std::move(texture),{},completion_.Fence(),context.fence,view_format,mapping,
        a.pixel_format==PixelFormat::A2R10G10B10Srgb};
}
}
