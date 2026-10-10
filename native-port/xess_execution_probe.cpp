#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
// An isolated real-SDK execution test, deliberately not a game integration.
// Stationary synthetic color/depth/MV inputs are valid for this test only.
#include "video_core/renderer_d3d12/backend_factory.h"
#include "video_core/renderer_d3d12/texture_resources.h"
#include <xess/xess_d3d12.h>
#include <d3d12sdklayers.h>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace NativeDX12;
namespace {
template<class F> F Symbol(HMODULE library,const char* name){
    auto address=GetProcAddress(library,name);
    if(!address)throw std::runtime_error(std::string("Missing XeSS export: ")+name);
    return reinterpret_cast<F>(address);
}
void XeCheck(xess_result_t result){if(result<0)throw std::runtime_error("XeSS status "+std::to_string(int(result)));}
float Half(unsigned value){
    const unsigned exp=(value>>10)&31,sign=value>>15,mantissa=value&1023;
    if(exp==31)return NAN;
    const float magnitude=exp?std::ldexp(1.f+float(mantissa)/1024,int(exp)-15):std::ldexp(float(mantissa),-24);
    return sign?-magnitude:magnitude;
}
struct Sdk {
    HMODULE library{};xess_context_handle_t context{};
    decltype(&xessDestroyContext) destroy{};
    ~Sdk(){if(context&&destroy)destroy(context);if(library)FreeLibrary(library);}
};
}
int wmain(int argc,wchar_t** argv){try{
    if(argc!=2)throw std::invalid_argument("usage: xess-execution-probe absolute-libxess.dll");
    auto device=CreateHardwareDevice();
    D3D12_COMMAND_QUEUE_DESC queue_desc{};queue_desc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;Check(DX12_API_CALL(device->CreateCommandQueue(&queue_desc,IID_PPV_ARGS(&queue))));
    Sdk sdk;sdk.library=LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!sdk.library)throw std::runtime_error("Cannot load the explicitly supplied XeSS SDK");
    sdk.destroy=Symbol<decltype(&xessDestroyContext)>(sdk.library,"xessDestroyContext");
    XeCheck(Symbol<decltype(&xessD3D12CreateContext)>(sdk.library,"xessD3D12CreateContext")(device.Get(),&sdk.context));
    xess_d3d12_init_params_t init{};init.outputResolution={1920,1200};init.qualitySetting=XESS_QUALITY_SETTING_QUALITY;
    XeCheck(Symbol<decltype(&xessD3D12Init)>(sdk.library,"xessD3D12Init")(sdk.context,&init));
    const auto execute=Symbol<decltype(&xessD3D12Execute)>(sdk.library,"xessD3D12Execute");
    xess_version_t intel{};
    XeCheck(Symbol<decltype(&xessGetIntelXeFXVersion)>(sdk.library,"xessGetIntelXeFXVersion")(sdk.context,&intel));
    auto color=Texture2D(device.Get(),1280,800,DXGI_FORMAT_R16G16B16A16_FLOAT,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST);
    auto velocity=Texture2D(device.Get(),1280,800,DXGI_FORMAT_R16G16_FLOAT,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST);
    auto depth=Texture2D(device.Get(),1280,800,DXGI_FORMAT_R32_FLOAT,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST);
    auto output=Texture2D(device.Get(),1920,1200,DXGI_FORMAT_R16G16B16A16_FLOAT,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    std::vector<uint16_t> pixels(1280*800*4),motion(1280*800*2);
    for(size_t i=0;i<pixels.size();i+=4){pixels[i]=0x3400;pixels[i+1]=0x3800;pixels[i+2]=0x3a00;pixels[i+3]=0x3c00;}
    std::vector<float> z(1280*800,.5f);
    auto upload_color=TextureTransfer::Create(device.Get(),color,false),upload_velocity=TextureTransfer::Create(device.Get(),velocity,false),upload_depth=TextureTransfer::Create(device.Get(),depth,false);
    upload_color.WriteRows(pixels.data(),pixels.size()*2,1280*8);
    upload_velocity.WriteRows(motion.data(),motion.size()*2,1280*4);
    upload_depth.WriteRows(z.data(),z.size()*4,1280*4);
    auto capture=TextureTransfer::Create(device.Get(),output,true);
    ComPtr<ID3D12CommandAllocator> allocator;Check(DX12_API_CALL(device->CreateCommandAllocator(queue_desc.Type,IID_PPV_ARGS(&allocator))));
    ComPtr<ID3D12GraphicsCommandList> commands;Check(DX12_API_CALL(device->CreateCommandList(0,queue_desc.Type,allocator.Get(),nullptr,IID_PPV_ARGS(&commands))));
    upload_color.Upload(commands.Get(),color);upload_velocity.Upload(commands.Get(),velocity);upload_depth.Upload(commands.Get(),depth);
    color.Transition(commands.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    velocity.Transition(commands.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    depth.Transition(commands.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Check(DX12_API_CALL(commands->Close()));Timeline completion(device.Get());completion.Wait(completion.Submit(queue.Get(),commands.Get()));
    for(unsigned frame=0;frame<8;++frame){
        Check(DX12_API_CALL(allocator->Reset()));Check(DX12_API_CALL(commands->Reset(allocator.Get(),nullptr)));
        output.Transition(commands.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        xess_d3d12_execute_params_t params{};
        params.pColorTexture=color.handle.Get();params.pVelocityTexture=velocity.handle.Get();params.pDepthTexture=depth.handle.Get();
        params.pOutputTexture=output.handle.Get();params.exposureScale=1;params.resetHistory=frame==0;
        params.inputWidth=1280;params.inputHeight=800;
        XeCheck(execute(sdk.context,commands.Get(),&params));
        capture.Readback(commands.Get(),output);Check(DX12_API_CALL(commands->Close()));completion.Wait(completion.Submit(queue.Get(),commands.Get()));
    }
    std::vector<uint16_t> result(1920*1200*4);capture.ReadRows(result.data(),result.size()*2,1920*8);
    double sums[3]{};size_t samples{};
    for(unsigned y=100;y<1100;y+=11)for(unsigned x=100;x<1820;x+=13){
        for(unsigned channel=0;channel<3;++channel){float value=Half(result[(y*1920+x)*4+channel]);
            if(!std::isfinite(value))throw std::runtime_error("Non-finite XeSS output");sums[channel]+=value;}
        ++samples;
    }
    const float expected[]{.25f,.5f,.75f};
    for(unsigned channel=0;channel<3;++channel)if(std::abs(sums[channel]/samples-expected[channel])>.08)
        throw std::runtime_error("XeSS output does not reconstruct the synthetic input");
    ComPtr<ID3D12InfoQueue> diagnostics;
    if(SUCCEEDED(device.As(&diagnostics)))for(UINT64 i=0;i<DX12_API_CALL(diagnostics->GetNumStoredMessages());++i){
        SIZE_T bytes{};Check(DX12_API_CALL(diagnostics->GetMessage(i,nullptr,&bytes)));std::vector<std::byte> storage(bytes);
        auto* message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());Check(DX12_API_CALL(diagnostics->GetMessage(i,message,&bytes)));
        if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);
    }
    if(GetModuleHandleW(L"vulkan-1.dll"))throw std::runtime_error("Unexpected Vulkan runtime");
    printf("{\"executionVerified\":true,\"frames\":8,\"input\":[1280,800],\"output\":[1920,1200],\"intelXeFxVersion\":\"%u.%u.%u\",\"gameIntegration\":false,\"xmxExecutionVerified\":false,\"scope\":\"synthetic stationary SR inputs; no frame generation\"}\n",intel.major,intel.minor,intel.patch);
    return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL XeSS execution: %s\n",error.what());return 1;}}
