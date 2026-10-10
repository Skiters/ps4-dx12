#include "common/api_trace.h"
#include "guest_texture_cache.h"
#include "video_core/amdgpu/resource.h"
#include <dxgi1_6.h>
#include <cstdio>
using namespace NativeDX12;
void Require(bool b){if(!b)throw std::runtime_error("guest texture assertion failed");}
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}Require(rejected);}
// Independent golden PS4 Thin microtile permutation, indexed by row-major texel.
constexpr uint8_t golden_morton[64]={0,1,4,5,16,17,20,21,2,3,6,7,18,19,22,23,8,9,12,13,24,25,28,29,10,11,14,15,26,27,30,31,32,33,36,37,48,49,52,53,34,35,38,39,50,51,54,55,40,41,44,45,56,57,60,61,42,43,46,47,58,59,62,63};
std::byte Value(UINT m,UINT l,UINT x,UINT y,UINT c){return std::byte((m*37+l*71+y*11+x*3+c)&255);}
int main(int argc,char**){try{
    const bool one_dimensional=argc>1;
    // R8 macro tiles hold only 64 bytes of texels inside a 256-byte chunk.
    // A floor division made the copy-plan empty and crashed the world loader.
    for(UINT format:{1u,2u})for(bool screen_size:{false,true}){
        GuestTextureDesc tiny_desc;tiny_desc.width=tiny_desc.height=tiny_desc.pitch=16;
        tiny_desc.data_format=format;tiny_desc.tile_mode=14;tiny_desc.layers=2;tiny_desc.levels=2;
        // Exercise the final macro/microtile at the exact low-resolution
        // world-loader dimensions, with no later layer hiding an overrun.
        if(screen_size){tiny_desc.width=tiny_desc.pitch=640;tiny_desc.height=360;
            tiny_desc.layers=tiny_desc.levels=1;}
        const auto storage=LayoutGuestTexture(tiny_desc);
        std::vector<std::byte> tiled(storage.guest_bytes,std::byte{0xee});
        for(UINT mip=0;mip<tiny_desc.levels;++mip)for(UINT layer=0;layer<tiny_desc.layers;++layer){
            const auto w=tiny_desc.width>>mip,h=tiny_desc.height>>mip;
            for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x)for(UINT c=0;c<storage.bytes_per_pixel;++c)
                tiled[GuestPixelOffset(tiny_desc,storage,mip,layer,x,y)+c]=Value(mip,layer,x,y,c);
            const auto linear=DecodeGuestSubresource(tiny_desc,storage,tiled,mip,layer);
            for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x)for(UINT c=0;c<storage.bytes_per_pixel;++c)
                Require(linear[(y*w+x)*storage.bytes_per_pixel+c]==Value(mip,layer,x,y,c));
        }
    }
    AmdGpu::Image image{};image.base_address=0x1000;image.width=16;image.height=8;image.pitch=23;image.last_level=2;image.type=uint64_t(AmdGpu::ImageType::Color2DArray);image.depth=1;image.last_array=1;image.data_format=10;image.num_format=0;image.tiling_index=13;image.dst_sel_x=4;image.dst_sel_y=5;image.dst_sel_z=6;image.dst_sel_w=7;
    if(one_dimensional){image.type=uint64_t(AmdGpu::ImageType::Color1DArray);image.height=0;}
    auto desc=DescribeGuestTexture(image);auto layout=LayoutGuestTexture(desc);
    Require(layout.mips[0].pitch==24&&layout.mips[0].height==(one_dimensional?8:16)&&layout.mips[0].slice_bytes==(one_dimensional?768:1536));
    Require(layout.mips[1].offset==(one_dimensional?1536:3072)&&layout.mips[1].slice_bytes==512&&layout.mips[2].offset==(one_dimensional?2560:4096)&&layout.guest_bytes==(one_dimensional?3072:4608));
    std::vector<std::byte> fixture(layout.guest_bytes,std::byte{0xee});
    for(UINT m=0;m<desc.levels;++m)for(UINT l=0;l<desc.layers;++l)for(UINT y=0;y<std::max(desc.height>>m,1u);++y)for(UINT x=0;x<std::max(desc.width>>m,1u);++x){const auto& ml=layout.mips[m];auto tile=(y/8)*(ml.pitch/8)+x/8;auto offset=ml.offset+l*ml.slice_bytes+(tile*64+golden_morton[(y%8)*8+x%8])*4;for(UINT c=0;c<4;++c)fixture[offset+c]=Value(m,l,x,y,c);}
    for(UINT m=0;m<desc.levels;++m)for(UINT l=0;l<desc.layers;++l){auto linear=DecodeGuestSubresource(desc,layout,fixture,m,l);auto w=std::max(desc.width>>m,1u),h=std::max(desc.height>>m,1u);for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x)for(UINT c=0;c<4;++c)Require(linear[(y*w+x)*4+c]==Value(m,l,x,y,c));}
    auto invalid=desc;invalid.tile_mode=17;Reject([&]{LayoutGuestTexture(invalid);});invalid=desc;invalid.data_format=15;Reject([&]{LayoutGuestTexture(invalid);});Reject([&]{DecodeGuestSubresource(desc,layout,std::span(fixture).first(1),0,0);});
    // Display32 golden addresses differ from Thin Morton in bit2 and bit3.
    auto display=desc;display.tile_mode=9;auto dl=LayoutGuestTexture(display);Require(GuestPixelOffset(display,dl,0,0,2,0)==8);if(!one_dimensional)Require(GuestPixelOffset(display,dl,0,0,0,1)==16);Require(GuestPixelOffset(display,dl,0,0,4,0)==32);
    // DISPLAY_LINEAR_GENERAL has no 256-byte padding. Preserve the declared
    // pitch while copying only logical texels in both directions.
    GuestTextureDesc general;general.width=5;general.height=3;general.pitch=7;
    general.data_format=10;general.tile_mode=31;auto gl=LayoutGuestTexture(general);
    Require(gl.mips[0].pitch==7&&gl.mips[0].height==3&&gl.guest_bytes==84);
    std::vector<std::byte> general_guest(gl.guest_bytes,std::byte{0xa5});
    std::vector<std::byte> general_linear(5*3*4);
    for(size_t i=0;i<general_linear.size();++i)general_linear[i]=std::byte(i);
    EncodeGuestSubresource(general,gl,general_guest,general_linear,0,0);
    Require(DecodeGuestSubresource(general,gl,general_guest,0,0)==general_linear);
    for(UINT y=0;y<3;++y)for(UINT x=5;x<7;++x)for(UINT c=0;c<4;++c)
        Require(general_guest[(y*7+x)*4+c]==std::byte{0xa5});
    ComPtr<IDXGIFactory6> factory;Check(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))));ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1>a;if(DX12_API_CALL(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&a)))==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 info{};DX12_API_CALL(a->GetDesc1(&info));if(info.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;if(SUCCEEDED(DX12_API_CALL(D3D12CreateDevice(a.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))){wprintf(L"Hardware: %ls\n",info.Description);break;}}Require(device!=nullptr);
    ComPtr<ID3D12CommandQueue> queue;D3D12_COMMAND_QUEUE_DESC q{};Check(DX12_API_CALL(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))));ComPtr<ID3D12CommandAllocator> allocator;Check(DX12_API_CALL(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))));ComPtr<ID3D12GraphicsCommandList> list;Check(DX12_API_CALL(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list))));
    GuestTextureCache cache(device.Get());auto& entry=cache.Upload(desc,fixture,list.Get(),0);auto srv=entry.Srv();
    auto clamped=desc;clamped.min_lod=384;clamped.first_mip=1;
    Require(&cache.Upload(clamped,fixture,list.Get(),0)==&entry);
    const auto clamped_srv=entry.Srv(clamped);
    Require((one_dimensional?clamped_srv.Texture1DArray.ResourceMinLODClamp:
        clamped_srv.Texture2DArray.ResourceMinLODClamp)==1.5f);
    // The minimum is absolute in resource space; the view base is independent.
    for(u32 dimension=0;dimension<5;++dimension){
        GuestTextureCache::Entry descriptor;
        descriptor.desc=desc;descriptor.desc.min_lod=384;descriptor.desc.first_mip=1;
        descriptor.desc.one_dimensional=dimension<2;
        descriptor.desc.layers=dimension==1||dimension==3?2:1;
        descriptor.desc.volume_depth=dimension==4?8:0;
        descriptor.layout=layout;
        const auto view=descriptor.Srv();
        const float minimum=dimension==0?view.Texture1D.ResourceMinLODClamp:
            dimension==1?view.Texture1DArray.ResourceMinLODClamp:
            dimension==2?view.Texture2D.ResourceMinLODClamp:
            dimension==3?view.Texture2DArray.ResourceMinLODClamp:view.Texture3D.ResourceMinLODClamp;
        Require(minimum==1.5f);
        descriptor.desc.min_lod=4095;
        const auto bounded=descriptor.Srv();
        const float maximum=dimension==0?bounded.Texture1D.ResourceMinLODClamp:
            dimension==1?bounded.Texture1DArray.ResourceMinLODClamp:
            dimension==2?bounded.Texture2D.ResourceMinLODClamp:
            dimension==3?bounded.Texture2DArray.ResourceMinLODClamp:bounded.Texture3D.ResourceMinLODClamp;
        Require(maximum==2.f);
    }
    // A read-only depth attachment sampled by the same command list must use
    // one typeless native resource with DSV and SRV views, without retirement.
    GuestTextureDesc depth;depth.address=0x800000;depth.width=depth.height=depth.pitch=64;
    depth.data_format=4;depth.number_format=7;depth.tile_mode=0;depth.depth_target=true;
    auto depth_layout=LayoutGuestTexture(depth);std::vector<std::byte> depth_bytes(depth_layout.guest_bytes);
    auto& depth_entry=cache.Upload(depth,depth_bytes,list.Get(),0);
    auto sampled_depth=depth;sampled_depth.depth_target=false;
    auto& depth_alias=cache.Upload(sampled_depth,depth_bytes,list.Get(),0);
    Require(&depth_alias==&depth_entry&&depth_alias.Srv(sampled_depth).Format==DXGI_FORMAT_R32_FLOAT);
    const auto read_only_dsv=depth_entry.Dsv(true,true);
    Require((read_only_dsv.Flags&D3D12_DSV_FLAG_READ_ONLY_DEPTH)!=0);
    // Multiple draws may sample the same unchanged texture before submission.
    Require(&cache.Upload(desc,fixture,list.Get(),0)==&entry);
    auto changed_fixture=fixture;changed_fixture[0]^=std::byte{1};
    // Changed staging must still be rejected without an owner retirement hook.
    Reject([&]{cache.Upload(desc,changed_fixture,list.Get(),0);});
    Require(srv.ViewDimension==(one_dimensional?D3D12_SRV_DIMENSION_TEXTURE1DARRAY:D3D12_SRV_DIMENSION_TEXTURE2DARRAY)&&srv.Texture2DArray.ArraySize==2&&srv.Texture2DArray.MipLevels==3&&srv.Shader4ComponentMapping==D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING);
    Descriptors views(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,1,true);auto slot=views.Allocate(0);DX12_API_CALL(device->CreateShaderResourceView(entry.texture.handle.Get(),&srv,views.Cpu(slot)));
    struct Read {Resource buffer;D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp;UINT m,l;};std::vector<Read> reads;entry.texture.Transition(list.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE);auto rd=DX12_API_CALL(entry.texture.handle->GetDesc());
    for(UINT l=0;l<desc.layers;++l)for(UINT m=0;m<desc.levels;++m){UINT64 total;D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT sub=m+l*desc.levels;DX12_API_CALL(device->GetCopyableFootprints(&rd,sub,1,0,&fp,nullptr,nullptr,&total));auto rb=Buffer(device.Get(),total,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=entry.texture.handle.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;src.SubresourceIndex=sub;D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=rb.handle.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=fp;DX12_API_CALL(list->CopyTextureRegion(&dst,0,0,0,&src,nullptr));reads.push_back({std::move(rb),fp,m,l});}
    Reject([&]{cache.Invalidate(desc.address,layout.guest_bytes,0);});Check(DX12_API_CALL(list->Close()));Timeline timeline(device.Get());auto token=timeline.Submit(queue.Get(),list.Get());cache.Submitted(token);views.Retire(slot,token);timeline.Wait(token);
    for(auto& r:reads){void* data=nullptr;Check(DX12_API_CALL(r.buffer.handle->Map(0,nullptr,&data)));auto w=std::max(desc.width>>r.m,1u),h=std::max(desc.height>>r.m,1u);auto bytes=static_cast<std::byte*>(data)+r.fp.Offset;for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x)for(UINT c=0;c<4;++c)Require(bytes[y*r.fp.Footprint.RowPitch+x*4+c]==Value(r.m,r.l,x,y,c));D3D12_RANGE empty{0,0};DX12_API_CALL(r.buffer.handle->Unmap(0,&empty));}
    cache.Collect(timeline.Completed());Require(entry.uploads.empty());cache.Invalidate(desc.address,layout.guest_bytes,timeline.Completed());
    puts("PASS: actual AmdGpu image metadata, PS4 golden thin1D tiles -> native 3mip 2layer RGBA texture -> all-subresource exact GPU readback; display32 golden offsets; unsupported/short/in-flight rejection");return 0;
}catch(const std::exception&e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}

