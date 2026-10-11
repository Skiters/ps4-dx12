#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_texture_cache.h"
#include "video_core/amdgpu/resource.h"
#include "video_core/amdgpu/regs_color.h"
#include "video_core/amdgpu/regs_depth.h"
#include "video_core/amdgpu/cb_db_extent.h"
#include "video_core/amdgpu/tiling.h"
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
    case 14:return DXGI_FORMAT_R32G32B32A32_TYPELESS;
    default:return fallback;
    }
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
GuestTextureCache::GuestTextureCache(ID3D12Device* device):device_(device),texture_allocator_(device) {
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
        std::vector<uint32_t> bases;
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
    source.stencil_address=0;
    source.render_target=source.depth_target=false;
    source.storage_height=sampled.storage_height=0;
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
    if(source.data_format==view.data_format&&
       WritableStorageFormat(source.data_format,DXGI_FORMAT_UNKNOWN)!=DXGI_FORMAT_UNKNOWN)
        source.number_format=view.number_format=0;
    return source==view;
}
bool CompatibleGuestShaderStorage(GuestTextureDesc source,GuestTextureDesc view) {
    // A writable member causes the group to allocate typeless UAV-capable
    // storage. A purely sampled group still uses its original typed resource.
    if(!source.writable&&!view.writable&&source.number_format!=view.number_format)return false;
    source.writable=view.writable=true;
    source.swizzle=view.swizzle={4,5,6,7};
    return CompatibleGuestWritableStorage(source,view);
}
namespace {
uint32_t Align(uint32_t n,uint32_t a){return (n+a-1)&~(a-1);}
uint32_t Pow2(uint32_t n){return std::bit_ceil(n);}
struct MacroProperties {uint32_t bank_height,aspect,banks,split;};
MacroProperties Macro(uint32_t tile,uint32_t bpp,uint32_t samples=1) {
    // Base PS4 table, shared with AmdGpu::CalculateMacrotileMode.
    // Depth modes select a hardware split; color selects two samples per split.
    const uint32_t split=tile<=4?64u<<tile:std::min(1024u,std::max(256u,128*bpp));
    const auto index=std::bit_width(std::min(split,64*bpp*samples)/64)-1;
    constexpr uint32_t height[]{4,2,1,1,1},aspect[]{4,2,2,2,1},banks[]{16,16,16,16,8};
    if(index>=5)throw std::out_of_range("Unsupported native macro tile mode");
    return {height[index],aspect[index],banks[index],split};
}
std::pair<DXGI_FORMAT,uint32_t> Format(uint32_t data,uint32_t number){
    if(data==1){switch(number){case 0:return {DXGI_FORMAT_R8_UNORM,1};case 1:return {DXGI_FORMAT_R8_SNORM,1};case 4:return {DXGI_FORMAT_R8_UINT,1};case 5:return {DXGI_FORMAT_R8_SINT,1};}}
    if(data==3){switch(number){case 0:return {DXGI_FORMAT_R8G8_UNORM,2};case 1:return {DXGI_FORMAT_R8G8_SNORM,2};case 4:return {DXGI_FORMAT_R8G8_UINT,2};case 5:return {DXGI_FORMAT_R8G8_SINT,2};}}
    if(data==10){switch(number){case 0:return {DXGI_FORMAT_R8G8B8A8_UNORM,4};case 1:return {DXGI_FORMAT_R8G8B8A8_SNORM,4};case 4:return {DXGI_FORMAT_R8G8B8A8_UINT,4};case 5:return {DXGI_FORMAT_R8G8B8A8_SINT,4};case 9:return {DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,4};}}
    if(data==4){switch(number){case 7:return {DXGI_FORMAT_R32_FLOAT,4};case 4:return {DXGI_FORMAT_R32_UINT,4};case 5:return {DXGI_FORMAT_R32_SINT,4};}}
    if(data==2){switch(number){case 0:return {DXGI_FORMAT_R16_UNORM,2};case 1:return {DXGI_FORMAT_R16_SNORM,2};case 4:return {DXGI_FORMAT_R16_UINT,2};case 5:return {DXGI_FORMAT_R16_SINT,2};case 7:return {DXGI_FORMAT_R16_FLOAT,2};}}
    if(data==5){switch(number){case 0:return {DXGI_FORMAT_R16G16_UNORM,4};case 1:return {DXGI_FORMAT_R16G16_SNORM,4};case 4:return {DXGI_FORMAT_R16G16_UINT,4};case 5:return {DXGI_FORMAT_R16G16_SINT,4};case 7:return {DXGI_FORMAT_R16G16_FLOAT,4};}}
    if(data==6&&number==7)return {DXGI_FORMAT_R11G11B10_FLOAT,4};
    if(data==9){if(number==0)return {DXGI_FORMAT_R10G10B10A2_UNORM,4};if(number==4)return {DXGI_FORMAT_R10G10B10A2_UINT,4};}
    if(data==34&&number==7)return {DXGI_FORMAT_R9G9B9E5_SHAREDEXP,4};
    if(data==11){switch(number){case 4:return {DXGI_FORMAT_R32G32_UINT,8};case 5:return {DXGI_FORMAT_R32G32_SINT,8};case 7:return {DXGI_FORMAT_R32G32_FLOAT,8};}}
    if(data==12){switch(number){case 0:return {DXGI_FORMAT_R16G16B16A16_UNORM,8};case 1:return {DXGI_FORMAT_R16G16B16A16_SNORM,8};case 4:return {DXGI_FORMAT_R16G16B16A16_UINT,8};case 5:return {DXGI_FORMAT_R16G16B16A16_SINT,8};case 7:return {DXGI_FORMAT_R16G16B16A16_FLOAT,8};}}
    if(data==14){switch(number){case 4:return {DXGI_FORMAT_R32G32B32A32_UINT,16};case 5:return {DXGI_FORMAT_R32G32B32A32_SINT,16};case 7:return {DXGI_FORMAT_R32G32B32A32_FLOAT,16};}}
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
UINT Mapping(const std::array<uint32_t,4>& s){UINT result=D3D12_SHADER_COMPONENT_MAPPING_ALWAYS_SET_BIT_AVOIDING_ZEROMEM_MISTAKES;for(UINT i=0;i<4;++i){UINT c;if(s[i]<=1)c=s[i]+4;else if(s[i]>=4&&s[i]<=7)c=s[i]-4;else throw std::runtime_error("invalid guest component swizzle");result|=c<<(i*D3D12_SHADER_COMPONENT_MAPPING_SHIFT);}return result;}
}
GuestTextureDesc DescribeGuestTexture(const AmdGpu::Image& image){
    const bool one_dimensional=image.GetBaseType()==AmdGpu::ImageType::Color1D;
    const bool volume=image.type==uint64_t(AmdGpu::ImageType::Color3D);
    const bool cube=image.GetType()==AmdGpu::ImageType::Cube;
    if(!one_dimensional&&!volume&&!cube&&image.type!=uint64_t(AmdGpu::ImageType::Color2D)&&image.type!=uint64_t(AmdGpu::ImageType::Color2DArray))
        throw std::runtime_error("Native image type="+std::to_string(image.type)+" format="+
            std::to_string(image.data_format)+" address="+std::to_string(image.Address())+
            " dimensions="+std::to_string(image.width+1)+"x"+std::to_string(image.height+1)+
            "x"+std::to_string(image.depth+1)+" tile="+std::to_string(image.tiling_index)+
            " needs native resource/view support");
    if(image.compression_en||image.alt_tile_mode||image.interlaced)throw std::runtime_error("compressed/alternate/interlaced guest image unsupported");
    GuestTextureDesc d;d.address=image.base_address<<8;d.width=uint32_t(image.width)+1;d.height=uint32_t(image.height)+1;d.pitch=uint32_t(image.pitch)+1;
    d.one_dimensional=one_dimensional;if(one_dimensional)d.height=1;
    if(volume)d.volume_depth=uint32_t(image.depth)+1;
    d.levels=uint32_t(image.last_level)+1;
    d.min_lod=uint32_t(image.min_lod);
    d.layers=image.NumLayers();
    d.pow2_padding=image.pow2pad;d.data_format=uint32_t(image.GetDataFmt());d.number_format=uint32_t(image.GetNumberFmt());d.tile_mode=uint32_t(image.tiling_index);
    d.first_mip=uint32_t(image.base_level);d.first_layer=std::min(uint32_t(image.base_array),d.layers-1);d.view_layers=image.NumViewLayers(true);
    if(volume){d.first_layer=0;d.view_layers=1;}
    if(d.pow2_padding)d.layers=Pow2(d.layers);d.swizzle={uint32_t(image.dst_sel_x),uint32_t(image.dst_sel_y),uint32_t(image.dst_sel_z),uint32_t(image.dst_sel_w)};
    LayoutGuestTexture(d);return d;
}
GuestTextureDesc DescribeGuestColorBuffer(const AmdGpu::ColorBuffer& buffer,AmdGpu::CbDbExtent extent) {
    if(!buffer||buffer.NumSamples()!=1||buffer.info.compression||buffer.info.dcc_enable||buffer.info.alt_tile_mode)
        throw std::runtime_error("Native guest color buffer needs valid single-sample uncompressed base tiling: address="+
            std::to_string(buffer.Address())+" samples="+std::to_string(buffer.NumSamples())+
            " compression="+std::to_string(buffer.info.compression)+" dcc="+std::to_string(buffer.info.dcc_enable)+
            " alt="+std::to_string(buffer.info.alt_tile_mode)+" linear="+std::to_string(buffer.info.linear_general)+
            " fast_clear="+std::to_string(buffer.info.fast_clear)+" cmask="+std::to_string(buffer.CmaskAddress())+
            " format="+std::to_string(buffer.info.format)+" number="+std::to_string(buffer.info.number_type));
    GuestTextureDesc desc;desc.address=buffer.Address();desc.width=extent.Valid()?extent.width:buffer.Pitch();
    desc.height=extent.Valid()?extent.height:buffer.Height();desc.pitch=buffer.Pitch();desc.layers=buffer.NumSlices();
    desc.storage_height=buffer.Height();
    desc.first_layer=buffer.BaseSlice();
    if(desc.first_layer>=desc.layers)throw std::out_of_range("Invalid guest color buffer layer view");
    desc.view_layers=desc.layers-desc.first_layer;desc.data_format=uint32_t(buffer.GetDataFmt());
    desc.number_format=uint32_t(buffer.GetNumberFmt());desc.tile_mode=uint32_t(buffer.GetTileMode());desc.render_target=true;
    const auto layout=LayoutGuestTexture(desc);
    if(layout.guest_bytes!=uint64_t(buffer.slice.tile_max+1)*64*layout.bytes_per_pixel*desc.layers)
        throw std::runtime_error("Native guest color buffer padded storage extent needs explicit layout");
    return desc;
}
// Single-sample GCN CMASK: 0 selects CB_CLEAR_WORD, F selects expanded color.
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
// AMD AddrLib SiLib::HwlComputeXmaskAddrFromCoord and
// TileCoordToMaskElementIndex, specialized for base PS4 P8_32x32_16x16.
// Tile-mode indices are NOT pipe configurations.  Eight 256-byte pipe lanes
// interleave 4-bit elements for 8x8 color tiles. A cache tile is 512x256 pixels;
// the complete metadata slice is padded to the 2048-byte pipe-group alignment.
struct CmaskLayout {
    uint32_t pitch,height;
    uint64_t slice_bytes;
    bool linear;
    explicit CmaskLayout(const AmdGpu::ColorBuffer& b):linear(b.info.cmask_is_linear) {
        const auto tile=uint32_t(b.GetTileMode());
        if(b.info.alt_tile_mode||b.info.cmask_addr_type||
           !(tile<=10||tile==12||tile==13||tile==14||tile==17||tile==18||tile==19||tile==20||tile==23||tile==25))
            throw std::runtime_error("Native CMASK requires base PS4 P8_32x32_16x16 addressing");
        const uint32_t mw=linear?64:512,mh=linear?64:256;
        pitch=Align(b.Pitch(),mw);height=Align(b.Height(),mh);
        while((uint64_t(pitch)*height/128)%2048)height+=mh;
        slice_bytes=uint64_t(pitch)*height/128;
        if(slice_bytes!=(uint64_t(b.cmask_slice.tile_max)+1)*128)
            throw std::runtime_error("Native CMASK descriptor does not match padded metadata slice");
    }
    CmaskTile At(uint32_t layer,uint32_t x,uint32_t y) const {
        const uint32_t tx=x/8,ty=y/8;
        const uint32_t pipe=((tx^ty^(tx>>1))&1)|((((tx>>1)^(ty>>1))&1)<<1)|
            ((((tx>>2)^(ty>>2))&1)<<2);
        const uint32_t elem=(((tx>>1)^ty)&1)|(tx&2);
        uint64_t micro,macro_bits;
        if(linear) {
            micro=uint64_t(tx/4+(ty/4)*(pitch/32))*2;
            macro_bits=uint64_t(layer)*(pitch/8)*(height/8)/8*4;
        } else {
            micro=uint64_t((x%512)/32+((y%256)/32)*16)*2;
            const auto macro=uint64_t(x/512)+uint64_t(y/256)*(pitch/512)+
                uint64_t(layer)*(pitch/512)*(height/256);
            macro_bits=macro*1024;
        }
        micro=(micro&~3ull)+elem;
        const uint64_t bits=macro_bits+micro*4;
        const uint64_t byte=(bits%2048)/8+uint64_t(pipe)*256+(bits/2048)*2048;
        if(byte<layer*slice_bytes||byte>=(layer+1)*slice_bytes)
            throw std::out_of_range("Native CMASK tile exceeds metadata slice");
        return {layer,x,y,byte,uint32_t(bits%8)};
    }
};
CmaskClearPlan PlanGuestColorClear(const AmdGpu::ColorBuffer& b,const GuestTextureDesc& d,
    const GuestTextureLayout& l,std::span<const std::byte> cmask) {
    CmaskClearPlan plan;
    if(!b.info.fast_clear||!b.cmask_base_address)return plan;
    if(b.NumSamples()!=1||b.info.compression||b.info.dcc_enable||l.block_extent!=1)
        throw std::runtime_error("Native CMASK requires uncompressed single-sample color");
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
        const CmaskLayout layout(b);
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
    const auto* pixel=reinterpret_cast<const std::byte*>(l.bytes_per_pixel==16?wide:words);
    const auto slice_bytes=GuestCmaskBytes(b)/b.NumSlices();
    if(!plan.uniform_layers.empty()) {
        std::vector<std::byte> linear(size_t(d.width)*d.height*l.bytes_per_pixel);
        for(size_t offset=0;offset<linear.size();offset+=l.bytes_per_pixel)memcpy(linear.data()+offset,pixel,l.bytes_per_pixel);
        for(auto layer:plan.uniform_layers) {
            EncodeGuestSubresource(d,l,color,linear,0,layer);
            std::fill_n(cmask.begin()+size_t(slice_bytes*layer),size_t(slice_bytes),std::byte{0xff});
        }
    }
    for(const auto& tile:plan.tiles) {
        for(uint32_t y=tile.y;y<std::min(tile.y+8,d.height);++y)
        for(uint32_t x=tile.x;x<std::min(tile.x+8,d.width);++x) {
            const auto offset=GuestPixelOffset(d,l,0,tile.layer,x,y);
            memcpy(color.data()+offset,pixel,l.bytes_per_pixel);
        }
        cmask[tile.byte]|=std::byte(15u<<tile.shift);
    }
    return true;
}
GuestTextureLayout LayoutGuestTexture(const GuestTextureDesc& d){
    if(!std::has_single_bit(d.samples)||d.samples>16)
        throw std::invalid_argument("Invalid guest sample count");
    if(d.samples>1&&(d.levels!=1||d.volume_depth||d.one_dimensional||
       (d.tile_mode>5&&d.tile_mode!=13&&d.tile_mode!=14)||d.data_format>=35))
        throw std::invalid_argument("MSAA layout requires uncompressed thin/depth 2D storage without mips");
    if(d.volume_depth&&(d.width>2048||d.height>2048||d.volume_depth>2048||d.layers!=1||d.one_dimensional||d.render_target||d.depth_target||
       (d.tile_mode!=8&&d.tile_mode!=13&&d.tile_mode!=19&&d.tile_mode!=31)))
        throw std::invalid_argument("Native 3D texture requires linear/thin/thick 1D volume tiling");
    if(d.one_dimensional&&(d.height!=1||d.render_target||d.depth_target))
        throw std::invalid_argument("Invalid native 1D texture extent/attachment usage");
    // Array slices never extend a mip chain, but volume depth does. D3D12
    // reduces all three spatial dimensions for each TEXTURE3D mip level.
    const auto spatial_extent=std::max({d.width,d.height,d.volume_depth});
    if(!d.width||!d.height||d.width>16384||d.height>16384||d.pitch<d.width||d.pitch>32768||!d.layers||d.layers>2048||!d.levels||d.levels>15||d.levels>uint32_t(std::bit_width(spatial_extent)))throw std::runtime_error("invalid guest dimensions/mips/layers");
    if(d.storage_height&&(d.storage_height<d.height||d.storage_height>32768||d.levels!=1))
        throw std::out_of_range("Invalid guest attachment physical height");
    if(d.tile_mode>5&&d.tile_mode!=8&&d.tile_mode!=9&&d.tile_mode!=10&&d.tile_mode!=13&&d.tile_mode!=14&&
       d.tile_mode!=31&&!(d.volume_depth&&d.tile_mode==19))
        throw std::runtime_error("unsupported guest tiling: linear/display/thin/depth 1D/2D or thick1D volume only");
    if(d.first_mip>=d.levels||d.first_layer>=d.layers||!d.view_layers||d.view_layers>d.layers-d.first_layer)throw std::runtime_error("invalid guest texture view range");
    // Storage image instructions access normalized raw channels. As in the
    // original guest view interpretation, sRGB decoding applies only to reads
    // through sampled views, never to storage UAV views.
    Mapping(d.swizzle);auto [format,bpp]=Format(d.data_format,d.writable&&d.number_format==9?0:d.number_format);GuestTextureLayout layout;layout.format=format;layout.bytes_per_pixel=bpp;
    if(d.data_format>=35&&d.data_format<=41){
        if(d.writable||d.render_target||d.depth_target||d.one_dimensional||d.volume_depth)
            throw std::invalid_argument("Native BC texture requires a sampled 2D image");
        layout.block_extent=4;
    }
    for(uint32_t mip=0;mip<d.levels;++mip){uint32_t pitch=std::max(d.pitch>>mip,1u),height=std::max((d.storage_height?d.storage_height:d.height)>>mip,1u);
        pitch=(pitch+layout.block_extent-1)/layout.block_extent;height=(height+layout.block_extent-1)/layout.block_extent;
        if(d.pow2_padding){pitch=Pow2(pitch);height=Pow2(height);}
        const auto props=Macro(d.tile_mode,bpp,d.samples);
        const auto macro_width=64*props.aspect,macro_height=8*props.banks*props.bank_height/props.aspect;
        const bool macro=(d.tile_mode<=4||d.tile_mode==10||d.tile_mode==14)&&(mip==0||(pitch>=macro_width&&height>=macro_height));
        if(macro){pitch=Align(pitch,macro_width);height=Align(height,macro_height);}
        else if(d.tile_mode==31){/* DISPLAY_LINEAR_GENERAL keeps the declared pitch and height. */}
        else if(d.tile_mode==8){auto pa=std::max(8u,64u/bpp);pitch=Align(pitch,pa);auto sa=std::max(64u,256u/bpp);while((uint64_t(pitch)*height)%sa)pitch+=pa;}
        else{pitch=Align(pitch,8);height=Align(height,8);while((uint64_t(pitch)*height*bpp)%256)pitch+=8;}
        const auto depth=d.volume_depth?std::max(d.volume_depth>>mip,1u):d.layers;
        const auto stored_depth=d.volume_depth&&d.tile_mode==19?Align(depth,4):depth;
        uint64_t slice=uint64_t(pitch)*height*bpp*d.samples;layout.mips.push_back({pitch,height,layout.guest_bytes,slice,macro});layout.guest_bytes+=slice*stored_depth;
        layout.logical_bytes+=uint64_t((std::max(d.width>>mip,1u)+layout.block_extent-1)/layout.block_extent)*
            ((std::max(d.height>>mip,1u)+layout.block_extent-1)/layout.block_extent)*bpp*depth*d.samples;
        if(layout.guest_bytes>128*1024*1024)throw std::runtime_error("guest texture exceeds 128MiB upload guard");
    }if(d.address>UINT64_MAX-layout.guest_bytes)throw std::runtime_error("guest texture address overflow");
    layout.copy_plans=std::make_shared<GuestTextureCopyPlans>();return layout;
}
GuestTextureDesc DescribeGuestDepthBuffer(const AmdGpu::DepthBuffer& buffer,
    const AmdGpu::DepthView& view,AmdGpu::CbDbExtent extent) {
    if(!buffer.DepthValid()||buffer.NumSamples()!=1||buffer.z_info.tile_mode_index>5)
        throw std::runtime_error("Native guest depth needs valid single-sample base tiling");
    GuestTextureDesc desc;desc.address=buffer.DepthAddress();desc.width=extent.Valid()?extent.width:buffer.Pitch();
    desc.height=extent.Valid()?extent.height:buffer.Height();desc.pitch=buffer.Pitch();desc.layers=view.NumSlices();
    desc.storage_height=buffer.Height();
    desc.first_layer=view.slice_start;desc.view_layers=desc.layers-desc.first_layer;desc.depth_target=true;
    if(buffer.StencilValid())desc.stencil_address=buffer.StencilAddress();
    desc.tile_mode=buffer.z_info.tile_mode_index;
    if(buffer.z_info.format==AmdGpu::DepthBuffer::ZFormat::Z32Float){desc.data_format=4;desc.number_format=7;}
    else if(buffer.z_info.format==AmdGpu::DepthBuffer::ZFormat::Z16){desc.data_format=2;desc.number_format=0;}
    else throw std::runtime_error("Unsupported native guest depth format");
    const auto layout=LayoutGuestTexture(desc);
    if(layout.guest_bytes!=u64(buffer.depth_slice.tile_max+1)*64*layout.bytes_per_pixel*desc.layers)
        throw std::runtime_error("Native guest depth allocation differs from declared slice stride");
    return desc;
}
GuestTextureDesc DescribeGuestStencil(const GuestTextureDesc& depth) {
    if(!depth.depth_target||!depth.stencil_address)throw std::invalid_argument("Missing guest stencil plane");
    auto stencil=depth;stencil.address=depth.stencil_address;stencil.stencil_address=0;
    stencil.depth_target=false;stencil.data_format=1;stencil.number_format=4;
    return stencil;
}
uint64_t GuestPixelOffset(const GuestTextureDesc& d,const GuestTextureLayout& l,uint32_t mip,uint32_t layer,uint32_t x,uint32_t y,uint32_t sample){
    if(sample>=d.samples||mip>=d.levels||layer>=(d.volume_depth?std::max(d.volume_depth>>mip,1u):d.layers)||
       x>=(std::max(d.width>>mip,1u)+l.block_extent-1)/l.block_extent||
       y>=(std::max(d.height>>mip,1u)+l.block_extent-1)/l.block_extent)throw std::out_of_range("guest pixel/block coordinates");
    const auto& m=l.mips.at(mip);uint64_t offset=m.offset+layer*m.slice_bytes;
    if(d.tile_mode==8||d.tile_mode==31)return offset+(uint64_t(y)*m.pitch+x)*l.bytes_per_pixel;
    uint32_t p;
    auto b=[](uint32_t v,uint32_t bit){return (v>>bit)&1u;};
    if(d.tile_mode==19) {
        p=b(x,0)|(b(y,0)<<1);
        if(l.bytes_per_pixel<=2)p|=(b(x,1)<<2)|(b(y,1)<<3)|(b(layer,0)<<4)|(b(layer,1)<<5);
        else if(l.bytes_per_pixel==4)p|=(b(x,1)<<2)|(b(layer,0)<<3)|(b(y,1)<<4)|(b(layer,1)<<5);
        else p|=(b(layer,0)<<2)|(b(x,1)<<3)|(b(y,1)<<4)|(b(layer,1)<<5);
        p|=(b(x,2)<<6)|(b(y,2)<<7);
        return m.offset+(layer/4)*m.slice_bytes*4+
            ((uint64_t(y/8)*(m.pitch/8)+x/8)*256+p)*l.bytes_per_pixel;
    }
    if(d.tile_mode<=5||d.tile_mode==13||d.tile_mode==14)p=b(x,0)|(b(y,0)<<1)|(b(x,1)<<2)|(b(y,1)<<3)|(b(x,2)<<4)|(b(y,2)<<5);
    else{switch(l.bytes_per_pixel){case 1:p=b(x,0)|(b(x,1)<<1)|(b(x,2)<<2)|(b(y,1)<<3)|(b(y,0)<<4)|(b(y,2)<<5);break;
        case 2:p=(x&7)|((y&7)<<3);break;case 4:p=b(x,0)|(b(x,1)<<1)|(b(y,0)<<2)|(b(x,2)<<3)|(b(y,1)<<4)|(b(y,2)<<5);break;
        case 8:p=b(x,0)|(b(y,0)<<1)|(b(x,1)<<2)|(b(x,2)<<3)|(b(y,1)<<4)|(b(y,2)<<5);break;default:throw std::runtime_error("display microtile bpp unsupported");}}
    if(m.macro){
        // PS4 base-mode color2D: PipeConfig P8_32x32_16x16, 16 banks,
        // bank width1, thickness1, no alternate-bank swizzle.
        const auto props=Macro(d.tile_mode,l.bytes_per_pixel,d.samples);
        const uint32_t bh=props.bank_height,aspect=props.aspect,mw=64*aspect,mh=8*props.banks*bh/aspect;
        const uint32_t tx=x/8,ty=y/8;
        const uint32_t pipe=(b(tx,0)^b(ty,0)^b(tx,1))|((b(tx,1)^b(ty,1))<<1)|((b(tx,2)^b(ty,2))<<2);
        const uint32_t bx=tx/8,by=ty/bh;
        uint32_t bank;
        if(props.banks==16)bank=(b(bx,0)^b(by,3))|((b(bx,1)^b(by,2)^b(by,3))<<1)|((b(bx,2)^b(by,1))<<2)|((b(bx,3)^b(by,0))<<3);
        else bank=(b(bx,0)^b(by,2))|((b(bx,1)^b(by,1)^b(by,2))<<1)|((b(bx,2)^b(by,0))<<2);
        const uint32_t full_micro_bytes=64*l.bytes_per_pixel*d.samples,micro_bytes=std::min(full_micro_bytes,props.split);
        const uint32_t element=(d.tile_mode<=5?p*d.samples+sample:sample*64+p)*l.bytes_per_pixel;
        const uint32_t split_slice=element/props.split,splits=full_micro_bytes/micro_bytes;
        bank^=((props.banks/2-1)*layer)&(props.banks-1);
        bank^=((props.banks/2+1)*split_slice)&(props.banks-1);
        const uint64_t macro_bytes=micro_bytes*bh;
        const uint64_t per_row=m.pitch/mw,per_slice=per_row*(m.height/mh);
        const uint64_t total=(split_slice+splits*layer)*per_slice*macro_bytes+((y/mh)*per_row+x/mw)*macro_bytes+(ty%bh)*micro_bytes+element%props.split;
        return m.offset+((total&255)|(uint64_t(pipe)<<8)|(uint64_t(bank)<<11)|((total>>8)<<(11+std::countr_zero(props.banks))));
    }
    auto tile=uint64_t(y/8)*(m.pitch/8)+x/8;
    const auto element=d.tile_mode<=5?p*d.samples+sample:sample*64+p;
    return offset+(tile*64*d.samples+element)*l.bytes_per_pixel;
}
namespace {
void CopyTexel(std::byte* dest,const std::byte* source,uint32_t bytes){
    switch(bytes){case 1:*dest=*source;break;case 2:memcpy(dest,source,2);break;case 4:memcpy(dest,source,4);break;
        case 8:memcpy(dest,source,8);break;case 16:memcpy(dest,source,16);break;default:throw std::logic_error("Invalid texel/block size");}
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
    if(d.tile_mode==19){
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
    const auto chunk=ml.macro?std::min({64*bpp,256u,Macro(d.tile_mode,bpp).split}):64*bpp;
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
                    plan->bases[tile_index*plan->pieces+piece]=uint32_t(base);ready[piece]=true;
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
    D3D12_SHADER_RESOURCE_VIEW_DESC view{};view.Format=layout.format;view.Shader4ComponentMapping=Mapping(desc.swizzle);
    const float minimum=std::min(float(desc.min_lod)/256.0f,float(desc.levels-1));
    if(desc.stencil_address)view.Format=DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
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
    if(desc.layers==1){view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2D;view.Texture2D.MipSlice=mip;}
    else {view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2DARRAY;view.Texture2DArray.MipSlice=mip;
        view.Texture2DArray.FirstArraySlice=desc.first_layer;view.Texture2DArray.ArraySize=desc.view_layers;}
    return view;
}
D3D12_DEPTH_STENCIL_VIEW_DESC GuestTextureCache::Entry::Dsv(bool read_only_depth,bool read_only_stencil)const {
    if(!desc.depth_target)throw std::logic_error("Guest image is not a depth attachment");
    D3D12_DEPTH_STENCIL_VIEW_DESC view{};
    view.Format=desc.stencil_address?DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        layout.bytes_per_pixel==4?DXGI_FORMAT_D32_FLOAT:DXGI_FORMAT_D16_UNORM;
    if(desc.layers==1)view.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2D;
    else {view.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
        view.Texture2DArray.FirstArraySlice=desc.first_layer;view.Texture2DArray.ArraySize=desc.view_layers;}
    if(read_only_depth)view.Flags|=D3D12_DSV_FLAG_READ_ONLY_DEPTH;
    if(read_only_stencil&&desc.stencil_address)view.Flags|=D3D12_DSV_FLAG_READ_ONLY_STENCIL;
    return view;
}
void GuestTextureCache::Entry::RememberSnapshot(std::span<const std::byte> bytes,std::span<const std::byte> stencil){
    if(desc.writable||bytes.size()<layout.guest_bytes||
       (desc.stencil_address&&stencil.size()<stencil_layout.guest_bytes))
        throw std::invalid_argument("Invalid coherent texture snapshot backing");
    sampled_snapshot.assign(bytes.begin(),bytes.begin()+size_t(layout.guest_bytes));
    if(desc.stencil_address)stencil_snapshot.assign(stencil.begin(),stencil.begin()+size_t(stencil_layout.guest_bytes));
}
GuestTextureCache::Entry& GuestTextureCache::Upload(const GuestTextureDesc& d,std::span<const std::byte> bytes,ID3D12GraphicsCommandList* list,uint64_t completed,std::span<const std::byte> stencil_bytes){
    if(d.samples!=1)throw std::invalid_argument("MSAA guest cache upload integration is pending");
    Diagnostics::Scope diagnostic(Diagnostics::Kind::Upload,"layout-and-cache-lookup");
    diagnostic.Detail("address=%llx size=%ux%u levels=%u layers=%u tile=%u bytes=%zu",
        (unsigned long long)d.address,d.width,d.height,d.levels,d.layers,d.tile_mode,bytes.size());
    auto layout=LayoutGuestTexture(d);if(bytes.size()<layout.guest_bytes)throw std::invalid_argument("short guest texture upload");Collect(completed);
    if(d.writable)ValidateWritableFormat(layout.format);
    GuestTextureDesc stencil_desc;GuestTextureLayout stencil_layout;
    if(d.stencil_address){stencil_desc=DescribeGuestStencil(d);stencil_layout=LayoutGuestTexture(stencil_desc);
        if(stencil_bytes.size()<stencil_layout.guest_bytes)throw std::invalid_argument("Short guest stencil upload");}
    auto address_entries=[&](){return address_index_.equal_range(d.address);};
    auto [owned_first,owned_last]=address_entries();
    for(auto indexed=owned_first;indexed!=owned_last;++indexed){auto* existing=indexed->second;
      const bool pending_coherent=!existing->gpu_owned&&!existing->cpu_watched&&existing->pending&&existing->sampled_snapshot.size()==layout.guest_bytes&&
        !memcmp(existing->sampled_snapshot.data(),bytes.data(),size_t(layout.guest_bytes))&&
        (!d.stencil_address||(existing->stencil_snapshot.size()==stencil_layout.guest_bytes&&
         !memcmp(existing->stencil_snapshot.data(),stencil_bytes.data(),size_t(stencil_layout.guest_bytes))));
      if((pending_coherent||existing->gpu_owned||existing->cpu_watched)&&
        (existing->desc==d||CompatibleGuestWritableStorage(existing->desc,d)||
         (existing->desc.writable&&CompatibleGuestShaderStorage(existing->desc,d))||
         (!d.writable&&!d.render_target&&!d.depth_target&&CompatibleGuestSampledStorage(existing->desc,d)))&&
        existing->layout.guest_bytes==layout.guest_bytes){
        bool same_layout=existing->layout.mips.size()==layout.mips.size();
        for(size_t i=0;same_layout&&i<layout.mips.size();++i){const auto& a=existing->layout.mips[i];const auto& b=layout.mips[i];
            same_layout=a.pitch==b.pitch&&a.height==b.height&&a.offset==b.offset&&a.slice_bytes==b.slice_bytes&&a.macro==b.macro;}
        if(!same_layout)continue;
        // Page watchers prove that no CPU access can have changed these bytes.
        // Keep the original native image through subsequent draws/sampling.
        existing->last_use=++use_sequence_;existing->pending=true;
        if(existing->texture.state==D3D12_RESOURCE_STATE_UNORDERED_ACCESS)existing->texture.UavBarrier(list);
        existing->texture.Transition(list,d.writable?D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        return *existing;
    }}
    // A different view (in particular a compute UAV) may overlap an owned
    // render target without qualifying for direct GPU reuse. Resolve ALL
    // overlapping backing on the caller before any comparison or parallel
    // detile can fault on a worker waiting for the caller's execution mutex.
    const auto overlaps=[](uint64_t a,uint64_t as,uint64_t b,uint64_t bs){
        return as&&bs&&(a<=b?b-a<as:a-b<bs);
    };
    for(auto& existing:entries_)if(existing->gpu_owned&&
        (overlaps(d.address,layout.guest_bytes,existing->desc.address,existing->layout.guest_bytes)||
         (d.stencil_address&&overlaps(d.stencil_address,stencil_layout.guest_bytes,existing->desc.address,existing->layout.guest_bytes))||
         (existing->desc.stencil_address&&(overlaps(d.address,layout.guest_bytes,existing->desc.stencil_address,existing->stencil_layout.guest_bytes)||
           (d.stencil_address&&overlaps(d.stencil_address,stencil_layout.guest_bytes,existing->desc.stencil_address,existing->stencil_layout.guest_bytes)))))){
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
    if(!d.writable&&!d.render_target&&!d.depth_target&&!d.stencil_address){
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
                    a.slice_bytes==b.slice_bytes&&a.macro==b.macro;
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
        if(indexed->second->desc==d||CompatibleGuestWritableStorage(indexed->second->desc,d)){entry=indexed->second;break;}}
    const bool immutable_view=!d.writable;
    if(entry&&immutable_view&&
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
    bool new_resource=false;
    if(!entry){
        diagnostic.Stage("resource-allocation");
        auto new_entry=std::make_unique<Entry>();new_entry->desc=d;new_entry->layout=layout;
        if(d.stencil_address)new_entry->stencil_layout=stencil_layout;
        D3D12_RESOURCE_DESC r{};r.Dimension=d.volume_depth?D3D12_RESOURCE_DIMENSION_TEXTURE3D:d.one_dimensional?D3D12_RESOURCE_DIMENSION_TEXTURE1D:D3D12_RESOURCE_DIMENSION_TEXTURE2D;r.Width=d.width;r.Height=d.height;r.DepthOrArraySize=UINT16(d.volume_depth?d.volume_depth:d.layers);r.MipLevels=UINT16(d.levels);r.Format=layout.format;r.SampleDesc.Count=1;
        if(d.depth_target) {
            if(d.writable||d.render_target||d.levels!=1||
               (layout.format!=DXGI_FORMAT_R32_FLOAT&&layout.format!=DXGI_FORMAT_R16_UNORM))
                throw std::invalid_argument("Invalid guest depth resource usage/format");
            r.Format=d.stencil_address?DXGI_FORMAT_R32G8X24_TYPELESS:
                layout.bytes_per_pixel==4?DXGI_FORMAT_R32_TYPELESS:DXGI_FORMAT_R16_TYPELESS;
            r.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        }
        if(d.writable) {
            r.Format=WritableStorageFormat(d.data_format,layout.format);
            r.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }
        if(d.render_target) {
            D3D12_FEATURE_DATA_FORMAT_SUPPORT support{layout.format};
            Check(DX12_API_CALL(device_->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support))));
            if(!(support.Support1&D3D12_FORMAT_SUPPORT1_RENDER_TARGET))throw std::runtime_error("Native guest color format lacks render-target support");
            r.Flags|=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }
        // Account for persistent upload/readback storage as well as texture
        // backing and CPU snapshots; pools must not silently bypass the bound.
        uint64_t transfer_bytes{};
        for(UINT sub=0;sub<d.layers*d.levels*(d.stencil_address?2u:1u);++sub){
            UINT64 bytes{};DX12_API_CALL(device_->GetCopyableFootprints(&r,sub,1,0,nullptr,nullptr,nullptr,&bytes));
            transfer_bytes+=bytes;
        }
        const auto resident=[](const Entry& e){
            uint64_t bytes=e.layout.guest_bytes+e.sampled_snapshot.size()+e.stencil_snapshot.size();
            for(const auto& buffer:e.upload_pool)bytes+=DX12_API_CALL(buffer.handle->GetDesc()).Width;
            if(e.readback_pool)for(const auto& buffer:*e.readback_pool)bytes+=DX12_API_CALL(buffer.handle->GetDesc()).Width;
            return bytes;
        };
        const uint64_t required=layout.guest_bytes*(immutable_view?2:1)+
            (immutable_view&&d.stencil_address?stencil_layout.guest_bytes:0)+
            transfer_bytes*((d.writable||d.render_target||d.depth_target)?2:1);
        RefreshMemoryBudget();
        uint64_t total=required;for(const auto& e:entries_)total+=resident(*e);
        while(total>resident_budget_){
            auto victim=entries_.end();
            for(auto it=entries_.begin();it!=entries_.end();++it)
                if(!(*it)->pending&&(*it)->fence<=completed&&
                   (!(*it)->readback_pool||(*it)->readback_pool.use_count()==1)&&
                   (victim==entries_.end()||(*it)->last_use<(*victim)->last_use))victim=it;
            if(victim==entries_.end())throw std::runtime_error("Native texture batch exceeds adapter-derived cache budget including transfer pools");
            if((*victim)->gpu_owned){if(!resolve_owned_)throw std::runtime_error("GPU-owned image has no retirement resolver");resolve_owned_(**victim);}
            if((*victim)->cpu_watched&&release_immutable_)release_immutable_(**victim);
            total-=resident(**victim);entries_.erase(victim);RebuildIndex();
        }
        new_entry->texture=texture_allocator_.Allocate(r,D3D12_RESOURCE_STATE_COPY_DEST);
        entry=new_entry.get();entries_.push_back(std::move(new_entry));Index(*entry);new_resource=true;}
    entry->last_use=++use_sequence_;entry->sampled_snapshot.clear();
    entry->pending=true;entry->texture.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);auto resource_desc=DX12_API_CALL(entry->texture.handle->GetDesc());
    struct PendingTextureCopy {D3D12_TEXTURE_COPY_LOCATION destination,source;};
    std::vector<PendingTextureCopy> copy_batch;
    size_t upload_index{};
    for(uint32_t plane=0;plane<(d.stencil_address?2u:1u);++plane)
    for(uint32_t layer=0;layer<d.layers;++layer)for(uint32_t mip=0;mip<d.levels;++mip){
        diagnostic.Stage("cpu-detile");
        auto linear=plane?DecodeGuestSubresource(stencil_desc,entry->stencil_layout,stencil_bytes,mip,layer):DecodeGuestSubresource(d,entry->layout,bytes,mip,layer);
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
            if(d.volume_depth&&z)linear=DecodeGuestSubresource(d,layout,bytes,mip,z);
            for(UINT row=0;row<rows;++row)memcpy(static_cast<std::byte*>(mapped)+fp.Offset+(UINT64(z)*rows+row)*fp.Footprint.RowPitch,linear.data()+UINT64(row)*row_bytes,size_t(row_bytes));
        }DX12_API_CALL(upload.handle->Unmap(0,nullptr));
        D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=upload.handle.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=fp;D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=entry->texture.handle.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dst.SubresourceIndex=sub;
        if(new_resource&&submit_copy_)copy_batch.push_back({dst,src});
        else DX12_API_CALL(list->CopyTextureRegion(&dst,0,0,0,&src,nullptr));
        entry->uploads.push_back(std::move(upload));}
    if(!copy_batch.empty())submit_copy_([copies=std::move(copy_batch)](ID3D12GraphicsCommandList* copy_list){
        for(const auto& copy:copies)DX12_API_CALL(copy_list->CopyTextureRegion(
            &copy.destination,0,0,0,&copy.source,nullptr));
    });
    diagnostic.Stage("cpu-coherence-snapshot");
    if(immutable_view)entry->RememberSnapshot(bytes,stencil_bytes);
    if(immutable_view&&!d.render_target&&!d.depth_target&&!d.stencil_address&&watch_immutable_)watch_immutable_(*entry);
    entry->texture.Transition(list,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);entry->pending=true;return *entry;
}
void GuestTextureCache::Submitted(uint64_t fence){if(!fence)throw std::invalid_argument("zero texture fence");for(auto& e:entries_)if(e->pending){e->pending=false;e->fence=fence;}}
void GuestTextureCache::Used(Entry& entry,uint64_t fence){if(!fence||entry.pending||fence<entry.fence)throw std::invalid_argument("invalid texture use fence");entry.fence=fence;}
void GuestTextureCache::Collect(uint64_t completed){
    // Completed fence usually stays constant across thousands of bindings in
    // one PS4 packet. Do the O(cache entries) retirement scan once per advance.
    if(completed<=collected_fence_)return;collected_fence_=completed;
    for(auto& e:entries_)if(!e->pending&&e->fence<=completed)e->uploads.clear();
}
Resource* GuestTextureCache::CoherentDisplayTexture(uint64_t address,UINT width,UINT height,UINT pitch,uint64_t completed){
    auto [first,last]=address_index_.equal_range(address);for(auto indexed=first;indexed!=last;++indexed){auto& e=*indexed->second;
        const auto& d=e.desc;
        if(d.address!=address||d.width!=width||d.height!=height||d.pitch!=pitch||
           d.tile_mode!=10||d.data_format!=10||(d.number_format!=0&&d.number_format!=9)||
           d.writable||d.depth_target||d.stencil_address||d.volume_depth||d.one_dimensional||
           d.levels!=1||d.layers!=1||d.swizzle!=std::array<uint32_t,4>{4,5,6,7}||
           e.pending||e.fence>completed||(!e.gpu_owned&&e.sampled_snapshot.size()!=e.layout.guest_bytes))continue;
        if(!e.gpu_owned&&memcmp(e.sampled_snapshot.data(),reinterpret_cast<const void*>(address),size_t(e.layout.guest_bytes)))continue;
        e.last_use=++use_sequence_;return &e.texture;
    }
    return nullptr;
}
void GuestTextureCache::Invalidate(uint64_t a,uint64_t s,uint64_t completed){if(!s||a>UINT64_MAX-s)throw std::invalid_argument("invalid texture invalidation");
    const auto matches=[&](const Entry& e){return (e.desc.address<a+s&&a<e.desc.address+e.layout.guest_bytes)||
        (e.desc.stencil_address&&e.desc.stencil_address<a+s&&a<e.desc.stencil_address+e.stencil_layout.guest_bytes);};
    for(const auto& e:entries_)if(matches(*e)&&(e->pending||e->fence>completed))throw std::runtime_error("texture invalidation before GPU completion");
    const auto before=entries_.size();std::erase_if(entries_,[&](const auto& e){const bool remove=matches(*e);
        if(remove&&e->cpu_watched&&release_immutable_)release_immutable_(*e);return remove;});
    if(entries_.size()!=before)RebuildIndex();}
}


