#include "common/api_trace.h"
#include "texture_resources.h"
#include <dxgi1_6.h>
#include <cstdio>
using namespace NativeDX12;
void Require(bool ok){if(!ok)throw std::runtime_error("texture assertion failed");}
int main(){try{
    ComPtr<IDXGIFactory6> factory;Check(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))));ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> a;if(DX12_API_CALL(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&a)))==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 d{};DX12_API_CALL(a->GetDesc1(&d));if(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;if(SUCCEEDED(DX12_API_CALL(D3D12CreateDevice(a.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))){wprintf(L"Hardware: %ls\n",d.Description);break;}}
    Require(device!=nullptr);ComPtr<ID3D12CommandQueue> queue;D3D12_COMMAND_QUEUE_DESC q{};Check(DX12_API_CALL(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))));
    ComPtr<ID3D12CommandAllocator> allocator;Check(DX12_API_CALL(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))));
    ComPtr<ID3D12GraphicsCommandList> list;Check(DX12_API_CALL(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list))));
    // Odd width guarantees source pitch differs from the D3D12 aligned row pitch.
    constexpr UINT width=67,height=39,pitch=width*4+12;
    std::vector<unsigned char> input(pitch*height,0xcd),output(pitch*height,0xef);
    for(UINT y=0;y<height;++y)for(UINT x=0;x<width*4;++x)input[y*pitch+x]=static_cast<unsigned char>((x*7+y*13)&255);
    auto texture=Texture2D(device.Get(),width,height,DXGI_FORMAT_R8G8B8A8_UNORM,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_DEST);
    auto upload=TextureTransfer::Create(device.Get(),texture,false),readback=TextureTransfer::Create(device.Get(),texture,true);
    Require(upload.footprint.Footprint.RowPitch%256==0);Require(upload.footprint.Footprint.RowPitch!=pitch);
    upload.WriteRows(input.data(),input.size(),pitch);upload.Upload(list.Get(),texture);
    texture.Transition(list.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);readback.Readback(list.Get(),texture);
    Descriptors srvs(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,1,true),rtvs(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_RTV,1,false),dsvs(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_DSV,1,false);
    auto s=srvs.Allocate(0),r=rtvs.Allocate(0),d=dsvs.Allocate(0);
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R8G8B8A8_UNORM;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;
    DX12_API_CALL(device->CreateShaderResourceView(texture.handle.Get(),&srv,srvs.Cpu(s)));DX12_API_CALL(device->CreateRenderTargetView(texture.handle.Get(),nullptr,rtvs.Cpu(r)));
    auto depth=Depth32(device.Get(),width,height);DX12_API_CALL(device->CreateDepthStencilView(depth.handle.Get(),nullptr,dsvs.Cpu(d)));
    DX12_API_CALL(list->ClearDepthStencilView(dsvs.Cpu(d),D3D12_CLEAR_FLAG_DEPTH,1.f,0,0,nullptr));
    Check(DX12_API_CALL(list->Close()));Timeline timeline(device.Get());auto token=timeline.Submit(queue.Get(),list.Get());
    srvs.Retire(s,token);rtvs.Retire(r,token);dsvs.Retire(d,token);timeline.Wait(token);
    readback.ReadRows(output.data(),output.size(),pitch);
    for(UINT y=0;y<height;++y){Require(memcmp(input.data()+y*pitch,output.data()+y*pitch,width*4)==0);for(UINT x=width*4;x<pitch;++x)Require(output[y*pitch+x]==0xef);}
    bool rejected=false;try{upload.WriteRows(input.data(),1,pitch);}catch(const std::invalid_argument&){rejected=true;}Require(rejected);
    puts("PASS: native RGBA8 texture upload/readback with padded odd-width rows; SRV/RTV; D32 DSV and GPU clear; staging lifetime; short input rejected");return 0;
}catch(const std::exception&e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
