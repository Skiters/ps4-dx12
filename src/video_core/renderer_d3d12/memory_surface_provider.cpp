#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "memory_surface_provider.h"
#include "guest_texture_cache.h"
#include "texture_resources.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <bit>
#include <cmath>
#include <d3dcompiler.h>
namespace NativeDX12 {
namespace {
bool IsEightBit(Libraries::VideoOut::PixelFormat format) {
    using P=Libraries::VideoOut::PixelFormat;
    return format==P::A8B8G8R8Srgb||format==P::A8R8G8B8Srgb;
}
float HalfToFloat(uint16_t half) {
    const uint32_t sign=uint32_t(half&0x8000)<<16;
    const uint32_t exponent=(half>>10)&31, mantissa=half&1023;
    uint32_t bits{};
    if(!exponent) {
        if(!mantissa)bits=sign;
        else {
            uint32_t value=mantissa,shift{};while(!(value&0x400)){value<<=1;++shift;}
            bits=sign|((127-14-shift)<<23)|((value&0x3ff)<<13);
        }
    }else if(exponent==31)bits=sign|0x7f800000|(mantissa<<13);
    else bits=sign|((exponent+112)<<23)|(mantissa<<13);
    return std::bit_cast<float>(bits);
}
std::vector<std::byte> ConvertDisplay(std::span<const std::byte> source,
    Libraries::VideoOut::PixelFormat format,size_t pixels) {
    using P=Libraries::VideoOut::PixelFormat;
    if(IsEightBit(format))return {source.begin(),source.end()};
    std::vector<std::byte> result(pixels*4);
    if(format==P::A2R10G10B10||format==P::A2R10G10B10Srgb||format==P::A2R10G10B10Bt2020Pq) {
        if(source.size()<pixels*4)throw std::out_of_range("Short native 10-bit display backing");
        for(size_t i=0;i<pixels;++i){uint32_t value;memcpy(&value,source.data()+i*4,4);
            result[i*4+0]=std::byte(((value>>0)&1023)*255/1023);
            result[i*4+1]=std::byte(((value>>10)&1023)*255/1023);
            result[i*4+2]=std::byte(((value>>20)&1023)*255/1023);
            result[i*4+3]=std::byte(((value>>30)&3)*255/3);}
        return result;
    }
    if(format==P::A16R16G16B16Float) {
        if(source.size()<pixels*8)throw std::out_of_range("Short native FP16 display backing");
        for(size_t i=0;i<pixels;++i)for(size_t channel=0;channel<4;++channel){uint16_t half;
            memcpy(&half,source.data()+i*8+channel*2,2);const auto value=std::clamp(HalfToFloat(half),0.f,1.f);
            result[i*4+channel]=std::byte(unsigned(std::lround(value*255.f)));}
        return result;
    }
    throw std::runtime_error("Unknown native VideoOut pixel format");
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
void MemorySurfaceProvider::SwapChannels(Context& context,Resource& source,Resource& target) {
    if(!swap_pipeline_){
        // As with Presenter's fixed scaler, compile only the host display
        // operation. Guest shader translation remains unchanged.
        struct Library{HMODULE handle;~Library(){if(handle)FreeLibrary(handle);}} library{
            LoadLibraryExW(L"d3dcompiler_47.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32)};
        if(!library.handle)throw std::runtime_error("System display shader compiler unavailable");
        auto compile=reinterpret_cast<decltype(&D3DCompile)>(GetProcAddress(library.handle,"D3DCompile"));
        if(!compile)throw std::runtime_error("System display D3DCompile unavailable");
        constexpr char code[]=R"(
Texture2D<float4> source:register(t0);RWTexture2D<float4> target:register(u0);
[numthreads(8,8,1)] void main(uint3 id:SV_DispatchThreadID){uint w,h;target.GetDimensions(w,h);
if(id.x<w&&id.y<h)target[id.xy]=source.Load(int3(id.xy,0)).bgra;}
)";
        ComPtr<ID3DBlob> shader,error,serialized;
        const auto hr=compile(code,sizeof(code)-1,"native-display-channel-swap",nullptr,nullptr,
            "main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&shader,&error);
        if(FAILED(hr))throw std::runtime_error(error?std::string(static_cast<const char*>(error->GetBufferPointer()),error->GetBufferSize()):"Display swap shader compile failed");
        D3D12_DESCRIPTOR_RANGE ranges[]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0},
                                      {D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,1}};
        D3D12_ROOT_PARAMETER parameter{};parameter.ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameter.DescriptorTable={2,ranges};
        const D3D12_ROOT_SIGNATURE_DESC root{1,&parameter,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
        Check(D3D12SerializeRootSignature(&root,D3D_ROOT_SIGNATURE_VERSION_1,&serialized,&error));
        Check(device_->CreateRootSignature(0,serialized->GetBufferPointer(),serialized->GetBufferSize(),IID_PPV_ARGS(&swap_root_)));
        D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline{};pipeline.pRootSignature=swap_root_.Get();
        pipeline.CS={shader->GetBufferPointer(),shader->GetBufferSize()};
        Check(device_->CreateComputePipelineState(&pipeline,IID_PPV_ARGS(&swap_pipeline_)));
    }
    if(!context.swap_views)context.swap_views=std::make_unique<Descriptors>(device_.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,2,true);
    auto* commands=context.commands.Get();
    source.Transition(commands,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    target.Transition(commands,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels=1;device_->CreateShaderResourceView(source.handle.Get(),&srv,context.swap_views->Cpu(0));
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.Format=DXGI_FORMAT_R8G8B8A8_UNORM;uav.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;
    device_->CreateUnorderedAccessView(target.handle.Get(),nullptr,&uav,context.swap_views->Cpu(1));
    ID3D12DescriptorHeap* heaps[]{context.swap_views->Heap()};commands->SetDescriptorHeaps(1,heaps);
    commands->SetComputeRootSignature(swap_root_.Get());commands->SetPipelineState(swap_pipeline_.Get());
    commands->SetComputeRootDescriptorTable(0,context.swap_views->Gpu(0));
    const auto desc=target.handle->GetDesc();commands->Dispatch(UINT((desc.Width+7)/8),(desc.Height+7)/8,1);
    target.UavBarrier(commands);
}
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
    if(!cached)source=ConvertDisplay(source,a.pixel_format,size_t(a.width)*a.height);
    if(a.pixel_format==PixelFormat::A8R8G8B8Srgb)
        for(size_t i=0;i<source.size();i+=4)std::swap(source[i],source[i+2]);
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
    std::shared_ptr<Resource> texture;
    for(auto& candidate:snapshots_)if(candidate.use_count()==1){
        const auto desc=DX12_API_CALL(candidate->handle->GetDesc());
        if(desc.Width!=a.width||desc.Height!=a.height)
            candidate=std::make_shared<Resource>(Texture2D(device_.Get(),a.width,a.height,DXGI_FORMAT_R8G8B8A8_UNORM,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_DEST));
        texture=candidate;break;
    }
    if(!texture){
        if(snapshots_.size()>=34)throw std::runtime_error("Native immutable display texture pool exhausted");
        texture=std::make_shared<Resource>(Texture2D(device_.Get(),a.width,a.height,DXGI_FORMAT_R8G8B8A8_UNORM,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_DEST));
        snapshots_.push_back(texture);
    }
    if(cached&&a.pixel_format==PixelFormat::A8R8G8B8Srgb){
        SwapChannels(context,*cached,*texture);
    }else if(cached){
        cached->Transition(commands,D3D12_RESOURCE_STATE_COPY_SOURCE);
        texture->Transition(commands,D3D12_RESOURCE_STATE_COPY_DEST);
        DX12_API_CALL(commands->CopyResource(texture->handle.Get(),cached->handle.Get()));
    }else{
        context.upload=TextureTransfer::Create(device_.Get(),*texture,false);
        context.upload.WriteRows(source.data(),source.size(),u64(a.width)*4);context.upload.Upload(commands,*texture);
    }
    diagnostic.Stage("snapshot-gpu-completion");
    context.target=texture;
    Check(DX12_API_CALL(commands->Close()));context.fence=completion_.Submit(queue_.Get(),commands);
    // The display queue waited for the renderer before reading the cached
    // image. Put the reciprocal wait behind this immutable copy so subsequent
    // guest rendering may safely reuse the same non-simultaneous texture.
    if(cached)renderer_.WaitExternalFence(completion_.Fence(),context.fence);
    return {std::move(texture),{},completion_.Fence(),context.fence};
}
}
