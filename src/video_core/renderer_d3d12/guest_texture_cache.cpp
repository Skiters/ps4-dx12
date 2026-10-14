#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_texture_cache.h"
#include "guest_bc_decoder.h"
#include "video_core/amdgpu/resource.h"
#include "video_core/amdgpu/regs_color.h"
#include "video_core/amdgpu/regs_depth.h"
#include "video_core/amdgpu/cb_db_extent.h"
#include "video_core/amdgpu/tiling.h"
#include "video_core/guest_image_layout.h"
#include "guest_tile_address.h"
#include "guest_cmask_address.h"
#ifdef SHADPS4_NATIVE_DX12_CORE
#include "core/libraries/kernel/process.h"
#endif
#include <bit>
#include <algorithm>
#include <cstring>
#include <mutex>
#include <map>
#include <execution>
#include <numeric>
#include <thread>
namespace NativeDX12 {
namespace {
bool GuestAltTile(bool enabled){
#ifdef SHADPS4_NATIVE_DX12_CORE
    return enabled&&Libraries::Kernel::sceKernelIsNeoMode();
#else
    return false;
#endif
}
// PS4 number formats reinterpret the same texel bits. Allocate the DXGI
// typeless family, then bind a fully typed UAV for each guest descriptor.
DXGI_FORMAT WritableStorageFormat(uint32_t data,DXGI_FORMAT fallback){
    switch(data){
    case 1:return DXGI_FORMAT_R8_TYPELESS;
    case 2:return DXGI_FORMAT_R16_TYPELESS;
    case 3:return DXGI_FORMAT_R8G8_TYPELESS;
    case 4:return DXGI_FORMAT_R32_TYPELESS;
    case 5:return DXGI_FORMAT_R16G16_TYPELESS;
    case 9:return DXGI_FORMAT_R10G10B10A2_TYPELESS;
    case 10:return DXGI_FORMAT_R8G8B8A8_TYPELESS;
    case 11:return DXGI_FORMAT_R32G32_TYPELESS;
    case 12:return DXGI_FORMAT_R16G16B16A16_TYPELESS;
    case 13:return DXGI_FORMAT_R32G32B32A32_TYPELESS;
    case 14:return DXGI_FORMAT_R32G32B32A32_TYPELESS;
    default:return fallback;
    }
}
std::span<const DXGI_FORMAT> PackedFloatCasts(DXGI_FORMAT format,bool supported){
    static constexpr std::array r11{DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R32_UINT};
    static constexpr std::array rgb9{DXGI_FORMAT_R9G9B9E5_SHAREDEXP,DXGI_FORMAT_R32_UINT};
    if(!supported)return {};
    if(format==DXGI_FORMAT_R11G11B10_FLOAT)return r11;
    if(format==DXGI_FORMAT_R9G9B9E5_SHAREDEXP)return rgb9;
    return {};
}
std::span<const DXGI_FORMAT> ResourceCasts(const Resource& resource){
    return resource.castable_formats?std::span<const DXGI_FORMAT>(*resource.castable_formats):std::span<const DXGI_FORMAT>{};
}
}
void GuestTextureCache::ValidateWritableFormat(DXGI_FORMAT format){
    if(writable_formats_.contains(format))return;
    D3D12_FEATURE_DATA_FORMAT_SUPPORT support{format};
    Check(DX12_API_CALL(device_->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support))));
    if(!(support.Support1&D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW)||
       !(support.Support2&D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD)||!(support.Support2&D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE))
        throw std::runtime_error("Native guest writable format="+std::to_string(format)+" lacks typed UAV load/store");
    writable_formats_.insert(format);
}
GuestTextureCache::GuestTextureCache(ID3D12Device* device,std::wstring converter):device_(device),texture_allocator_(device),converter_(std::move(converter)) {
    ComPtr<IDXGIFactory4> factory;
    if(SUCCEEDED(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)))))
        DX12_API_CALL(factory->EnumAdapterByLuid(DX12_API_CALL(device->GetAdapterLuid()),IID_PPV_ARGS(&budget_adapter_)));
    if(budget_adapter_){
        budget_event_=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        if(!budget_event_||FAILED(DX12_API_CALL(budget_adapter_->RegisterVideoMemoryBudgetChangeNotificationEvent(
            budget_event_,&budget_cookie_)))){
            if(budget_event_)CloseHandle(budget_event_);budget_event_=nullptr;budget_cookie_=0;
        }
    }
    RefreshMemoryBudget();
}
GuestTextureCache::~GuestTextureCache(){
    if(budget_adapter_&&budget_cookie_)budget_adapter_->UnregisterVideoMemoryBudgetChangeNotification(budget_cookie_);
    if(budget_event_)CloseHandle(budget_event_);
}
void GuestTextureCache::Entry::SetGpuOwned(bool value){
    if(!value)linear_download_pending=false;
    if(gpu_owned==value)return;
    if(cache)cache->guest_owned_overlap_ranges_dirty_=true;
    gpu_owned=value;
}
bool GuestTextureCache::HasGuestRangeOverlap(uint64_t address,uint64_t size,bool gpu_owned_only)const{
    if(!size)return false;
    if(address>UINT64_MAX-size)throw std::out_of_range("Guest image overlap range overflow");
    auto& index=gpu_owned_only?guest_owned_overlap_ranges_:guest_overlap_ranges_;
    auto& dirty=gpu_owned_only?guest_owned_overlap_ranges_dirty_:guest_overlap_ranges_dirty_;
    if(dirty){
        std::map<uint64_t,uint64_t> ranges;
        const auto add=[&](uint64_t begin,uint64_t bytes){
            if(!bytes)return;
            if(begin>UINT64_MAX-bytes)throw std::logic_error("Cached guest image extent overflow");
            auto end=begin+bytes;
            auto it=ranges.lower_bound(begin);
            if(it!=ranges.begin()&&std::prev(it)->second>=begin)it=std::prev(it);
            while(it!=ranges.end()&&it->first<=end){begin=std::min(begin,it->first);end=std::max(end,it->second);it=ranges.erase(it);}
            ranges.emplace(begin,end);
        };
        for(const auto& entry:entries_){if(gpu_owned_only&&!entry->gpu_owned)continue;
            add(entry->desc.address,entry->layout.guest_bytes);
            if(entry->desc.stencil_address)add(entry->desc.stencil_address,entry->stencil_layout.guest_bytes);}
        index=std::move(ranges);dirty=false;
    }
    const auto next=index.upper_bound(address);
    return (next!=index.begin()&&std::prev(next)->second>address)||
        (next!=index.end()&&next->first<address+size);
}
void GuestTextureCache::RefreshMemoryBudget() {
    const auto now=GetTickCount64();
    // DXGI signals this event when Windows changes the process budget.  Keep a
    // slow fallback poll for drivers that fail to deliver notifications.
    if(budget_checked_at_&&now-budget_checked_at_<10000&&
       (!budget_event_||WaitForSingleObject(budget_event_,0)!=WAIT_OBJECT_0))return;
    budget_checked_at_=now;
    DXGI_QUERY_VIDEO_MEMORY_INFO memory{};
    if(!budget_adapter_||FAILED(DX12_API_CALL(budget_adapter_->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&memory)))||!memory.Budget)return;
    // The original renderer derives cache pressure from the adapter budget.
    // The cache accounts CPU snapshots and transfer pools as well as GPU
    // images. A fixed 4 GiB cap therefore evicts considerably less than 4 GiB
    // of actual VRAM on large adapters. Follow the current Windows budget,
    // reserving a quarter for buffers/presentation and other native resources.
    const auto budget=std::clamp<uint64_t>(memory.Budget/4*3,64*1024*1024,12ULL*1024*1024*1024);
    if(Diagnostics::Enabled()&&budget!=resident_budget_)fprintf(stderr,
        "DX12 texture budget: cache_bytes=%llu adapter_budget=%llu process_usage=%llu\n",
        (unsigned long long)budget,(unsigned long long)memory.Budget,(unsigned long long)memory.CurrentUsage);
    resident_budget_=budget;
    texture_allocator_.SetBudget(budget);
}
struct GuestTextureCopyPlans {
    struct Plan {
        uint32_t pieces{};
        std::array<std::array<uint32_t,2>,64> pixels{};
        std::vector<uint64_t> bases;
    };
    std::mutex mutex;
    std::map<std::pair<uint32_t,uint32_t>,std::shared_ptr<const Plan>> subresources;
};
// View-only differences do not change physical PS4 storage. Borrow only retired,
// CPU-coherent, single-plane resources; stencil/format reinterpretation needs
// separate plane and typeless-view support and is deliberately excluded here.
bool CompatibleGuestSampledStorage(GuestTextureDesc source,GuestTextureDesc sampled) {
    if(source.writable||sampled.writable||sampled.stencil_address||
       sampled.render_target||sampled.depth_target)return false;
    if(source.stencil_address&&!source.depth_target)return false;
    if(source.render_target&&source.data_format==9&&source.number_format==5)return false;
    source.stencil_address=0;
    source.render_target=source.depth_target=false;
    source.storage_height=sampled.storage_height=0;
    source.storage_slice_bytes=sampled.storage_slice_bytes=0;
    source.pow2_padding=sampled.pow2_padding=false;
    source.first_mip=sampled.first_mip=0;
    source.min_lod=sampled.min_lod=0;
    source.first_layer=sampled.first_layer=0;
    source.view_layers=sampled.view_layers=1;
    source.swizzle=sampled.swizzle={4,5,6,7};
    return source==sampled;
}
bool CompatibleGuestWritableStorage(GuestTextureDesc source,GuestTextureDesc view) {
    if(!source.writable||!view.writable||source.stencil_address||view.stencil_address||
       source.render_target||view.render_target||source.depth_target||view.depth_target)return false;
    source.first_mip=view.first_mip=0;
    source.min_lod=view.min_lod=0;
    source.first_layer=view.first_layer=0;
    source.view_layers=view.view_layers=1;
    // UAVs have no component mapping; guest storage swizzles are lowered in
    // the shader and do not require separate physical images.
    source.swizzle=view.swizzle={4,5,6,7};
    if(source.UsesRaw32()&&view.UsesRaw32()&&
       (source.data_format==4||source.data_format==10)&&(view.data_format==4||view.data_format==10)){
        source.data_format=view.data_format=4;source.number_format=view.number_format=4;
    }
    if(source.UsesRaw32()!=view.UsesRaw32())return false;
    source.native_raw32=view.native_raw32=false;
    if(source.data_format==9&&view.data_format==9){
        if(source.UsesRawPacked10()!=view.UsesRawPacked10())return false;
        source.native_packed10_raw=view.native_packed10_raw=false;
    }
    if(source.data_format==view.data_format&&
       WritableStorageFormat(source.data_format,DXGI_FORMAT_UNKNOWN)!=DXGI_FORMAT_UNKNOWN)
        source.number_format=view.number_format=0;
    return source==view;
}
bool CompatibleGuestShaderStorage(GuestTextureDesc source,GuestTextureDesc view) {
    if(((source.data_format==9&&(source.number_format==1||source.number_format==5||
        view.number_format==1||view.number_format==5))||(source.data_format>=16&&source.data_format<=19))&&
       source.writable!=view.writable)
        return false;
    // A writable member causes the group to allocate typeless UAV-capable
    // storage. A purely sampled group still uses its original typed resource.
    if(!source.writable&&!view.writable&&source.number_format!=view.number_format)return false;
    source.writable=view.writable=true;
    source.swizzle=view.swizzle={4,5,6,7};
    return CompatibleGuestWritableStorage(source,view);
}
bool CompatibleGuestAttachmentStorage(GuestTextureDesc source,GuestTextureDesc view){
    if((!source.render_target&&!source.depth_target)||(!view.render_target&&!view.depth_target))return false;
    source.first_mip=view.first_mip=0;
    source.first_layer=view.first_layer=0;
    source.view_layers=view.view_layers=1;
    return source==view;
}
void MergeGuestColorAttachmentLayers(std::span<GuestTextureDesc> attachments){
    // CB slice_max bounds a view as well as the requested storage. The original
    // cache grows one compatible backing to the maximum requested layer count.
    // Preserve each view's active slices while giving simultaneous views the
    // same allocation description before any resource or descriptor is made.
    for(size_t i=0;i<attachments.size();++i){auto& a=attachments[i];
        if(!a.render_target||a.depth_target)continue;
        for(size_t j=0;j<i;++j){auto& b=attachments[j];
            if(!b.render_target||b.depth_target||a.address!=b.address)continue;
            auto x=a,y=b;const auto layers=std::max(x.layers,y.layers);
            x.layers=y.layers=layers;
            if(CompatibleGuestAttachmentStorage(x,y))a.layers=b.layers=layers;
        }
    }
}
bool CompatibleGuestPackedStorage(GuestTextureDesc source,GuestTextureDesc view){
    const auto packed_number=[](UINT value){return value==0||value==1||value==4||value==5;};
    const bool packed10=source.data_format==9&&view.data_format==9&&
        packed_number(source.number_format)&&packed_number(view.number_format);
    const bool raw32=(source.data_format==4||source.data_format==10)&&
        (view.data_format==4||view.data_format==10)&&
        (source.native_raw32||view.native_raw32||source.data_format!=view.data_format);
    const bool packed16=source.data_format>=16&&source.data_format<=19&&source.data_format==view.data_format&&
        source.number_format==0&&view.number_format==0&&source.writable!=view.writable;
    if((!packed10&&!packed16&&!raw32)||source.depth_target||view.depth_target||source.stencil_address||view.stencil_address)return false;
    const auto a=LayoutGuestTexture(source),b=LayoutGuestTexture(view);
    if(a.guest_bytes!=b.guest_bytes||a.mips.size()!=b.mips.size())return false;
    for(size_t i=0;i<a.mips.size();++i){const auto& x=a.mips[i];const auto& y=b.mips[i];
        if(x.pitch!=y.pitch||x.height!=y.height||x.offset!=y.offset||x.slice_bytes!=y.slice_bytes||
           x.macro!=y.macro||x.thickness!=y.thickness)return false;
    }
    source.writable=view.writable=source.render_target=view.render_target=false;
    if(packed10)source.number_format=view.number_format=0;
    if(raw32){source.data_format=view.data_format=4;source.number_format=view.number_format=4;}
    source.native_packed10_raw=view.native_packed10_raw=false;
    source.native_raw32=view.native_raw32=false;
    source.storage_height=view.storage_height=0;source.storage_slice_bytes=view.storage_slice_bytes=0;
    source.pow2_padding=view.pow2_padding=false;
    source.first_mip=view.first_mip=source.first_layer=view.first_layer=source.min_lod=view.min_lod=0;
    source.view_layers=view.view_layers=1;source.swizzle=view.swizzle={4,5,6,7};
    return source==view;
}
uint64_t GuestTextureCache::Entry::ResidentBytes()const{
    uint64_t bytes=allocation_bytes+expanded_backing_bytes+sampled_snapshot.size()+stencil_snapshot.size();
    bytes+=buffer_sync_allocation_bytes;
    for(const auto& buffer:upload_pool)bytes+=buffer.handle->GetDesc().Width;
    for(const auto& buffer:tiler_input_pool)bytes+=buffer.handle->GetDesc().Width;
    for(const auto& buffer:tiler_output_pool)bytes+=buffer.handle->GetDesc().Width;
    uint64_t readback{};
    if(readback_pool)for(const auto& buffer:*readback_pool)readback+=buffer.handle->GetDesc().Width;
    return bytes+std::max(readback_reserve_bytes,readback);
}
namespace {
uint32_t Align(uint32_t n,uint32_t a){return (n+a-1)&~(a-1);}
uint32_t Pow2(uint32_t n){return std::bit_ceil(n);}
std::pair<DXGI_FORMAT,uint32_t> Format(uint32_t data,uint32_t number){
    if(data==1){switch(number){case 0:return {DXGI_FORMAT_R8_UNORM,1};case 1:return {DXGI_FORMAT_R8_SNORM,1};case 4:return {DXGI_FORMAT_R8_UINT,1};case 5:return {DXGI_FORMAT_R8_SINT,1};}}
    if(data==3){switch(number){case 0:return {DXGI_FORMAT_R8G8_UNORM,2};case 1:return {DXGI_FORMAT_R8G8_SNORM,2};case 4:return {DXGI_FORMAT_R8G8_UINT,2};case 5:return {DXGI_FORMAT_R8G8_SINT,2};}}
    if(data==10){switch(number){case 0:return {DXGI_FORMAT_R8G8B8A8_UNORM,4};case 1:return {DXGI_FORMAT_R8G8B8A8_SNORM,4};case 4:return {DXGI_FORMAT_R8G8B8A8_UINT,4};case 5:return {DXGI_FORMAT_R8G8B8A8_SINT,4};case 9:return {DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,4};}}
    if(data==4){switch(number){case 7:return {DXGI_FORMAT_R32_FLOAT,4};case 4:return {DXGI_FORMAT_R32_UINT,4};case 5:return {DXGI_FORMAT_R32_SINT,4};}}
    if(data==2){switch(number){case 0:return {DXGI_FORMAT_R16_UNORM,2};case 1:return {DXGI_FORMAT_R16_SNORM,2};case 4:return {DXGI_FORMAT_R16_UINT,2};case 5:return {DXGI_FORMAT_R16_SINT,2};case 7:return {DXGI_FORMAT_R16_FLOAT,2};}}
    if(data==5){switch(number){case 0:return {DXGI_FORMAT_R16G16_UNORM,4};case 1:return {DXGI_FORMAT_R16G16_SNORM,4};case 4:return {DXGI_FORMAT_R16G16_UINT,4};case 5:return {DXGI_FORMAT_R16G16_SINT,4};case 7:return {DXGI_FORMAT_R16G16_FLOAT,4};}}
    if(data==6&&number==7)return {DXGI_FORMAT_R11G11B10_FLOAT,4};
    if(data==9){if(number==0)return {DXGI_FORMAT_R10G10B10A2_UNORM,4};if(number==4)return {DXGI_FORMAT_R10G10B10A2_UINT,4};
        if(number==1)return {DXGI_FORMAT_R32G32B32A32_FLOAT,4};if(number==5)return {DXGI_FORMAT_R32G32B32A32_SINT,4};}
    if(data==34&&number==7)return {DXGI_FORMAT_R9G9B9E5_SHAREDEXP,4};
    if(data==11){switch(number){case 4:return {DXGI_FORMAT_R32G32_UINT,8};case 5:return {DXGI_FORMAT_R32G32_SINT,8};case 7:return {DXGI_FORMAT_R32G32_FLOAT,8};}}
    if(data==12){switch(number){case 0:return {DXGI_FORMAT_R16G16B16A16_UNORM,8};case 1:return {DXGI_FORMAT_R16G16B16A16_SNORM,8};case 4:return {DXGI_FORMAT_R16G16B16A16_UINT,8};case 5:return {DXGI_FORMAT_R16G16B16A16_SINT,8};case 7:return {DXGI_FORMAT_R16G16B16A16_FLOAT,8};}}
    if(data==13){switch(number){case 4:return {DXGI_FORMAT_R32G32B32A32_UINT,12};case 5:return {DXGI_FORMAT_R32G32B32A32_SINT,12};case 7:return {DXGI_FORMAT_R32G32B32A32_FLOAT,12};}}
    if(data==14){switch(number){case 4:return {DXGI_FORMAT_R32G32B32A32_UINT,16};case 5:return {DXGI_FORMAT_R32G32B32A32_SINT,16};case 7:return {DXGI_FORMAT_R32G32B32A32_FLOAT,16};}}
    if(number==0){
        if(data==16)return {DXGI_FORMAT_B5G6R5_UNORM,2};
        if(data==17||data==18)return {DXGI_FORMAT_B5G5R5A1_UNORM,2};
        if(data==19)return {DXGI_FORMAT_B4G4R4A4_UNORM,2};
    }
    if(data==35){if(number==0)return {DXGI_FORMAT_BC1_UNORM,8};if(number==9)return {DXGI_FORMAT_BC1_UNORM_SRGB,8};}
    if(data==36){if(number==0)return {DXGI_FORMAT_BC2_UNORM,16};if(number==9)return {DXGI_FORMAT_BC2_UNORM_SRGB,16};}
    if(data==37){if(number==0)return {DXGI_FORMAT_BC3_UNORM,16};if(number==9)return {DXGI_FORMAT_BC3_UNORM_SRGB,16};}
    if(data==38){if(number==0)return {DXGI_FORMAT_BC4_UNORM,8};if(number==1)return {DXGI_FORMAT_BC4_SNORM,8};}
    if(data==39){if(number==0)return {DXGI_FORMAT_BC5_UNORM,16};if(number==1)return {DXGI_FORMAT_BC5_SNORM,16};}
    if(data==40){if(number==0||number==7)return {DXGI_FORMAT_BC6H_UF16,16};if(number==1)return {DXGI_FORMAT_BC6H_SF16,16};}
    if(data==41){if(number==0)return {DXGI_FORMAT_BC7_UNORM,16};if(number==9)return {DXGI_FORMAT_BC7_UNORM_SRGB,16};}
    throw std::runtime_error("unsupported native guest texture format data="+std::to_string(data)+
        " number="+std::to_string(number));
}
std::vector<std::byte> ExpandSignedPacked10(std::span<const std::byte> source,uint32_t number){
    if((number!=1&&number!=5)||source.size()%4||source.size()>SIZE_MAX/4)
        throw std::invalid_argument("Invalid signed packed sampled data");
    std::vector<std::byte> result(source.size()*4);
    for(size_t pixel=0;pixel<source.size()/4;++pixel){
        uint32_t packed;memcpy(&packed,source.data()+pixel*4,4);
        for(uint32_t component=0;component<4;++component){
            const uint32_t bits=component==3?2:10;
            const uint32_t raw=(packed>>(component*10))&((1u<<bits)-1);
            const int32_t value=int32_t(raw)-((raw&(1u<<(bits-1)))?int32_t(1u<<bits):0);
            if(number==5)memcpy(result.data()+pixel*16+component*4,&value,4);
            else {const float normalized=std::max(float(value)/float((1u<<(bits-1))-1),-1.f);
                memcpy(result.data()+pixel*16+component*4,&normalized,4);}
        }
    }
    return result;
}
UINT Mapping(const std::array<uint32_t,4>& s){UINT result=D3D12_SHADER_COMPONENT_MAPPING_ALWAYS_SET_BIT_AVOIDING_ZEROMEM_MISTAKES;for(UINT i=0;i<4;++i){UINT c;if(s[i]<=1)c=s[i]+4;else if(s[i]>=4&&s[i]<=7)c=s[i]-4;else throw std::runtime_error("invalid guest component swizzle");result|=c<<(i*D3D12_SHADER_COMPONENT_MAPPING_SHIFT);}return result;}
}
GuestTextureDesc DescribeGuestTexture(const AmdGpu::Image& image){
    const bool one_dimensional=image.GetBaseType()==AmdGpu::ImageType::Color1D;
    const bool volume=image.type==uint64_t(AmdGpu::ImageType::Color3D);
    const bool cube=image.GetType()==AmdGpu::ImageType::Cube;
    const bool msaa=image.GetType()==AmdGpu::ImageType::Color2DMsaa||image.GetType()==AmdGpu::ImageType::Color2DMsaaArray;
    if(!one_dimensional&&!volume&&!cube&&!msaa&&image.type!=uint64_t(AmdGpu::ImageType::Color2D)&&image.type!=uint64_t(AmdGpu::ImageType::Color2DArray))
        throw std::runtime_error("Native image type="+std::to_string(image.type)+" format="+
            std::to_string(image.data_format)+" address="+std::to_string(image.Address())+
            " dimensions="+std::to_string(image.width+1)+"x"+std::to_string(image.height+1)+
            "x"+std::to_string(image.depth+1)+" tile="+std::to_string(image.tiling_index)+
            " needs native resource/view support");
    GuestTextureDesc d;d.address=image.base_address<<8;d.width=uint32_t(image.width)+1;d.height=uint32_t(image.height)+1;d.pitch=uint32_t(image.pitch)+1;
    d.alt_tile=GuestAltTile(image.alt_tile_mode);d.bank_swizzle=image.GetBankSwizzle();
    d.one_dimensional=one_dimensional;if(one_dimensional)d.height=1;
    if(volume)d.volume_depth=uint32_t(image.depth)+1;
    d.levels=image.NumLevels();d.samples=image.NumSamples();
    d.min_lod=uint32_t(image.min_lod);
    d.layers=image.NumLayers();
    d.pow2_padding=image.pow2pad;d.data_format=uint32_t(image.GetDataFmt());d.number_format=uint32_t(image.GetNumberFmt());d.tile_mode=uint32_t(image.tiling_index);
    d.first_mip=msaa?0:uint32_t(image.base_level);d.first_layer=std::min(uint32_t(image.base_array),d.layers-1);d.view_layers=image.NumViewLayers(true);
    if(volume){d.first_layer=0;d.view_layers=1;}
    if(d.pow2_padding)d.layers=Pow2(d.layers);
    const auto mapping=image.DstSelect();
    d.swizzle={uint32_t(mapping.r),uint32_t(mapping.g),uint32_t(mapping.b),uint32_t(mapping.a)};
    LayoutGuestTexture(d);return d;
}
GuestTextureDesc DescribeGuestColorBuffer(const AmdGpu::ColorBuffer& buffer,AmdGpu::CbDbExtent extent) {
    if(!buffer||buffer.info.compression||buffer.info.dcc_enable)
        throw std::runtime_error("Native guest color buffer needs valid uncompressed tiling: address="+
            std::to_string(buffer.Address())+" samples="+std::to_string(buffer.NumSamples())+
            " compression="+std::to_string(buffer.info.compression)+" dcc="+std::to_string(buffer.info.dcc_enable)+
            " alt="+std::to_string(buffer.info.alt_tile_mode)+" linear="+std::to_string(buffer.info.linear_general)+
            " fast_clear="+std::to_string(buffer.info.fast_clear)+" cmask="+std::to_string(buffer.CmaskAddress())+
            " format="+std::to_string(buffer.info.format)+" number="+std::to_string(buffer.info.number_type));
    GuestTextureDesc desc;desc.address=buffer.Address();desc.width=extent.Valid()?extent.width:buffer.Pitch();
    desc.height=extent.Valid()?extent.height:buffer.Height();desc.pitch=buffer.Pitch();desc.layers=buffer.NumSlices();
    desc.storage_height=buffer.Height();
    desc.storage_slice_bytes=buffer.GetColorSliceSize();
    desc.samples=buffer.NumSamples();
    desc.alt_tile=GuestAltTile(buffer.info.alt_tile_mode);
    desc.first_layer=buffer.BaseSlice();
    if(desc.first_layer>=desc.layers)throw std::out_of_range("Invalid guest color buffer layer view");
    desc.view_layers=desc.layers-desc.first_layer;desc.data_format=uint32_t(buffer.GetDataFmt());
    desc.number_format=uint32_t(buffer.GetNumberFmt());desc.tile_mode=uint32_t(buffer.GetTileMode());desc.render_target=true;
    const auto layout=LayoutGuestTexture(desc);
    if(layout.guest_bytes!=uint64_t(buffer.slice.tile_max+1)*64*layout.bytes_per_pixel*desc.layers*desc.samples)
        throw std::runtime_error("Native guest color buffer padded storage extent needs explicit layout");
    return desc;
}
// Uncompressed GCN CMASK: 0 selects CB_CLEAR_WORD, F selects expanded color.
uint64_t GuestCmaskBytes(const AmdGpu::ColorBuffer& b) {
    return (uint64_t(b.cmask_slice.tile_max)+1)*128*b.NumSlices();
}
namespace {
struct CmaskTile { uint32_t layer,x,y; uint64_t byte; uint32_t shift; };
struct CmaskClearPlan {
    std::vector<uint32_t> uniform_layers;
    std::vector<CmaskTile> tiles;
    bool Pending() const {return !uniform_layers.empty()||!tiles.empty();}
};
struct CmaskLayout {
    GuestCmaskAddress address;
    explicit CmaskLayout(const AmdGpu::ColorBuffer& b,bool alt_tile)
        : address(b.Pitch(),b.Height(),alt_tile?AmdGpu::GetAltPipeConfig(b.GetTileMode()):
            AmdGpu::GetPipeConfig(b.GetTileMode()),b.info.cmask_is_linear) {
        if(b.info.cmask_addr_type)
            throw std::runtime_error("Native TC-compatible CMASK requires FMASK-derived addressing");
        if(address.slice_bytes!=(uint64_t(b.cmask_slice.tile_max)+1)*128)
            throw std::runtime_error("Native CMASK descriptor does not match padded metadata slice");
    }
    CmaskTile At(uint32_t layer,uint32_t x,uint32_t y) const {
        const auto [byte,shift]=address.At(layer,x,y);
        return {layer,x,y,byte,shift};
    }
};
CmaskClearPlan PlanGuestColorClear(const AmdGpu::ColorBuffer& b,const GuestTextureDesc& d,
    const GuestTextureLayout& l,std::span<const std::byte> cmask) {
    CmaskClearPlan plan;
    if(!b.info.fast_clear||!b.cmask_base_address)return plan;
    if(b.info.compression||b.info.dcc_enable||l.block_extent!=1)
        throw std::runtime_error("Native CMASK requires uncompressed color");
    if(cmask.size()<GuestCmaskBytes(b)||d.first_layer>=b.NumSlices()||
       !d.view_layers||d.view_layers>b.NumSlices()-d.first_layer)
        throw std::out_of_range("Native CMASK storage/view too short");
    const auto slice_bytes=GuestCmaskBytes(b)/b.NumSlices();
    for(uint32_t layer=d.first_layer;layer<d.first_layer+d.view_layers;++layer) {
        const auto slice=cmask.subspan(size_t(slice_bytes*layer),size_t(slice_bytes));
        if(std::all_of(slice.begin(),slice.end(),[](auto v){return v==std::byte{0xff};}))continue;
        if(std::all_of(slice.begin(),slice.end(),[](auto v){return v==std::byte{0};})) {
            plan.uniform_layers.push_back(layer);continue;
        }
        const CmaskLayout layout(b,d.alt_tile);
        for(uint32_t y=0;y<d.height;y+=8)for(uint32_t x=0;x<d.width;x+=8) {
            const auto tile=layout.At(layer,x,y);
            const auto code=(std::to_integer<unsigned>(cmask[tile.byte])>>tile.shift)&15;
            if(code!=0&&code!=15)
                throw std::runtime_error("Native single-sample CMASK unsupported tile code="+std::to_string(code)+
                    " address="+std::to_string(b.CmaskAddress())+" x="+std::to_string(x)+" y="+std::to_string(y));
            if(code==0)plan.tiles.push_back(tile);
        }
    }
    return plan;
}
}
bool GuestColorClearPending(const AmdGpu::ColorBuffer& b,const GuestTextureDesc& d,
    const GuestTextureLayout& l,std::span<const std::byte> cmask) {
    return PlanGuestColorClear(b,d,l,cmask).Pending();
}
bool MaterializeGuestColorClear(const AmdGpu::ColorBuffer& b,const GuestTextureDesc& d,
    const GuestTextureLayout& l,std::span<std::byte> color,std::span<std::byte> cmask) {
    // Validate every selected layer and tile before modifying either plane.
    const auto plan=PlanGuestColorClear(b,d,l,cmask);
    if(!plan.Pending())return false;
    if(color.size()<l.guest_bytes)throw std::out_of_range("Native CMASK color storage span too short");
    const uint32_t words[]{b.clear_word0,b.clear_word1};
    const uint32_t wide[]{b.clear_word0,b.clear_word0,b.clear_word0,b.clear_word1};
    const auto* pixel=reinterpret_cast<const std::byte*>(l.bytes_per_pixel>=12?wide:words);
    const auto slice_bytes=GuestCmaskBytes(b)/b.NumSlices();
    if(!plan.uniform_layers.empty()) {
        std::vector<std::byte> linear(size_t(d.width)*d.height*d.samples*l.bytes_per_pixel);
        for(size_t offset=0;offset<linear.size();offset+=l.bytes_per_pixel)memcpy(linear.data()+offset,pixel,l.bytes_per_pixel);
        for(auto layer:plan.uniform_layers) {
            EncodeGuestSubresource(d,l,color,linear,0,layer);
            std::fill_n(cmask.begin()+size_t(slice_bytes*layer),size_t(slice_bytes),std::byte{0xff});
        }
    }
    for(const auto& tile:plan.tiles) {
        for(uint32_t y=tile.y;y<std::min(tile.y+8,d.height);++y)
        for(uint32_t x=tile.x;x<std::min(tile.x+8,d.width);++x)
        for(uint32_t sample=0;sample<d.samples;++sample) {
            const auto offset=GuestPixelOffset(d,l,0,tile.layer,x,y,sample);
            memcpy(color.data()+offset,pixel,l.bytes_per_pixel);
        }
        cmask[tile.byte]|=std::byte(15u<<tile.shift);
    }
    return true;
}
GuestTextureLayout LayoutGuestTexture(const GuestTextureDesc& d){
    if(!std::has_single_bit(d.samples)||d.samples>16)
        throw std::invalid_argument("Invalid guest sample count");
    if(d.samples>1&&(d.levels!=1||d.volume_depth||d.one_dimensional||d.data_format>=35))
        throw std::invalid_argument("Invalid multisample guest image dimensions/format");
    if(d.volume_depth&&(d.width>2048||d.height>2048||d.volume_depth>2048||d.layers!=1||d.one_dimensional||d.render_target||d.depth_target))
        throw std::invalid_argument("Invalid native 3D texture dimensions/usage");
    if(d.one_dimensional&&(d.height!=1||d.render_target||d.depth_target))
        throw std::invalid_argument("Invalid native 1D texture extent/attachment usage");
    const auto spatial_extent=std::max({d.width,d.height,d.volume_depth});
    if(!d.width||!d.height||d.width>16384||d.height>16384||d.pitch<d.width||d.pitch>32768||!d.layers||d.layers>2048||!d.levels||d.levels>15||d.levels>uint32_t(std::bit_width(spatial_extent)))
        throw std::runtime_error("Invalid guest dimensions/mips/layers");
    if(d.storage_height&&(d.storage_height<d.height||d.storage_height>32768||d.levels!=1))
        throw std::out_of_range("Invalid guest attachment physical height");
    if(d.tile_mode>26&&d.tile_mode!=31)throw std::invalid_argument("Invalid guest tile mode");
    if(d.first_mip>=d.levels||d.first_layer>=d.layers||!d.view_layers||d.view_layers>d.layers-d.first_layer)
        throw std::runtime_error("Invalid guest texture view range");
    Mapping(d.swizzle);
    const bool signed_packed10=d.data_format==9&&(d.number_format==1||d.number_format==5);
    if(signed_packed10&&(d.depth_target||(d.render_target&&d.number_format==1)))
        throw std::invalid_argument("Signed packed 10-bit attachment storage requires exact native packing");
    const auto [format,bpp]=Format(d.data_format,d.writable&&d.number_format==9?0:d.number_format);
    GuestTextureLayout layout;layout.format=format;layout.bytes_per_pixel=bpp;
    if(signed_packed10&&!d.writable&&!d.render_target)layout.expanded_bytes_per_pixel=16;
    // B4G4R4A4 does not provide portable MSAA render-target support. Sampled
    // images use the existing exact GPU packed expansion; attachment writes
    // still require their own guest quantization/blending implementation.
    if(d.data_format>=16&&d.data_format<=19&&
       (d.volume_depth||d.one_dimensional||(d.data_format==19&&d.samples>1&&!d.render_target))&&
       !d.writable&&d.number_format==0){
        layout.expanded_bytes_per_pixel=16;layout.format=DXGI_FORMAT_R32G32B32A32_FLOAT;
    }
    if(d.data_format==9&&d.number_format==5&&d.render_target)layout.format=DXGI_FORMAT_R10G10B10A2_UINT;
    if(d.UsesRaw32()||(d.writable&&d.data_format>=16&&d.data_format<=19))layout.format=DXGI_FORMAT_R32_UINT;
    if(d.data_format>=35&&d.data_format<=41){
        if(d.writable||d.render_target||d.depth_target)
            throw std::invalid_argument("Native BC texture requires a sampled image");
        layout.block_extent=4;
        if(d.volume_depth||d.one_dimensional||(d.width%4)||(d.height%4)){
            layout.expanded_bytes_per_pixel=d.data_format>=38&&d.data_format<=40?16:4;
            layout.format=layout.expanded_bytes_per_pixel==16?DXGI_FORMAT_R32G32B32A32_FLOAT:
                d.number_format==9?DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:DXGI_FORMAT_R8G8B8A8_UNORM;
        }
    }
    const auto mode=AmdGpu::TileMode(d.tile_mode);
    const auto array=AmdGpu::GetArrayMode(mode);
    const bool macro_image=AmdGpu::IsMacroTiled(array);
    const auto thickness=AmdGpu::GetMicroTileThickness(array);
    if(d.storage_slice_bytes&&d.levels!=1)throw std::invalid_argument("Explicit CB/DB allocation must have one mip");
    for(uint32_t mip=0;mip<d.levels;++mip){
        uint32_t pitch=std::max(d.pitch>>mip,1u),height=std::max((d.storage_height?d.storage_height:d.height)>>mip,1u);
        pitch=(pitch+layout.block_extent-1)/layout.block_extent;
        height=(height+layout.block_extent-1)/layout.block_extent;
        if(d.pow2_padding){pitch=Pow2(pitch);height=Pow2(height);}
        bool macro=false;uint32_t mip_thickness=1;
        if(d.storage_slice_bytes){
            // The pinned ImageInfo attachment constructors retain the declared
            // CB/DB pitch, height and slice size instead of recomputing a
            // sampled-image allocation from the active render area.
            macro=macro_image;mip_thickness=thickness;
        }else if(array==AmdGpu::ArrayMode::ArrayLinearGeneral){
            // Native VideoOut already uses linear-general images; retain their
            // declared stride instead of forcing the upstream UpdateSize assert.
        }else if(array==AmdGpu::ArrayMode::ArrayLinearAligned){
            const auto pa=std::max(8u,64u/bpp),sa=std::max(64u,256u/bpp);
            pitch=Align(pitch,pa);
            while((uint64_t(pitch)*height*d.samples)%sa)pitch+=pa;
        }else{
            uint32_t mw=8,mh=8;
            if(macro_image){
                if(d.samples<=8)std::tie(mw,mh)=VideoCore::GetMacroTileExtents(mode,bpp*8,d.samples,d.alt_tile);
                else{
                    const auto mt=AmdGpu::CalculateMacrotileMode(mode,bpp*8,d.samples);
                    const auto banks=d.alt_tile?AmdGpu::GetAltNumBanks(mt):AmdGpu::GetNumBanks(mt);
                    const auto bh=d.alt_tile?AmdGpu::GetAltBankHeight(mt):AmdGpu::GetBankHeight(mt);
                    const auto aspect=d.alt_tile?AmdGpu::GetAltMacrotileAspect(mt):AmdGpu::GetMacrotileAspect(mt);
                    const auto pipes=AmdGpu::GetPipeCount(d.alt_tile?AmdGpu::GetAltPipeConfig(mode):AmdGpu::GetPipeConfig(mode));
                    mw=8*pipes*AmdGpu::GetBankWidth(mt)*aspect;mh=8*banks*bh/aspect;
                }
                if(!mw||!mh)throw std::logic_error("Invalid guest macro tile extents");
                macro=mip==0||(pitch>=mw&&height>=mh);
            }
            mip_thickness=macro?thickness:std::min(thickness,4u);
            pitch=Align(pitch,macro?mw:8);height=Align(height,macro?mh:8);
            if(!macro)while((uint64_t(pitch)*height*bpp*d.samples*mip_thickness)%256)pitch+=8;
        }
        uint32_t depth=d.volume_depth?std::max(d.volume_depth>>mip,1u):1;
        if(d.pow2_padding)depth=Pow2(depth);
        const auto slices=Align(depth*d.layers,mip_thickness);
        const uint64_t minimum_slice=uint64_t(pitch)*height*bpp*d.samples;
        const uint64_t slice=d.storage_slice_bytes?d.storage_slice_bytes:minimum_slice;
        if(slice<minimum_slice)throw std::invalid_argument("CB/DB slice is shorter than its physical extent");
        layout.mips.push_back({pitch,height,layout.guest_bytes,slice,macro,mip_thickness});
        layout.guest_bytes+=slice*slices;
        layout.logical_bytes+=uint64_t((std::max(d.width>>mip,1u)+layout.block_extent-1)/layout.block_extent)*
            ((std::max(d.height>>mip,1u)+layout.block_extent-1)/layout.block_extent)*bpp*
            (d.volume_depth?std::max(d.volume_depth>>mip,1u):d.layers)*d.samples;
    }
    if(d.address>UINT64_MAX-layout.guest_bytes)throw std::out_of_range("Guest texture address overflow");
    layout.copy_plans=std::make_shared<GuestTextureCopyPlans>();return layout;
}
GuestTextureDesc DescribeGuestDepthBuffer(const AmdGpu::DepthBuffer& buffer,
    const AmdGpu::DepthView& view,AmdGpu::CbDbExtent extent) {
    if(!buffer.DepthValid()||buffer.z_info.tile_mode_index>7||view.slice_start>=view.NumSlices())
        throw std::runtime_error("Native guest depth needs valid depth tiling");
    GuestTextureDesc desc;desc.address=buffer.DepthAddress();desc.width=extent.Valid()?extent.width:buffer.Pitch();
    desc.height=extent.Valid()?extent.height:buffer.Height();desc.pitch=buffer.Pitch();desc.layers=view.NumSlices();
    desc.storage_height=buffer.Height();
    desc.storage_slice_bytes=buffer.GetDepthSliceSize();
    desc.samples=buffer.NumSamples();
    desc.first_layer=view.slice_start;desc.view_layers=desc.layers-desc.first_layer;desc.depth_target=true;
    if(buffer.StencilValid())desc.stencil_address=buffer.StencilAddress();
    desc.tile_mode=buffer.z_info.tile_mode_index;
    if(buffer.z_info.format==AmdGpu::DepthBuffer::ZFormat::Z32Float){desc.data_format=4;desc.number_format=7;}
    else if(buffer.z_info.format==AmdGpu::DepthBuffer::ZFormat::Z16){desc.data_format=2;desc.number_format=0;}
    else throw std::runtime_error("Unsupported native guest depth format");
    const auto layout=LayoutGuestTexture(desc);
    if(layout.guest_bytes!=u64(buffer.depth_slice.tile_max+1)*64*layout.bytes_per_pixel*desc.layers*desc.samples)
        throw std::runtime_error("Native guest depth allocation differs from declared slice stride");
    return desc;
}
GuestTextureDesc DescribeGuestStencil(const GuestTextureDesc& depth) {
    if(!depth.depth_target||!depth.stencil_address)throw std::invalid_argument("Missing guest stencil plane");
    auto stencil=depth;stencil.address=depth.stencil_address;stencil.stencil_address=0;
    stencil.depth_target=false;stencil.data_format=1;stencil.number_format=4;
    if(stencil.storage_slice_bytes)stencil.storage_slice_bytes/=Format(depth.data_format,depth.number_format).second;
    return stencil;
}
uint64_t GuestPixelOffset(const GuestTextureDesc& d,const GuestTextureLayout& l,uint32_t mip,uint32_t layer,uint32_t x,uint32_t y,uint32_t sample){
    if(sample>=d.samples||mip>=d.levels||layer>=(d.volume_depth?std::max(d.volume_depth>>mip,1u):d.layers)||
       x>=(std::max(d.width>>mip,1u)+l.block_extent-1)/l.block_extent||
       y>=(std::max(d.height>>mip,1u)+l.block_extent-1)/l.block_extent)
        throw std::out_of_range("Guest pixel/block coordinates");
    const auto& m=l.mips.at(mip);
    if(d.tile_mode==8||d.tile_mode==31)
        return m.offset+uint64_t(layer)*m.slice_bytes+(uint64_t(sample)*m.pitch*m.height+uint64_t(y)*m.pitch+x)*l.bytes_per_pixel;
    const GuestTileAddress address(AmdGpu::TileMode(d.tile_mode),l.bytes_per_pixel,d.samples);
    const auto offset=m.macro?address.Macro(x,y,layer,m.pitch,m.height,sample,d.bank_swizzle):
        address.Micro(x,y,layer,m.pitch,m.height,sample,m.thickness);
    const auto indexed=m.offset+offset;
    // Upstream's scalar-layout u32vec3 buffer indexes the complete tiled byte offset.
    return l.bytes_per_pixel==12?(indexed/12)*12:indexed;
}
namespace {
void CopyTexel(std::byte* dest,const std::byte* source,uint32_t bytes){
    switch(bytes){case 1:*dest=*source;break;case 2:memcpy(dest,source,2);break;case 4:memcpy(dest,source,4);break;
        case 8:memcpy(dest,source,8);break;case 12:memcpy(dest,source,12);break;case 16:memcpy(dest,source,16);break;default:throw std::logic_error("Invalid texel/block size");}
}
template<size_t Bytes,bool Encode>
void CopyPlannedTiles(const GuestTextureCopyPlans::Plan& plan,uint32_t width,uint32_t height,
    const std::byte* source,std::byte* dest){
    const auto rows=(height+7)/8,columns=(width+7)/8;
    static const unsigned workers=std::min(14u,std::max(1u,std::thread::hardware_concurrency()/2>2?
        std::thread::hardware_concurrency()/2-2:1u));
    const auto jobs=uint64_t(width)*height*Bytes>=1024*1024?std::min(workers,rows):1u;
    std::array<unsigned,14> indices{};std::iota(indices.begin(),indices.end(),0u);
    auto copy_rows=[&](unsigned job){
    for(uint32_t tile_row=rows*job/jobs;tile_row<rows*(job+1)/jobs;++tile_row){
    const auto ty=tile_row*8;
    for(uint32_t tx=0;tx<width;tx+=8){
        const auto tile=size_t(tile_row)*columns+tx/8;
        const auto* bases=plan.bases.data()+tile*plan.pieces;
        for(uint32_t y=0;y<std::min(8u,height-ty);++y){
            const auto row=(size_t(ty+y)*width+tx)*Bytes;
            for(uint32_t x=0;x<std::min(8u,width-tx);++x){
                const auto& pixel=plan.pixels[y*8+x];
                const auto guest=size_t(bases[pixel[0]])+pixel[1],linear=row+x*Bytes;
                if constexpr(Encode)memcpy(dest+guest,source+linear,Bytes);
                else memcpy(dest+linear,source+guest,Bytes);
            }
        }
    }
    }
    };
    // Only disjoint texels are written; layout, padding and mip/layer offsets
    // remain identical. Use the persistent Windows STL pool for large surfaces.
    if(jobs==1)copy_rows(0);
    else std::for_each(std::execution::par,indices.begin(),indices.begin()+jobs,copy_rows);
}
void CopyGuestSubresource(const GuestTextureDesc& d,const GuestTextureLayout& l,uint32_t mip,uint32_t layer,
    const std::byte* source,std::byte* dest,bool encode){
    const auto width=(std::max(d.width>>mip,1u)+l.block_extent-1)/l.block_extent;
    const auto height=(std::max(d.height>>mip,1u)+l.block_extent-1)/l.block_extent;
    const auto bpp=l.bytes_per_pixel;const auto& ml=l.mips.at(mip);
    if(d.samples>1){
        for(uint32_t sample=0;sample<d.samples;++sample)
        for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x){
            const auto guest=GuestPixelOffset(d,l,mip,layer,x,y,sample);
            const auto linear=((uint64_t(sample)*height+y)*width+x)*bpp;
            if(guest+bpp>l.guest_bytes)throw std::out_of_range("Guest MSAA texel exceeds backing");
            CopyTexel(dest+(encode?guest:linear),source+(encode?linear:guest),bpp);
        }
        return;
    }
    if(d.tile_mode==8||d.tile_mode==31){
        for(uint32_t y=0;y<height;++y){const auto guest=ml.offset+layer*ml.slice_bytes+uint64_t(y)*ml.pitch*bpp;
            const auto linear=uint64_t(y)*width*bpp;
            if(guest+uint64_t(width)*bpp>l.guest_bytes)throw std::out_of_range("Linear guest row exceeds backing");
            memcpy(dest+(encode?guest:linear),source+(encode?linear:guest),size_t(width)*bpp);}
        return;
    }
    if(ml.thickness>1||bpp==12){
        for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x){
            const auto guest=GuestPixelOffset(d,l,mip,layer,x,y),linear=(uint64_t(y)*width+x)*bpp;
            if(guest+bpp>l.guest_bytes)throw std::out_of_range("Guest volume texel exceeds backing");
            CopyTexel(dest+(encode?guest:linear),source+(encode?linear:guest),bpp);}
        return;
    }
    GuestTextureDesc tile;tile.width=tile.height=tile.pitch=8;tile.tile_mode=d.tile_mode;
    GuestTextureLayout local;local.bytes_per_pixel=bpp;local.mips.push_back({8,8,0,uint64_t(64)*bpp,false});
    std::array<uint32_t,64> offsets{};
    for(uint32_t y=0;y<8;++y)for(uint32_t x=0;x<8;++x)offsets[y*8+x]=uint32_t(GuestPixelOffset(tile,local,0,0,x,y));
    // A piece cannot exceed the microtile itself. R8/R16 microtiles occupy
    // 64/128 bytes even when the macro tile split is 256 bytes.
    const auto chunk=ml.macro?std::min({64*bpp,256u,GuestTileAddress(AmdGpu::TileMode(d.tile_mode),bpp,d.samples).split}):64*bpp;
    auto build_plan=[&]{
        auto plan=std::make_shared<GuestTextureCopyPlans::Plan>();plan->pieces=(64*bpp+chunk-1)/chunk;
        for(uint32_t i=0;i<64;++i)plan->pixels[i]={offsets[i]/chunk,offsets[i]%chunk};
        plan->bases.resize(size_t((width+7)/8)*((height+7)/8)*plan->pieces);
        size_t tile_index{};
        for(uint32_t ty=0;ty<height;ty+=8)for(uint32_t tx=0;tx<width;tx+=8,++tile_index){
            std::array<bool,16> ready{};
            for(uint32_t y=0;y<std::min(8u,height-ty);++y)for(uint32_t x=0;x<std::min(8u,width-tx);++x){
                const auto& pixel=plan->pixels[y*8+x];const auto piece=pixel[0],within=pixel[1];
                if(!ready[piece]){
                    const auto base=GuestPixelOffset(d,l,mip,layer,tx+x,ty+y)-within;
                    if(base>l.guest_bytes||chunk>l.guest_bytes-base)throw std::out_of_range("Guest tile exceeds backing");
                    plan->bases[tile_index*plan->pieces+piece]=base;ready[piece]=true;
                }
            }
        }
        return plan;
    };
    std::shared_ptr<const GuestTextureCopyPlans::Plan> plan;
    if(l.copy_plans){
        std::lock_guard lock(l.copy_plans->mutex);
        auto& stored=l.copy_plans->subresources[{mip,layer}];if(!stored)stored=build_plan();plan=stored;
    }else plan=build_plan();
    switch(bpp){
    case 1:if(encode)CopyPlannedTiles<1,true>(*plan,width,height,source,dest);else CopyPlannedTiles<1,false>(*plan,width,height,source,dest);break;
    case 2:if(encode)CopyPlannedTiles<2,true>(*plan,width,height,source,dest);else CopyPlannedTiles<2,false>(*plan,width,height,source,dest);break;
    case 4:if(encode)CopyPlannedTiles<4,true>(*plan,width,height,source,dest);else CopyPlannedTiles<4,false>(*plan,width,height,source,dest);break;
    case 8:if(encode)CopyPlannedTiles<8,true>(*plan,width,height,source,dest);else CopyPlannedTiles<8,false>(*plan,width,height,source,dest);break;
    case 16:if(encode)CopyPlannedTiles<16,true>(*plan,width,height,source,dest);else CopyPlannedTiles<16,false>(*plan,width,height,source,dest);break;
    default:throw std::logic_error("Invalid planned texel/block size");
    }
}
}
std::vector<std::byte> DecodeGuestSubresource(const GuestTextureDesc& d,const GuestTextureLayout& l,std::span<const std::byte> bytes,uint32_t mip,uint32_t layer){
    if(bytes.size()<l.guest_bytes)throw std::invalid_argument("short guest texture backing");
    if(mip>=d.levels||layer>=(d.volume_depth?std::max(d.volume_depth>>mip,1u):d.layers))throw std::out_of_range("guest texture subresource");
    uint32_t w=(std::max(d.width>>mip,1u)+l.block_extent-1)/l.block_extent,h=(std::max(d.height>>mip,1u)+l.block_extent-1)/l.block_extent;
    std::vector<std::byte> linear(uint64_t(w)*h*l.bytes_per_pixel*d.samples);
    CopyGuestSubresource(d,l,mip,layer,bytes.data(),linear.data(),false);return linear;
}
D3D12_SHADER_RESOURCE_VIEW_DESC GuestTextureCache::Entry::Srv()const{
    auto swizzle=desc.swizzle;
    if(desc.data_format==13)for(auto& component:swizzle)if(component==7)component=1;
    D3D12_SHADER_RESOURCE_VIEW_DESC view{};view.Format=layout.format;view.Shader4ComponentMapping=Mapping(swizzle);
    const float minimum=std::min(float(desc.min_lod)/256.0f,float(desc.levels-1));
    if(desc.stencil_address)view.Format=DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    if(desc.samples>1){
        if(desc.layers==1)view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DMS;
        else{view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
            view.Texture2DMSArray={desc.first_layer,desc.view_layers};}
        return view;
    }
    if(desc.volume_depth){view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE3D;view.Texture3D={desc.first_mip,desc.levels-desc.first_mip,minimum};return view;}
    if(desc.one_dimensional) {
        if(desc.layers==1){view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE1D;view.Texture1D.MostDetailedMip=desc.first_mip;view.Texture1D.ResourceMinLODClamp=minimum;view.Texture1D.MipLevels=desc.levels-desc.first_mip;}
        else{view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE1DARRAY;view.Texture1DArray.MostDetailedMip=desc.first_mip;view.Texture1DArray.ResourceMinLODClamp=minimum;view.Texture1DArray.MipLevels=desc.levels-desc.first_mip;view.Texture1DArray.FirstArraySlice=desc.first_layer;view.Texture1DArray.ArraySize=desc.view_layers;}
        return view;
    }
    if(desc.layers==1){view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;view.Texture2D.MostDetailedMip=desc.first_mip;view.Texture2D.ResourceMinLODClamp=minimum;view.Texture2D.MipLevels=desc.levels-desc.first_mip;}
    else{view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DARRAY;view.Texture2DArray.MostDetailedMip=desc.first_mip;view.Texture2DArray.ResourceMinLODClamp=minimum;view.Texture2DArray.MipLevels=desc.levels-desc.first_mip;view.Texture2DArray.FirstArraySlice=desc.first_layer;view.Texture2DArray.ArraySize=desc.view_layers;}return view;
}
D3D12_SHADER_RESOURCE_VIEW_DESC GuestTextureCache::Entry::Srv(const GuestTextureDesc& sampled_view)const{
    if(sampled_view==desc)return Srv();
    const bool shader_alias=!sampled_view.writable&&CompatibleGuestShaderStorage(desc,sampled_view);
    if((!CompatibleGuestSampledStorage(desc,sampled_view)&&!shader_alias)||sampled_view.first_mip>=sampled_view.levels||
       sampled_view.first_layer>=sampled_view.layers||!sampled_view.view_layers||
       sampled_view.view_layers>sampled_view.layers-sampled_view.first_layer)
        throw std::invalid_argument("Incompatible native sampled view storage/range");
    Entry view;view.desc=sampled_view;view.desc.stencil_address=desc.stencil_address;
    view.layout=shader_alias?LayoutGuestTexture(sampled_view):layout;return view.Srv();
}
void EncodeGuestSubresource(const GuestTextureDesc& d,const GuestTextureLayout& l,
    std::span<std::byte> guest,std::span<const std::byte> linear,uint32_t mip,uint32_t layer) {
    if(mip>=d.levels||layer>=(d.volume_depth?std::max(d.volume_depth>>mip,1u):d.layers))throw std::out_of_range("guest encode texture subresource");
    auto width=(std::max(d.width>>mip,1u)+l.block_extent-1)/l.block_extent,height=(std::max(d.height>>mip,1u)+l.block_extent-1)/l.block_extent;
    if(guest.size()<l.guest_bytes||linear.size()!=u64(width)*height*l.bytes_per_pixel*d.samples)
        throw std::invalid_argument("invalid guest texture encode backing");
    CopyGuestSubresource(d,l,mip,layer,linear.data(),guest.data(),true);
}
D3D12_UNORDERED_ACCESS_VIEW_DESC GuestTextureCache::Entry::Uav(uint32_t mip)const {
    if(!desc.writable||mip<desc.first_mip||mip>=desc.levels)throw std::out_of_range("invalid guest texture UAV mip/access");
    D3D12_UNORDERED_ACCESS_VIEW_DESC view{};view.Format=layout.format;
    if(desc.volume_depth){view.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE3D;view.Texture3D={mip,0,std::max(desc.volume_depth>>mip,1u)};return view;}
    if(desc.one_dimensional) {
        if(desc.layers==1){view.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE1D;view.Texture1D.MipSlice=mip;}
        else {view.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE1DARRAY;view.Texture1DArray={mip,desc.first_layer,desc.view_layers};}
        return view;
    }
    if(desc.layers==1){view.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;view.Texture2D.MipSlice=mip;}
    else {view.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2DARRAY;view.Texture2DArray.MipSlice=mip;
        view.Texture2DArray.FirstArraySlice=desc.first_layer;view.Texture2DArray.ArraySize=desc.view_layers;}
    return view;
}
D3D12_UNORDERED_ACCESS_VIEW_DESC GuestTextureCache::Entry::Uav(const GuestTextureDesc& writable_view,uint32_t mip)const {
    if(writable_view==desc)return Uav(mip);
    if(!CompatibleGuestWritableStorage(desc,writable_view)||
       writable_view.first_layer>=writable_view.layers||!writable_view.view_layers||
       writable_view.view_layers>writable_view.layers-writable_view.first_layer)
        throw std::invalid_argument("Incompatible native writable view storage/range");
    Entry view;view.desc=writable_view;view.layout=LayoutGuestTexture(writable_view);return view.Uav(mip);
}
D3D12_RENDER_TARGET_VIEW_DESC GuestTextureCache::Entry::Rtv(uint32_t mip)const {
    if(!desc.render_target||mip<desc.first_mip||mip>=desc.levels)throw std::out_of_range("Invalid guest texture RTV mip/access");
    D3D12_RENDER_TARGET_VIEW_DESC view{};view.Format=layout.format;
    if(texture.handle&&texture.handle->GetDesc().SampleDesc.Count>1){
        if(desc.layers==1)view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2DMS;
        else{view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
            view.Texture2DMSArray={desc.first_layer,desc.view_layers};}
        return view;
    }
    if(desc.volume_depth){view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE3D;
        view.Texture3D={mip,0,std::max(desc.volume_depth>>mip,1u)};return view;}
    if(desc.one_dimensional){
        if(desc.layers==1){view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE1D;view.Texture1D.MipSlice=mip;}
        else{view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
            view.Texture1DArray={mip,desc.first_layer,desc.view_layers};}return view;
    }
    if(desc.layers==1){view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2D;view.Texture2D.MipSlice=mip;}
    else {view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2DARRAY;view.Texture2DArray.MipSlice=mip;
        view.Texture2DArray.FirstArraySlice=desc.first_layer;view.Texture2DArray.ArraySize=desc.view_layers;}
    return view;
}
D3D12_RENDER_TARGET_VIEW_DESC GuestTextureCache::Entry::Rtv(const GuestTextureDesc& attachment,uint32_t mip)const{
    if(!CompatibleGuestAttachmentStorage(desc,attachment))
        throw std::invalid_argument("Incompatible native render target view");
    Entry view;view.desc=attachment;view.layout=layout;view.texture.handle=texture.handle;
    return view.Rtv(mip);
}
D3D12_DEPTH_STENCIL_VIEW_DESC GuestTextureCache::Entry::Dsv(bool read_only_depth,bool read_only_stencil)const {
    if(!desc.depth_target)throw std::logic_error("Guest image is not a depth attachment");
    D3D12_DEPTH_STENCIL_VIEW_DESC view{};
    view.Format=desc.stencil_address?DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        layout.bytes_per_pixel==4?DXGI_FORMAT_D32_FLOAT:DXGI_FORMAT_D16_UNORM;
    if(desc.samples>1){
        if(desc.layers==1)view.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2DMS;
        else{view.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
            view.Texture2DMSArray={desc.first_layer,desc.view_layers};}
    }else if(desc.layers==1){view.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2D;view.Texture2D.MipSlice=desc.first_mip;}
    else {view.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
        view.Texture2DArray.MipSlice=desc.first_mip;
        view.Texture2DArray.FirstArraySlice=desc.first_layer;view.Texture2DArray.ArraySize=desc.view_layers;}
    if(read_only_depth)view.Flags|=D3D12_DSV_FLAG_READ_ONLY_DEPTH;
    if(read_only_stencil&&desc.stencil_address)view.Flags|=D3D12_DSV_FLAG_READ_ONLY_STENCIL;
    return view;
}
D3D12_DEPTH_STENCIL_VIEW_DESC GuestTextureCache::Entry::Dsv(const GuestTextureDesc& attachment,bool read_only_depth,bool read_only_stencil)const{
    if(!CompatibleGuestAttachmentStorage(desc,attachment))
        throw std::invalid_argument("Incompatible native depth target view");
    Entry view;view.desc=attachment;view.layout=layout;
    return view.Dsv(read_only_depth,read_only_stencil);
}
void GuestTextureCache::Entry::RememberSnapshot(std::span<const std::byte> bytes,std::span<const std::byte> stencil){
    if(desc.writable||bytes.size()<layout.guest_bytes||
       (desc.stencil_address&&stencil.size()<stencil_layout.guest_bytes))
        throw std::invalid_argument("Invalid coherent texture snapshot backing");
    sampled_snapshot.assign(bytes.begin(),bytes.begin()+size_t(layout.guest_bytes));
    if(desc.stencil_address)stencil_snapshot.assign(stencil.begin(),stencil.begin()+size_t(stencil_layout.guest_bytes));
}
void GuestTextureCache::SetBackingSamples(Entry& entry,UINT samples,ID3D12GraphicsCommandList* list,bool copy){
    if(!list||!entry.texture.handle||!samples||(samples&(samples-1))||samples>16)
        throw std::invalid_argument("Invalid native image backing sample count");
    const auto current=entry.texture.handle->GetDesc();
    const auto old_samples=current.SampleDesc.Count;if(old_samples==samples)return;
    if(entry.desc.depth_target||entry.desc.levels!=1||entry.desc.volume_depth||entry.desc.one_dimensional)
        throw std::invalid_argument("Sample backing conversion requires a single-mip 2D color image");
    auto& destination=entry.sample_backings[samples];
    entry.sample_backings.try_emplace(old_samples);
    if(!destination.handle){
        auto description=current;description.SampleDesc={samples,0};description.Alignment=0;
        description.Flags|=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if(samples>1)description.Flags&=~D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        D3D12_FEATURE_DATA_FORMAT_SUPPORT support{entry.layout.format};
        Check(DX12_API_CALL(device_->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support))));
        if(!(support.Support1&D3D12_FORMAT_SUPPORT1_RENDER_TARGET))
            throw std::runtime_error("Native sample backing format lacks render-target support");
        if(samples>1){
            D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS quality{entry.layout.format,samples,D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE,0};
            Check(DX12_API_CALL(device_->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,&quality,sizeof(quality))));
            if(!quality.NumQualityLevels)throw std::runtime_error("Native sample backing count is unsupported by the adapter");
        }
        const auto formats=ResourceCasts(entry.texture);
        const auto allocation=texture_allocator_.AllocationInfo(description,formats);
        if(!allocation.SizeInBytes||allocation.SizeInBytes==UINT64_MAX||
           entry.allocation_bytes>UINT64_MAX-allocation.SizeInBytes)
            throw std::runtime_error("Invalid native sample backing allocation size");
        ReserveMemory(allocation.SizeInBytes,collected_fence_,&entry);
        destination=texture_allocator_.Allocate(description,D3D12_RESOURCE_STATE_COMMON,nullptr,formats);
        entry.allocation_bytes+=allocation.SizeInBytes;
    }
    if(copy){
        if(!multisample_)multisample_=std::make_shared<GuestMultisample>(device_.Get());
        entry.multisample=multisample_;
        std::vector<std::shared_ptr<GuestMultisample::CopyLease>> prepared;
        prepared.reserve(entry.desc.layers);
        for(UINT layer=0;layer<entry.desc.layers;++layer)
            prepared.push_back(multisample_->PrepareCopy(entry.texture,destination,entry.desc,entry.layout,layer));
        entry.sample_copy_leases.insert(entry.sample_copy_leases.end(),prepared.begin(),prepared.end());
        entry.pending=true;
        for(const auto& lease:prepared)lease->Record(list,entry.texture,destination);
    }
    entry.sample_backings[old_samples]=std::move(entry.texture);
    entry.texture=std::move(destination);entry.last_use=++use_sequence_;
}
std::shared_ptr<GuestMultisample::StorageLease> GuestTextureCache::PrepareMultisampleStorage(Entry& entry){
    if(!entry.desc.writable||entry.desc.samples<2||entry.storage_samples.size()!=entry.desc.samples||
       !entry.storage_linear.handle||!multisample_)
        throw std::invalid_argument("Native MSAA shader storage is not allocated");
    auto lease=multisample_->PrepareStorage(entry.texture,entry.storage_samples,entry.storage_linear,entry.desc,entry.layout);
    entry.storage_leases.push_back(lease);entry.pending=true;return lease;
}
void GuestTextureCache::RegisterMeta(Entry& entry,MetaType type,uint64_t address,uint32_t initial_mask){
    const auto owner=image_uids_.find(entry.image_uid);
    if(entry.cache!=this||owner==image_uids_.end()||owner->second!=&entry)
        throw std::invalid_argument("Metadata owner is not a live native image");
    auto& previous=entry.metadata_addresses.at(size_t(type));
    if(previous!=address){
        const auto old=surface_metas_.find(previous);
        if(old!=surface_metas_.end()&&old->second.owner_uid==entry.image_uid)surface_metas_.erase(old);
        previous=address;
    }
    if(!address)return;
    const MetaDataInfo replacement{type,entry.image_uid,entry.desc.samples,initial_mask};
    const auto [it,inserted]=surface_metas_.try_emplace(address,replacement);
    if(!inserted){
        if(it->second.type!=type||it->second.owner_uid!=entry.image_uid)it->second=replacement;
        else it->second.num_samples=entry.desc.samples;
    }
}
std::optional<GuestTextureCache::MetaDataInfo> GuestTextureCache::GetMetaInfo(uint64_t address)const{
    const auto it=surface_metas_.find(address);
    if(it==surface_metas_.end())return std::nullopt;
    const auto owner=image_uids_.find(it->second.owner_uid);
    if(owner==image_uids_.end()||owner->second->metadata_addresses.at(size_t(it->second.type))!=address)
        return std::nullopt;
    return it->second;
}
bool GuestTextureCache::ClearMeta(uint64_t address){
    if(!GetMetaInfo(address))return false;
    surface_metas_.at(address).clear_mask=UINT32_MAX;return true;
}
bool GuestTextureCache::ConsumeMeta(uint64_t address,uint32_t slice){
    if(slice>=32||!GetMetaInfo(address))return false;
    auto& mask=surface_metas_.at(address).clear_mask;
    const uint32_t bit=uint32_t(1)<<slice;
    const bool clear=(mask&bit)!=0;mask&=~bit;return clear;
}
void GuestTextureCache::RemoveMeta(uint64_t address,uint64_t size){
    std::erase_if(surface_metas_,[&](const auto& meta){return meta.first>=address&&meta.first-address<size;});
}
std::shared_ptr<void> GuestTextureCache::PinBinding(Entry& entry){
    const auto owner=image_uids_.find(entry.image_uid);
    if(entry.cache!=this||owner==image_uids_.end()||owner->second!=&entry)
        throw std::invalid_argument("Native image binding is not owned by this cache");
    if(!entry.binding_pins)entry.binding_pins=std::make_shared<uint32_t>();
    if(*entry.binding_pins==UINT32_MAX)throw std::overflow_error("Native image binding pin overflow");
    ++*entry.binding_pins;
    auto counter=entry.binding_pins;
    // shared_ptr invokes the deleter if control-block allocation fails as well.
    return std::shared_ptr<void>(counter.get(),[counter](void*){--*counter;});
}
GuestTextureCache::PressureSourceLease GuestTextureCache::RetainPressureSource(Entry& entry){
    const auto owner=image_uids_.find(entry.image_uid);
    if(entry.cache!=this||owner==image_uids_.end()||owner->second!=&entry||!entry.texture.handle)
        throw std::invalid_argument("Pressure source is not owned by this image cache");
    const auto description=entry.texture.handle->GetDesc();
    auto bytes=texture_allocator_.AllocationInfo(description,ResourceCasts(entry.texture)).SizeInBytes;
    if(!bytes||bytes==UINT64_MAX)
        throw std::overflow_error("Invalid pressure source allocation size");
    Resource packed;
    if(entry.layout.expanded_bytes_per_pixel){
        if(!HasExactPackedExpansion(entry.desc,entry.layout)||!entry.expanded_guest_backing.handle)
            throw std::invalid_argument("Expanded pressure source has no exact packed backing");
        packed=entry.expanded_guest_backing;const auto buffer=packed.handle->GetDesc();
        const auto packed_bytes=device_->GetResourceAllocationInfo(0,1,&buffer).SizeInBytes;
        if(!packed_bytes||packed_bytes==UINT64_MAX||bytes>UINT64_MAX-packed_bytes)
            throw std::overflow_error("Invalid packed pressure source allocation size");
        bytes+=packed_bytes;
    }
    if(PressureRetirementBytes()>UINT64_MAX-bytes)
        throw std::overflow_error("Pressure source residency overflow");
    auto source=entry.texture;auto counter=pressure_retirement_bytes_;counter->fetch_add(bytes);
    auto lease=std::shared_ptr<void>(counter.get(),[counter,bytes,source=std::move(source),packed=std::move(packed)](void*)mutable{
        source={};packed={};counter->fetch_sub(bytes);
    });
    return {std::move(lease),bytes};
}
uint64_t GuestTextureCache::ResidentBytes()const{
    uint64_t total=PressureRetirementBytes();
    for(const auto& e:entries_){const auto count=e->ResidentBytes();
        if(total>UINT64_MAX-count)throw std::overflow_error("Texture residency total overflow");total+=count;
    }
    return total;
}
void GuestTextureCache::ReserveMemory(uint64_t bytes,uint64_t completed,Entry* preserve){
    RefreshMemoryBudget();
    const auto resident=[&]{const auto total=ResidentBytes();
        if(total>UINT64_MAX-bytes)throw std::overflow_error("Texture reservation total overflow");return total+bytes;
    };
    uint64_t total=resident();
    if(total>resident_budget_)for(auto& e:entries_){
        if(e.get()==preserve||e->BindingPinned()||e->pending||e->fence>completed||!e->tiler_leases.empty()||!e->multisample_leases.empty()||
           (e->readback_pool&&e->readback_pool.use_count()!=1))continue;
        const auto before=e->ResidentBytes();
        e->upload_pool.clear();e->tiler_input_pool.clear();e->tiler_output_pool.clear();e->readback_pool.reset();
        for(auto it=e->sample_backings.begin();it!=e->sample_backings.end();){
            const auto& backing=it->second;
            if(backing.handle&&backing.allocation&&backing.allocation.use_count()==1){
                const auto description=backing.handle->GetDesc();
                const auto allocation=texture_allocator_.AllocationInfo(description,ResourceCasts(backing)).SizeInBytes;
                if(allocation&&allocation<=e->allocation_bytes){e->allocation_bytes-=allocation;it=e->sample_backings.erase(it);continue;}
            }
            ++it;
        }
        total-=before-e->ResidentBytes();
        if(total<=resident_budget_)break;
    }
    while(total>resident_budget_){
        auto victim=entries_.end();
        for(auto it=entries_.begin();it!=entries_.end();++it)
            if(it->get()!=preserve&&!(*it)->BindingPinned()&&!(*it)->pending&&(*it)->fence<=completed&&
               // Original GC skips a clean GPU-modified tiled image: its
               // unsupported download cannot replace the physical buffer domain.
               !((*it)->gpu_owned&&(*it)->buffer_alias_version&&
                 ((*it)->desc.tile_mode!=uint32_t(AmdGpu::TileMode::DisplayLinearAligned)||(*it)->desc.samples!=1))&&
               (!(*it)->readback_pool||(*it)->readback_pool.use_count()==1)&&
               (victim==entries_.end()||(*it)->last_use<(*victim)->last_use))victim=it;
        if(victim==entries_.end()){
            const auto retired=PressureRetirementBytes();
            if(retired&&wait_pressure_){
                completed=std::max(completed,wait_pressure_());Collect(completed);total=resident();
                if(total<=resident_budget_)break;
                if(PressureRetirementBytes()<retired)continue;
            }
            throw std::runtime_error("Native texture batch exceeds adapter-derived cache budget including transfer pools and in-flight sources");
        }
        if((*victim)->gpu_owned){
            const auto& retire=retire_pressure_?retire_pressure_:resolve_owned_;
            if(!retire)throw std::runtime_error("GPU-owned image has no retirement resolver");retire(**victim);
        }
        if((*victim)->cpu_watched&&release_immutable_)release_immutable_(**victim);
        entries_.erase(victim);RebuildIndex();total=resident();
    }
}
bool GuestTextureCache::CanUploadGpuSnapshot(const GuestTextureDesc& d,const GuestTextureLayout& layout,bool writable_snapshot){
    return (!d.writable||writable_snapshot)&&!d.stencil_address&&d.samples==1&&d.address%4==0&&
        layout.guest_bytes%4==0&&layout.guest_bytes<=64*1024*1024&&
        (!layout.expanded_bytes_per_pixel||HasExactPackedExpansion(d,layout));
}
GuestTextureCache::Entry& GuestTextureCache::Upload(const GuestTextureDesc& d,std::span<const std::byte> bytes,ID3D12GraphicsCommandList* list,uint64_t completed,std::span<const std::byte> stencil_bytes,bool guest_snapshot,const Resource* gpu_snapshot,bool writable_snapshot){
    Diagnostics::Scope diagnostic(Diagnostics::Kind::Upload,"layout-and-cache-lookup");
    diagnostic.Detail("address=%llx size=%ux%u levels=%u layers=%u tile=%u bytes=%zu",
        (unsigned long long)d.address,d.width,d.height,d.levels,d.layers,d.tile_mode,bytes.size());
    auto layout=LayoutGuestTexture(d);
    if(gpu_snapshot){
        if(!guest_snapshot||!gpu_snapshot->handle||!CanUploadGpuSnapshot(d,layout,writable_snapshot)||
            gpu_snapshot->handle->GetDesc().Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||
            gpu_snapshot->handle->GetDesc().Width<layout.guest_bytes||
            gpu_snapshot->state!=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
            throw std::invalid_argument("Invalid GPU guest image snapshot");
    }else if(bytes.size()<layout.guest_bytes)throw std::invalid_argument("short guest texture upload");
    Collect(completed);
    if(guest_snapshot&&d.writable&&!writable_snapshot)throw std::invalid_argument("Guest image snapshot must be read-only");
    if(writable_snapshot&&(!guest_snapshot||!CanRetainBufferAlias(d,layout)))
        throw std::invalid_argument("Writable image snapshot has no independent cache ownership");
    if(d.writable)ValidateWritableFormat(layout.format);
    GuestTextureDesc stencil_desc;GuestTextureLayout stencil_layout;
    if(d.stencil_address){stencil_desc=DescribeGuestStencil(d);stencil_layout=LayoutGuestTexture(stencil_desc);
        if(stencil_bytes.size()<stencil_layout.guest_bytes)throw std::invalid_argument("Short guest stencil upload");}
    auto address_entries=[&](){return address_index_.equal_range(d.address);};
    auto [owned_first,owned_last]=address_entries();
    for(auto indexed=owned_first;indexed!=owned_last;++indexed){auto* existing=indexed->second;
      if(guest_snapshot&&existing->cpu_watched){
        if(release_immutable_)release_immutable_(*existing);existing->cpu_watched=false;
      }
      const bool pending_coherent=!gpu_snapshot&&!existing->gpu_owned&&!existing->cpu_watched&&existing->pending&&existing->sampled_snapshot.size()==layout.guest_bytes&&
        !memcmp(existing->sampled_snapshot.data(),bytes.data(),size_t(layout.guest_bytes))&&
        (!d.stencil_address||(existing->stencil_snapshot.size()==stencil_layout.guest_bytes&&
         !memcmp(existing->stencil_snapshot.data(),stencil_bytes.data(),size_t(stencil_layout.guest_bytes))));
      if((pending_coherent||(!guest_snapshot&&(existing->gpu_owned||existing->cpu_watched)))&&
        (existing->desc==d||CompatibleGuestAttachmentStorage(existing->desc,d)||CompatibleGuestWritableStorage(existing->desc,d)||
         (existing->desc.writable&&CompatibleGuestShaderStorage(existing->desc,d))||
         (!d.writable&&!d.render_target&&!d.depth_target&&CompatibleGuestSampledStorage(existing->desc,d)))&&
        existing->layout.guest_bytes==layout.guest_bytes){
        bool same_layout=existing->layout.mips.size()==layout.mips.size();
        for(size_t i=0;same_layout&&i<layout.mips.size();++i){const auto& a=existing->layout.mips[i];const auto& b=layout.mips[i];
            same_layout=a.pitch==b.pitch&&a.height==b.height&&a.offset==b.offset&&a.slice_bytes==b.slice_bytes&&a.macro==b.macro&&a.thickness==b.thickness;}
        if(!same_layout)continue;
        // Page watchers prove that no CPU access can have changed these bytes.
        // Keep the original native image through subsequent draws/sampling.
        existing->last_use=++use_sequence_;existing->pending=true;
        if(existing->texture.state==D3D12_RESOURCE_STATE_UNORDERED_ACCESS)existing->texture.UavBarrier(list);
        existing->texture.Transition(list,d.writable&&d.samples==1?D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        return *existing;
    }}
    // A different view (in particular a compute UAV) may overlap an owned
    // render target without qualifying for direct GPU reuse. Resolve ALL
    // overlapping backing on the caller before any comparison or parallel
    // detile can fault on a worker waiting for the caller's execution mutex.
    for(auto indexed=owned_first;indexed!=owned_last;++indexed){auto& existing=*indexed->second;
        if(!guest_snapshot&&existing.gpu_owned&&CompatibleGuestPackedStorage(existing.desc,d)&&
           ConvertPackedBacking(existing,d,layout,list))return existing;
    }
    const auto overlaps=[](uint64_t a,uint64_t as,uint64_t b,uint64_t bs){
        return as&&bs&&(a<=b?b-a<as:a-b<bs);
    };
    // A prewrite snapshot already owns complete independent contents. Current
    // shader writers may have acquired guest ownership after capture; resolving
    // them here would retire their initialization before their recorded writes.
    for(auto& existing:entries_)if(!guest_snapshot&&existing->gpu_owned&&
        (overlaps(d.address,layout.guest_bytes,existing->desc.address,existing->layout.guest_bytes)||
         (d.stencil_address&&overlaps(d.stencil_address,stencil_layout.guest_bytes,existing->desc.address,existing->layout.guest_bytes))||
         (existing->desc.stencil_address&&(overlaps(d.address,layout.guest_bytes,existing->desc.stencil_address,existing->stencil_layout.guest_bytes)||
           (d.stencil_address&&overlaps(d.stencil_address,stencil_layout.guest_bytes,existing->desc.stencil_address,existing->stencil_layout.guest_bytes)))))){
        if(existing->buffer_alias_version){
            if(!read_independent_||!acquire_independent_||!retire_independent_||
               !CanUploadGpuSnapshot(d,layout,d.writable)||d.stencil_address||existing->desc.samples!=1)
                throw std::runtime_error("Independent image view has no domain-preserving GPU transfer");
            auto* source=existing.get();auto pin=PinBinding(*source);
            auto snapshot=read_independent_(*source,d.address,layout.guest_bytes);
            // Source ownership and its CPU-write guard remain live until the
            // new interpretation has acquired its own guard. Pending copies
            // retain the old allocation/descriptors through normal cache fences.
            auto& converted=Upload(d,{},list,completed,{},true,&snapshot,d.writable);
            if(!acquire_independent_(converted))
                throw std::runtime_error("Converted independent image could not retain ownership");
            retire_independent_(*source);
            Diagnostics::Count("image-version.gpu-reinterpret");
            return converted;
        }
        if(!resolve_owned_)throw std::runtime_error("GPU-owned upload backing has no resolver");
        resolve_owned_(*existing);
        if(existing->gpu_owned)throw std::runtime_error("Upload backing remains GPU-owned after resolution");
        // The resolver retires the image and reopens the recording list. Its
        // submission may be newer than the fence supplied by the caller.
        completed=std::max(completed,existing->fence);
    }
    // A depth/color attachment can be sampled by a different T# view. Reusing
    // its native texture avoids a 64MiB CPU detile/upload for 4096² shadows.
    // Compare the complete backing, including padding: CPU changes fall back to
    // the original checked upload path. No guest readback or fence is skipped.
    if(!gpu_snapshot&&!d.writable&&!d.render_target&&!d.depth_target&&!d.stencil_address){
        static unsigned large_traces{};
        if(Diagnostics::Enabled()&&d.width>=4096&&large_traces++<8){
            auto [first,last]=address_entries();for(auto indexed=first;indexed!=last;++indexed){auto* existing=indexed->second;
                const auto& s=existing->desc;
                fprintf(stderr,"DX12 sampled candidate address=%llx requested=%ux%u fmt=%u/%u tile=%u pow2=%u existing=%ux%u fmt=%u/%u tile=%u depth=%u stencil=%llx pow2=%u bytes=%llu snapshot=%zu pending=%u\n",
                    (unsigned long long)d.address,d.width,d.height,d.data_format,d.number_format,d.tile_mode,unsigned(d.pow2_padding),
                    s.width,s.height,s.data_format,s.number_format,s.tile_mode,unsigned(s.depth_target),(unsigned long long)s.stencil_address,
                    unsigned(s.pow2_padding),(unsigned long long)existing->layout.guest_bytes,existing->sampled_snapshot.size(),unsigned(existing->pending));
            }
        }
        auto [first,last]=address_entries();for(auto indexed=first;indexed!=last;++indexed){auto* existing=indexed->second;
            if(existing->pending||existing->fence>completed||
               !CompatibleGuestSampledStorage(existing->desc,d)||
               existing->layout.guest_bytes!=layout.guest_bytes||
               existing->sampled_snapshot.size()!=layout.guest_bytes)continue;
            bool same_layout=existing->layout.mips.size()==layout.mips.size();
            for(size_t i=0;same_layout&&i<layout.mips.size();++i){
                const auto& a=existing->layout.mips[i];const auto& b=layout.mips[i];
                same_layout=a.pitch==b.pitch&&a.height==b.height&&a.offset==b.offset&&
                    a.slice_bytes==b.slice_bytes&&a.macro==b.macro&&a.thickness==b.thickness;
            }
            if(!same_layout||memcmp(existing->sampled_snapshot.data(),bytes.data(),size_t(layout.guest_bytes)))continue;
            diagnostic.Stage("coherent-native-view-reuse");
            static unsigned reuse_traces{};
            if(Diagnostics::Enabled()&&reuse_traces++<16){
                fprintf(stderr,"DX12 coherent sampled reuse address=%llx size=%ux%u depth=%u color=%u bytes=%llu\n",
                    (unsigned long long)d.address,d.width,d.height,unsigned(existing->desc.depth_target),
                    unsigned(existing->desc.render_target),(unsigned long long)layout.guest_bytes);
                fflush(stderr);
            }
            existing->last_use=++use_sequence_;existing->pending=true;
            existing->texture.Transition(list,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            return *existing;
        }
    }
    Entry* entry=nullptr;{auto [first,last]=address_entries();for(auto indexed=first;indexed!=last;++indexed)
        if(!(guest_snapshot&&(indexed->second->gpu_owned||indexed->second->BindingPinned()))&&
           (indexed->second->desc==d||CompatibleGuestAttachmentStorage(indexed->second->desc,d)||
            CompatibleGuestWritableStorage(indexed->second->desc,d))){entry=indexed->second;break;}}
    const bool immutable_view=!d.writable;
    if(entry&&immutable_view&&!gpu_snapshot&&
       ((!entry->pending&&entry->fence<=completed)||(!d.render_target&&!d.depth_target))&&
       entry->sampled_snapshot.size()==layout.guest_bytes&&
       std::equal(entry->sampled_snapshot.begin(),entry->sampled_snapshot.end(),bytes.begin())&&
       (!d.stencil_address||(entry->stencil_snapshot.size()==stencil_layout.guest_bytes&&
           std::equal(entry->stencil_snapshot.begin(),entry->stencil_snapshot.end(),stencil_bytes.begin())))){
        entry->last_use=++use_sequence_;entry->pending=true;
        entry->texture.Transition(list,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        return *entry;
    }
    if(entry&&(entry->pending||entry->fence>completed)) {
        // Changed CPU data must not overwrite staging used by queued copies.
        // The caller retires the packet and reopens this command-list object.
        if(retire_pending_) {retire_pending_();completed=std::max(completed,entry->fence);}
        if(entry->pending||entry->fence>completed)
            throw std::runtime_error("guest texture upload still in flight");
    }
    const bool gpu_detile=gpu_snapshot||HasExactPackedExpansion(d,layout)||
        (!layout.expanded_bytes_per_pixel&&(d.data_format==13||(d.data_format>=16&&d.data_format<=19)||d.samples>1||(d.tile_mode!=8&&d.tile_mode!=31)||
        (d.depth_target&&d.stencil_address&&layout.bytes_per_pixel==2)));
    if(entry&&gpu_snapshot){
        const auto previous=entry->tiler_input_pool.empty()?0:entry->tiler_input_pool[0].handle->GetDesc().Width;
        const auto incoming=gpu_snapshot->handle->GetDesc().Width;
        if(incoming>previous)ReserveMemory(incoming-previous,completed,entry);
    }
    if(layout.expanded_bytes_per_pixel&&d.data_format>=35&&!bc_decoder_)bc_decoder_=std::make_unique<GuestBcDecoder>(converter_);
    if(gpu_detile&&!tiler_)tiler_=std::make_shared<GuestTiler>(device_.Get());
    if(d.samples>1&&!multisample_)multisample_=std::make_shared<GuestMultisample>(device_.Get());
    const auto cast_formats=PackedFloatCasts(layout.format,texture_allocator_.SupportsFormatCasting());
    if(d.samples>1&&(d.data_format==6||d.data_format==34)&&cast_formats.empty())
        throw std::runtime_error("Native packed-float MSAA requires bit-preserving D3D12 format casting");
    bool new_resource=false;
    if(!entry){
        diagnostic.Stage("resource-allocation");
        if(!next_image_uid_)throw std::overflow_error("Native image identity exhausted");
        auto new_entry=std::make_unique<Entry>();new_entry->desc=d;new_entry->layout=layout;new_entry->cache=this;
        new_entry->image_uid=next_image_uid_++;
        if(d.depth_target&&std::ranges::any_of(entries_,[&](const auto& prior){return
            prior->desc.address==d.address&&!prior->desc.depth_target&&
            prior->layout.bytes_per_pixel==layout.bytes_per_pixel&&
            prior->layout.guest_bytes==layout.guest_bytes&&prior->desc.samples==d.samples;}))
            new_entry->htile_initial_mask=0;
        if(d.stencil_address)new_entry->stencil_layout=stencil_layout;
        D3D12_RESOURCE_DESC r{};r.Dimension=d.volume_depth?D3D12_RESOURCE_DIMENSION_TEXTURE3D:d.one_dimensional?D3D12_RESOURCE_DIMENSION_TEXTURE1D:D3D12_RESOURCE_DIMENSION_TEXTURE2D;r.Width=d.width;r.Height=d.height;r.DepthOrArraySize=UINT16(d.volume_depth?d.volume_depth:d.layers);r.MipLevels=UINT16(d.levels);r.Format=layout.format;r.SampleDesc.Count=d.samples;
        if(layout.format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)r.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;
        if(!d.depth_target&&!layout.expanded_bytes_per_pixel)r.Format=WritableStorageFormat(d.data_format,layout.format);
        if(HasExactPackedExpansion(d,layout))r.Format=DXGI_FORMAT_R32G32B32A32_TYPELESS;
        if(d.depth_target) {
            if(d.writable||d.render_target||(d.samples>1&&d.levels!=1)||
               (layout.format!=DXGI_FORMAT_R32_FLOAT&&layout.format!=DXGI_FORMAT_R16_UNORM))
                throw std::invalid_argument("Invalid guest depth resource usage/format");
            r.Format=d.stencil_address?DXGI_FORMAT_R32G8X24_TYPELESS:
                layout.bytes_per_pixel==4?DXGI_FORMAT_R32_TYPELESS:DXGI_FORMAT_R16_TYPELESS;
            r.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        }
        if(d.writable) {
            r.Format=d.UsesRaw32()?
                DXGI_FORMAT_R32_TYPELESS:WritableStorageFormat(d.data_format,layout.format);
            if(d.samples==1)r.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }
        if(d.render_target||(d.samples>1&&!d.depth_target)) {
            D3D12_FEATURE_DATA_FORMAT_SUPPORT support{layout.format};
            Check(DX12_API_CALL(device_->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support))));
            if(!(support.Support1&D3D12_FORMAT_SUPPORT1_RENDER_TARGET))throw std::runtime_error("Native guest color format lacks render-target support");
            r.Flags|=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }
        if(d.samples>1){
            const auto typed=d.depth_target?(d.stencil_address?DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
                layout.bytes_per_pixel==2?DXGI_FORMAT_D16_UNORM:DXGI_FORMAT_D32_FLOAT):layout.format;
            D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS quality{typed,d.samples,D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE,0};
            Check(DX12_API_CALL(device_->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,&quality,sizeof(quality))));
            if(!quality.NumQualityLevels)throw std::runtime_error("Native adapter does not support the guest attachment sample count");
        }
        // Account for persistent upload/readback storage as well as texture
        // backing and CPU snapshots; pools must not silently bypass the bound.
        uint64_t transfer_bytes{};
        for(UINT sub=0;sub<d.layers*d.levels*(d.stencil_address?2u:1u);++sub){
            UINT64 bytes{};
            if(d.samples>1){const auto& physical=sub>=d.layers?stencil_layout:layout;
                bytes=GuestMultisample::LinearBytes(d,GuestMultisample::Footprint(d,physical));}
            else DX12_API_CALL(device_->GetCopyableFootprints(&r,sub,1,0,nullptr,nullptr,nullptr,&bytes));
            transfer_bytes+=bytes;
        }
        const auto allocation=texture_allocator_.AllocationInfo(r,cast_formats);
        if(allocation.SizeInBytes==UINT64_MAX)throw std::runtime_error("Native guest image allocation information is invalid");
        new_entry->allocation_bytes=allocation.SizeInBytes;
        auto storage_description=r;storage_description.SampleDesc={1,0};storage_description.Alignment=0;
        storage_description.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        uint64_t storage_linear_bytes{};
        if(d.writable&&d.samples>1){
            const auto storage_allocation=texture_allocator_.AllocationInfo(storage_description,cast_formats);
            storage_linear_bytes=GuestMultisample::LinearBytes(d,GuestMultisample::Footprint(d,layout));
            D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
            buffer.Width=(storage_linear_bytes+3)&~uint64_t(3);buffer.Height=buffer.DepthOrArraySize=buffer.MipLevels=1;
            buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            buffer.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            const auto linear_allocation=device_->GetResourceAllocationInfo(0,1,&buffer);
            if(!storage_allocation.SizeInBytes||storage_allocation.SizeInBytes==UINT64_MAX||
               !linear_allocation.SizeInBytes||linear_allocation.SizeInBytes==UINT64_MAX||
               new_entry->allocation_bytes>UINT64_MAX-linear_allocation.SizeInBytes||
               storage_allocation.SizeInBytes>(UINT64_MAX-new_entry->allocation_bytes-linear_allocation.SizeInBytes)/d.samples)
                throw std::runtime_error("Invalid native MSAA shader storage allocation size");
            new_entry->allocation_bytes+=storage_allocation.SizeInBytes*d.samples+linear_allocation.SizeInBytes;
        }
        const uint64_t physical_bytes=(gpu_snapshot?gpu_snapshot->handle->GetDesc().Width:((layout.guest_bytes+3)&~uint64_t(3)))+
            (d.stencil_address?((stencil_layout.guest_bytes+3)&~uint64_t(3)):0);
        const uint64_t masks=GuestTiler::WrittenBytes(layout.guest_bytes)+
            (d.stencil_address?GuestTiler::WrittenBytes(stencil_layout.guest_bytes):0);
        if(d.writable||d.render_target||d.depth_target)
            new_entry->readback_reserve_bytes=transfer_bytes+(gpu_detile?physical_bytes*2+masks*2:0);
        const uint64_t required=new_entry->allocation_bytes+
            (immutable_view&&!gpu_snapshot?layout.guest_bytes+(d.stencil_address?stencil_layout.guest_bytes:0):0)+
            transfer_bytes+(gpu_detile?physical_bytes:0)+new_entry->readback_reserve_bytes;
        ReserveMemory(required,completed);
        // Copy queues require COMMON on entry and decay back to COMMON at
        // submission completion. Depth/stencil cannot use implicit promotion,
        // so keep its upload and explicit barriers on the direct list.
        new_entry->texture=texture_allocator_.Allocate(r,submit_copy_&&!d.depth_target&&!gpu_detile?
            D3D12_RESOURCE_STATE_COMMON:D3D12_RESOURCE_STATE_COPY_DEST,nullptr,cast_formats);
        if(storage_linear_bytes){
            new_entry->storage_samples.reserve(d.samples);
            for(UINT sample=0;sample<d.samples;++sample)
                new_entry->storage_samples.push_back(texture_allocator_.Allocate(storage_description,D3D12_RESOURCE_STATE_COMMON,nullptr,cast_formats));
            new_entry->storage_linear=GuestTiler::Scratch(device_.Get(),storage_linear_bytes);
        }
        entry=new_entry.get();entries_.push_back(std::move(new_entry));Index(*entry);new_resource=true;}
    SetBackingSamples(*entry,d.samples,list,false);
    entry->expanded_guest_backing={};entry->expanded_written_mask={};entry->expanded_backing_bytes=0;
    entry->last_use=++use_sequence_;entry->sampled_snapshot.clear();
    const bool copy_upload=new_resource&&submit_copy_&&!d.depth_target&&!gpu_detile;
    entry->pending=true;
    if(!copy_upload)entry->texture.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
    auto resource_desc=DX12_API_CALL(entry->texture.handle->GetDesc());
    struct PendingTextureCopy {D3D12_TEXTURE_COPY_LOCATION destination,source;};
    std::vector<PendingTextureCopy> copy_batch;
    if(gpu_detile){
        diagnostic.Stage("gpu-detile-upload");
        entry->tiler=tiler_;entry->tiler_leases.clear();
        entry->multisample=multisample_;entry->multisample_leases.clear();
        const auto planes=d.stencil_address?2u:1u;
        entry->tiler_leases.reserve(size_t(planes)*d.layers*d.levels);
        entry->tiler_input_pool.reserve(planes);
        entry->tiler_output_pool.reserve(size_t(planes)*d.layers*d.levels);
        entry->multisample_leases.reserve(size_t(planes)*d.layers);
        for(uint32_t plane=0;plane<planes;++plane){
            const auto& physical=plane?stencil_layout:layout;
            const auto backing=plane?stencil_bytes:bytes;
            if(physical.guest_bytes>UINT32_MAX-3)throw std::out_of_range("Native tiling upload exceeds guest buffer addressing");
            const auto size=(physical.guest_bytes+3)&~uint64_t(3);
            if(gpu_snapshot){
                if(plane)throw std::logic_error("GPU image snapshot has an unexpected stencil plane");
                if(entry->tiler_input_pool.empty())entry->tiler_input_pool.push_back(*gpu_snapshot);
                else entry->tiler_input_pool[0]=*gpu_snapshot;
                continue;
            }
            if(plane<entry->tiler_input_pool.size()){
                D3D12_HEAP_PROPERTIES heap{};D3D12_HEAP_FLAGS flags{};
                Check(entry->tiler_input_pool[plane].handle->GetHeapProperties(&heap,&flags));
                if(heap.Type!=D3D12_HEAP_TYPE_UPLOAD)entry->tiler_input_pool[plane]=
                    Buffer(device_.Get(),size,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
            }
            if(plane==entry->tiler_input_pool.size())entry->tiler_input_pool.push_back(
                Buffer(device_.Get(),size,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ));
            auto& raw=entry->tiler_input_pool.at(plane);
            if(raw.handle->GetDesc().Width<size)throw std::logic_error("Native raw tiling upload pool mismatch");
            void* mapped{};D3D12_RANGE empty{};Check(DX12_API_CALL(raw.handle->Map(0,&empty,&mapped)));
            memcpy(mapped,backing.data(),size_t(physical.guest_bytes));
            if(size>physical.guest_bytes)memset(static_cast<std::byte*>(mapped)+physical.guest_bytes,0,size_t(size-physical.guest_bytes));
            DX12_API_CALL(raw.handle->Unmap(0,nullptr));
        }
        size_t output_index{};
        for(uint32_t plane=0;plane<planes;++plane)
        for(uint32_t layer=0;layer<d.layers;++layer)for(uint32_t mip=0;mip<d.levels;++mip){
            const auto& physical=plane?stencil_layout:layout;
            const auto& interpreted=plane?stencil_desc:d;
            const UINT sub=mip+layer*d.levels+plane*d.levels*d.layers;
            UINT rows{};UINT64 row_bytes{},total{};D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};
            if(d.samples>1){fp=GuestMultisample::Footprint(interpreted,physical);
                rows=d.height;row_bytes=uint64_t(d.width)*physical.HostBytesPerPixel();
                total=GuestMultisample::LinearBytes(interpreted,fp);}
            else DX12_API_CALL(device_->GetCopyableFootprints(&resource_desc,sub,1,0,&fp,&rows,&row_bytes,&total));
            const auto width=(std::max(d.width>>mip,1u)+physical.block_extent-1)/physical.block_extent;
            if(!width||row_bytes%width)throw std::logic_error("Native tiling format footprint mismatch");
            if(output_index==entry->tiler_output_pool.size())entry->tiler_output_pool.push_back(GuestTiler::Scratch(device_.Get(),total));
            auto& linear=entry->tiler_output_pool.at(output_index++);
            if(linear.handle->GetDesc().Width<total)throw std::logic_error("Native linear tiling pool mismatch");
            auto tiling=tiler_->Prepare(entry->tiler_input_pool.at(plane),linear,linear,
                interpreted,physical,mip,d.volume_depth?0:layer,fp,uint32_t(row_bytes/width),false);
            entry->tiler_leases.push_back(tiling);
            if(d.samples>1){
                auto transfer=multisample_->Prepare(entry->texture,linear,d,layout,layer,plane,true);
                entry->multisample_leases.push_back(transfer);
                tiling->Record(list,entry->tiler_input_pool.at(plane),linear,linear);
                transfer->Record(list,entry->texture,linear);continue;
            }
            tiling->Record(list,entry->tiler_input_pool.at(plane),linear,linear);
            linear.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
            D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=linear.handle.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=fp;
            D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=entry->texture.handle.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dst.SubresourceIndex=sub;
            DX12_API_CALL(list->CopyTextureRegion(&dst,0,0,0,&src,nullptr));
        }
    }else{
    size_t upload_index{};
    const auto expand_sampled=[&](std::vector<std::byte> linear,uint32_t mip){
        if(!layout.expanded_bytes_per_pixel)return linear;
        if(d.data_format==9)return ExpandSignedPacked10(linear,d.number_format);
        return bc_decoder_->Expand(d.data_format,d.number_format,
            std::max(d.width>>mip,1u),std::max(d.height>>mip,1u),linear);
    };
    for(uint32_t plane=0;plane<(d.stencil_address?2u:1u);++plane)
    for(uint32_t layer=0;layer<d.layers;++layer)for(uint32_t mip=0;mip<d.levels;++mip){
        diagnostic.Stage("cpu-detile");
        auto linear=plane?DecodeGuestSubresource(stencil_desc,entry->stencil_layout,stencil_bytes,mip,layer):DecodeGuestSubresource(d,entry->layout,bytes,mip,layer);
        linear=expand_sampled(std::move(linear),mip);
        if(!plane&&d.stencil_address&&layout.bytes_per_pixel==2){
            std::vector<std::byte> converted(linear.size()*2);
            for(size_t i=0;i<linear.size()/2;++i){uint16_t v;memcpy(&v,linear.data()+i*2,2);float f=float(v)/65535.f;memcpy(converted.data()+i*4,&f,4);}
            linear=std::move(converted);
        }
        diagnostic.Stage("upload-allocation-and-copy");
        UINT sub=mip+layer*d.levels+plane*d.levels*d.layers,rows;UINT64 row_bytes,total;D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};
        DX12_API_CALL(device_->GetCopyableFootprints(&resource_desc,sub,1,0,&fp,&rows,&row_bytes,&total));
        if(upload_index==entry->upload_pool.size())entry->upload_pool.push_back(
            Buffer(device_.Get(),total,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ));
        auto upload=entry->upload_pool.at(upload_index++);
        if(DX12_API_CALL(upload.handle->GetDesc()).Width<total)throw std::logic_error("Native upload pool footprint mismatch");
        void* mapped=nullptr;D3D12_RANGE empty{0,0};Check(DX12_API_CALL(upload.handle->Map(0,&empty,&mapped)));
        for(UINT z=0;z<fp.Footprint.Depth;++z){
            if(d.volume_depth&&z){
                linear=DecodeGuestSubresource(d,layout,bytes,mip,z);
                linear=expand_sampled(std::move(linear),mip);
            }
            for(UINT row=0;row<rows;++row)memcpy(static_cast<std::byte*>(mapped)+fp.Offset+(UINT64(z)*rows+row)*fp.Footprint.RowPitch,linear.data()+UINT64(row)*row_bytes,size_t(row_bytes));
        }DX12_API_CALL(upload.handle->Unmap(0,nullptr));
        D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=upload.handle.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=fp;D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=entry->texture.handle.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dst.SubresourceIndex=sub;
        if(copy_upload)copy_batch.push_back({dst,src});
        else DX12_API_CALL(list->CopyTextureRegion(&dst,0,0,0,&src,nullptr));
        entry->uploads.push_back(std::move(upload));}
    }
    if(!copy_batch.empty())submit_copy_([copies=std::move(copy_batch)](ID3D12GraphicsCommandList* copy_list){
        for(const auto& copy:copies)DX12_API_CALL(copy_list->CopyTextureRegion(
            &copy.destination,0,0,0,&copy.source,nullptr));
    });
    diagnostic.Stage("cpu-coherence-snapshot");
    if(immutable_view&&!gpu_snapshot)entry->RememberSnapshot(bytes,stencil_bytes);
    if(immutable_view&&!guest_snapshot&&!d.render_target&&!d.depth_target&&!d.stencil_address&&watch_immutable_)watch_immutable_(*entry);
    entry->texture.Transition(list,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);entry->pending=true;return *entry;
}
void GuestTextureCache::Submitted(uint64_t fence){if(!fence)throw std::invalid_argument("zero texture fence");for(auto& e:entries_)if(e->pending){e->pending=false;e->fence=fence;}}
void GuestTextureCache::Used(Entry& entry,uint64_t fence){if(!fence||entry.pending||fence<entry.fence)throw std::invalid_argument("invalid texture use fence");entry.fence=fence;}
Resource GuestTextureCache::RetileBufferVersion(Entry& image,Resource& initial,
    ID3D12GraphicsCommandList* list,uint64_t completed){
    auto ownership_desc=image.desc;ownership_desc.writable=true;ownership_desc.render_target=false;
    const auto ownership_layout=LayoutGuestTexture(ownership_desc);
    if(!list||!image.gpu_owned||!image.buffer_alias_version||!CanRetainBufferAlias(ownership_desc,ownership_layout)||
       ownership_layout.guest_bytes!=image.layout.guest_bytes||
       !initial.handle||initial.handle->GetDesc().Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||
       initial.handle->GetDesc().Width<image.layout.guest_bytes)
        throw std::invalid_argument("Unsupported independent image formatted-buffer geometry");
    const auto resource_desc=image.texture.handle->GetDesc();
    const auto linear_bytes=image.layout.HostBytesPerPixel();
    const bool inherited=image.layout.expanded_bytes_per_pixel!=0;
    if(inherited&&(!HasExactPackedExpansion(image.desc,image.layout)||
       !image.expanded_guest_backing.handle||!image.expanded_written_mask.handle||!image.expanded_backing_bytes))
        throw std::invalid_argument("Independent expanded image lost its exact packed backing");
    struct Linear {D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;UINT64 bytes;UINT mip,layer;Resource buffer;
        std::shared_ptr<GuestTiler::Lease> dispatch;};
    std::vector<Linear> linear;linear.reserve(size_t(image.desc.levels)*image.desc.layers);
    if(!inherited)for(UINT layer=0;layer<image.desc.layers;++layer)for(UINT mip=0;mip<image.desc.levels;++mip){
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 bytes{},row_bytes{};
        DX12_API_CALL(device_->GetCopyableFootprints(&resource_desc,mip+layer*image.desc.levels,
            1,0,&footprint,nullptr,&row_bytes,&bytes));
        if(row_bytes!=u64(std::max(image.desc.width>>mip,1u))*linear_bytes||!bytes||bytes>UINT32_MAX)
            throw std::logic_error("Independent image transfer footprint changed");
        linear.push_back({footprint,bytes,mip,layer,{},{}});
    }
    const auto mask_bytes=GuestTiler::WrittenBytes(image.layout.guest_bytes);
    const auto allocation_bytes=[&](uint64_t length,bool uav){
        D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width=length;desc.Height=desc.DepthOrArraySize=desc.MipLevels=1;
        desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags=uav?D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS:D3D12_RESOURCE_FLAG_NONE;
        const auto allocation=DX12_API_CALL(device_->GetResourceAllocationInfo(0,1,&desc));
        if(!allocation.SizeInBytes||allocation.SizeInBytes==UINT64_MAX)
            throw std::runtime_error("Invalid formatted image transfer allocation requirements");
        return allocation.SizeInBytes;
    };
    auto reservation=allocation_bytes(image.layout.guest_bytes,true)+
        (inherited?0:allocation_bytes(mask_bytes,true))+allocation_bytes(initial.handle->GetDesc().Width,false);
    for(const auto& slice:linear)reservation+=allocation_bytes((slice.bytes+3)&~u64(3),false);
    ReserveMemory(reservation,completed,&image);
    if(!image.tiler)image.tiler=std::make_shared<GuestTiler>(device_.Get());
    auto output=GuestTiler::Scratch(device_.Get(),image.layout.guest_bytes);
    auto mask=inherited?image.expanded_written_mask:GuestTiler::Scratch(device_.Get(),mask_bytes);
    auto merge=inherited?image.tiler->PrepareMaskedMerge(image.expanded_guest_backing,output,mask,image.layout.guest_bytes):
        std::shared_ptr<GuestTiler::Lease>{};
    for(auto& slice:linear){
        slice.buffer=Buffer(device_.Get(),(slice.bytes+3)&~u64(3),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
        slice.dispatch=image.tiler->Prepare(slice.buffer,output,mask,image.desc,image.layout,
            slice.mip,slice.layer,slice.footprint,linear_bytes,true);
    }
    // Retain all resources/descriptors and account transient bytes before recording.
    image.buffer_sync_resources.reserve(image.buffer_sync_resources.size()+linear.size()+3+(inherited?1:0));
    image.tiler_leases.reserve(image.tiler_leases.size()+linear.size()+(inherited?1:0));
    image.buffer_sync_resources.insert(image.buffer_sync_resources.end(),{output,mask,initial});
    if(inherited){image.buffer_sync_resources.push_back(image.expanded_guest_backing);image.tiler_leases.push_back(merge);}
    for(const auto& slice:linear){image.buffer_sync_resources.push_back(slice.buffer);
        image.tiler_leases.push_back(slice.dispatch);}
    image.buffer_sync_allocation_bytes+=reservation;
    image.pending=true;
    if(image.texture.state==D3D12_RESOURCE_STATE_UNORDERED_ACCESS)image.texture.UavBarrier(list);
    image.texture.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
    initial.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);output.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
    DX12_API_CALL(list->CopyBufferRegion(output.handle.Get(),0,initial.handle.Get(),0,image.layout.guest_bytes));
    if(inherited)merge->Record(list,image.expanded_guest_backing,output,image.expanded_written_mask);
    for(auto& slice:linear){
        D3D12_TEXTURE_COPY_LOCATION source{};source.pResource=image.texture.handle.Get();
        source.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;source.SubresourceIndex=slice.mip+slice.layer*image.desc.levels;
        D3D12_TEXTURE_COPY_LOCATION destination{};destination.pResource=slice.buffer.handle.Get();
        destination.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;destination.PlacedFootprint=slice.footprint;
        DX12_API_CALL(list->CopyTextureRegion(&destination,0,0,0,&source,nullptr));
        slice.dispatch->Record(list,slice.buffer,output,mask);
    }
    output.UavBarrier(list);
    return output;
}
void GuestTextureCache::Collect(uint64_t completed){
    // Completed fence usually stays constant across thousands of bindings in
    // one PS4 packet. Do the O(cache entries) retirement scan once per advance.
    if(completed<=collected_fence_)return;collected_fence_=completed;
    for(auto& e:entries_)if(!e->pending&&e->fence<=completed){
        e->uploads.clear();e->tiler_leases.clear();e->multisample_leases.clear();e->sample_copy_leases.clear();e->storage_leases.clear();
        e->storage_clear_leases.clear();
        e->buffer_sync_resources.clear();
        e->buffer_sync_allocation_bytes=0;
        e->format_transfers.clear();e->allocation_bytes-=e->format_retirement_bytes;e->format_retirement_bytes=0;
    }
}
Resource* GuestTextureCache::CoherentDisplayTexture(uint64_t address,UINT width,UINT height,UINT pitch,uint64_t completed){
    auto [first,last]=address_index_.equal_range(address);for(auto indexed=first;indexed!=last;++indexed){auto& e=*indexed->second;
        const auto& d=e.desc;
        if(d.address!=address||d.width!=width||d.height!=height||d.pitch!=pitch||
           d.tile_mode!=10||d.data_format!=10||(d.number_format!=0&&d.number_format!=9)||
           d.writable||d.depth_target||d.stencil_address||d.volume_depth||d.one_dimensional||
           d.levels!=1||d.layers!=1||d.samples!=1||d.swizzle!=std::array<uint32_t,4>{4,5,6,7}||
           e.pending||e.fence>completed||(!e.gpu_owned&&e.sampled_snapshot.size()!=e.layout.guest_bytes))continue;
        if(!e.gpu_owned&&memcmp(e.sampled_snapshot.data(),reinterpret_cast<const void*>(address),size_t(e.layout.guest_bytes)))continue;
        e.last_use=++use_sequence_;return &e.texture;
    }
    return nullptr;
}
void GuestTextureCache::Invalidate(uint64_t a,uint64_t s,uint64_t completed){if(!s||a>UINT64_MAX-s)throw std::invalid_argument("invalid texture invalidation");
    const auto matches=[&](const Entry& e){return (e.desc.address<a+s&&a<e.desc.address+e.layout.guest_bytes)||
        (e.desc.stencil_address&&e.desc.stencil_address<a+s&&a<e.desc.stencil_address+e.stencil_layout.guest_bytes);};
    for(const auto& e:entries_)if(matches(*e)&&(e->BindingPinned()||e->pending||e->fence>completed))throw std::runtime_error("texture invalidation before binding/GPU retirement");
    const auto before=entries_.size();std::erase_if(entries_,[&](const auto& e){const bool remove=matches(*e);
        if(remove&&e->cpu_watched&&release_immutable_)release_immutable_(*e);return remove;});
    if(entries_.size()!=before)RebuildIndex();}
}


