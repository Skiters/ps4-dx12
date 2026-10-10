#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "presenter.h"
#include "texture_resources.h"
#include <cstdio>
#include <vector>
#include <algorithm>
#include <cmath>
#include <d3d12sdklayers.h>
LRESULT CALLBACK TestWindowProc(HWND h,UINT m,WPARAM w,LPARAM l){return DefWindowProcW(h,m,w,l);}
static void CALLBACK DebugMessage(D3D12_MESSAGE_CATEGORY,D3D12_MESSAGE_SEVERITY severity,D3D12_MESSAGE_ID id,LPCSTR text,void*){
    if(severity<=D3D12_MESSAGE_SEVERITY_WARNING){std::fprintf(stderr,"D3D12 validation %u/%u: %s\n",unsigned(severity),unsigned(id),text);std::fflush(stderr);}
}
int main(){try {
    using namespace NativeDX12;
    WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=TestWindowProc;wc.lpszClassName=L"NativeDx12PresentTest";
    if(!RegisterClassW(&wc))throw std::runtime_error("Window class failed");
    HWND window=CreateWindowW(wc.lpszClassName,L"Native DX12 presentation self-test",WS_POPUP,0,0,67,39,nullptr,nullptr,wc.hInstance,nullptr);
    if(!window)throw std::runtime_error("Window failed");
    {
        ComPtr<ID3D12Debug> debug;
        if(SUCCEEDED(DX12_API_CALL(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))))DX12_API_CALL(debug->EnableDebugLayer());
        ComPtr<IDXGIFactory6> factory;Check(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))));
        ComPtr<ID3D12Device> device;
        for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> adapter;
            auto hr=DX12_API_CALL(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter)));
            if(hr==DXGI_ERROR_NOT_FOUND)break;Check(hr);
            DXGI_ADAPTER_DESC1 info{};Check(DX12_API_CALL(adapter->GetDesc1(&info)));
            if(info.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
            if(SUCCEEDED(DX12_API_CALL(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)))))break;
        }
        if(!device)throw std::runtime_error("No hardware D3D12");
        ComPtr<ID3D12InfoQueue1> debug_messages;DWORD debug_cookie{};
        if(SUCCEEDED(DX12_API_CALL(device.As(&debug_messages))))Check(DX12_API_CALL(debug_messages->RegisterMessageCallback(DebugMessage,D3D12_MESSAGE_CALLBACK_FLAG_NONE,nullptr,&debug_cookie)));
        D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
        ComPtr<ID3D12CommandQueue> queue;Check(DX12_API_CALL(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))));
        auto texture=Texture2D(device.Get(),67,39,DXGI_FORMAT_R8G8B8A8_UNORM,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST);
        std::vector<unsigned char> pattern(67*39*4);
        for(size_t i=0;i<pattern.size();i++)pattern[i]=static_cast<unsigned char>((i*13)^37);
        auto upload=TextureTransfer::Create(device.Get(),texture,false);upload.WriteRows(pattern.data(),pattern.size(),67*4);
        ComPtr<ID3D12CommandAllocator> allocator;Check(DX12_API_CALL(device->CreateCommandAllocator(q.Type,IID_PPV_ARGS(&allocator))));
        ComPtr<ID3D12GraphicsCommandList> commands;Check(DX12_API_CALL(device->CreateCommandList(0,q.Type,allocator.Get(),nullptr,IID_PPV_ARGS(&commands))));
        upload.Upload(commands.Get(),texture);Check(DX12_API_CALL(commands->Close()));Timeline ready(device.Get());ready.Wait(ready.Submit(queue.Get(),commands.Get()));
        Presenter presenter(device.Get(),queue.Get(),window,67,39);
        auto capture=TextureTransfer::Create(device.Get(),texture,true);
        for(unsigned i=0;i<36;i++)presenter.Present(texture,i==35?&capture:nullptr);
        presenter.Drain();std::vector<unsigned char> result(pattern.size());capture.ReadRows(result.data(),result.size(),67*4);
        if(result!=pattern)throw std::runtime_error("Swapchain image differs from source texture");
        auto low=Texture2D(device.Get(),3,2,DXGI_FORMAT_R8G8B8A8_UNORM,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST);
        const unsigned char low_pixels[]{0,0,77,255,127,0,77,255,255,0,77,255,
            0,255,77,255,127,255,77,255,255,255,77,255};
        auto low_upload=TextureTransfer::Create(device.Get(),low,false);
        low_upload.WriteRows(low_pixels,sizeof(low_pixels),12);
        Check(DX12_API_CALL(allocator->Reset()));Check(DX12_API_CALL(commands->Reset(allocator.Get(),nullptr)));
        low_upload.Upload(commands.Get(),low);Check(DX12_API_CALL(commands->Close()));ready.Wait(ready.Submit(queue.Get(),commands.Get()));
        // Check the spatial upscaler against an independent CPU version of
        // its bilinear taps and contrast-adaptive sharpening equation.
        for(unsigned pass=0;pass<5;++pass)presenter.Present(low,&capture);
        presenter.Drain();capture.ReadRows(result.data(),result.size(),67*4);
        auto sample=[&](float u,float v,unsigned channel){
            const float sx=std::clamp(u*3-0.5f,0.f,2.f),sy=std::clamp(v*2-0.5f,0.f,1.f);
            const unsigned x0=unsigned(sx),x1=std::min(x0+1,2u),y0=unsigned(sy),y1=std::min(y0+1,1u);
            const float fx=sx-x0,fy=sy-y0;
            const auto at=[&](unsigned x,unsigned y){return float(low_pixels[(y*3+x)*4+channel])/255.f;};
            return std::lerp(std::lerp(at(x0,y0),at(x1,y0),fx),std::lerp(at(x0,y1),at(x1,y1),fx),fy);
        };
        for(unsigned y=0;y<39;++y)for(unsigned x=0;x<67;++x){
            const float u=(x+0.5f)/67,v=(y+0.5f)/39;
            for(unsigned c=0;c<4;++c){
                const float center=sample(u,v,c);
                float expected=center;
                if(c<3){
                    const float n=sample(u,v-0.5f,c),s=sample(u,v+0.5f,c);
                    const float w=sample(u-1.f/3,v,c),e=sample(u+1.f/3,v,c);
                    const float mn=std::min({center,n,s,w,e}),mx=std::max({center,n,s,w,e});
                    const float amplitude=std::clamp(std::min(mn,1.f-mx)/std::max(mx,1e-4f),0.f,1.f);
                    const float weight=(1.f/std::sqrt(std::max(amplitude,1e-4f)))*(-1.f/(8.f-3.f*0.6f));
                    expected=std::clamp((center+weight*(n+s+w+e))/(1.f+4.f*weight),0.f,1.f);
                }
                if(std::abs(float(result[(y*67+x)*4+c])-expected*255.f)>3)
                    throw std::runtime_error("Native GPU spatial presentation pixel mismatch");
            }
        }
        const auto scaled_pixels=result;
        presenter.Present(texture,&capture);presenter.Drain();capture.ReadRows(result.data(),result.size(),67*4);
        if(result!=pattern)throw std::runtime_error("Same-size presentation changed after scaled frames");
        presenter.SetGamma(2.f);presenter.Present(texture,&capture);presenter.Drain();
        capture.ReadRows(result.data(),result.size(),67*4);
        for(size_t i=0;i<result.size();i+=4)for(unsigned c=0;c<3;++c){
            const auto expected=std::sqrt(float(pattern[i+c])/255.f)*255.f;
            if(std::abs(float(result[i+c])-expected)>2)throw std::runtime_error("Native gamma presentation mismatch");
        }
        presenter.SetGamma(1.f);
        // Exercise the native UI insertion point: draw into the backbuffer while
        // preserving the guest image outside the overlay rectangle.
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;heapDesc.NumDescriptors=1;
        ComPtr<ID3D12DescriptorHeap> rtv;
        Check(DX12_API_CALL(device->CreateDescriptorHeap(&heapDesc,IID_PPV_ARGS(&rtv))));
        unsigned overlayCalls=0;
        presenter.SetOverlay([&](ID3D12GraphicsCommandList* list,ID3D12Resource* target){
            auto handle=DX12_API_CALL(rtv->GetCPUDescriptorHandleForHeapStart());
            DX12_API_CALL(device->CreateRenderTargetView(target,nullptr,handle));
            const float color[4]{1,0,1,1};const D3D12_RECT rectangle{5,7,19,21};
            DX12_API_CALL(list->ClearRenderTargetView(handle,color,1,&rectangle));++overlayCalls;
        });
        presenter.Present(texture,&capture);presenter.Drain();
        capture.ReadRows(result.data(),result.size(),67*4);
        auto expected=pattern;
        for(unsigned y=7;y<21;++y)for(unsigned x=5;x<19;++x){
            auto offset=(y*67+x)*4;
            expected[offset]=255;expected[offset+1]=0;expected[offset+2]=255;expected[offset+3]=255;
        }
        if(overlayCalls!=1||result!=expected)throw std::runtime_error("Native overlay damaged the guest image or failed to draw");
        presenter.Present(low,&capture);presenter.Drain();capture.ReadRows(result.data(),result.size(),67*4);
        expected=scaled_pixels;
        for(unsigned y=7;y<21;++y)for(unsigned x=5;x<19;++x){
            auto offset=(y*67+x)*4;
            expected[offset]=255;expected[offset+1]=0;expected[offset+2]=255;expected[offset+3]=255;
        }
        if(overlayCalls!=2||result!=expected)throw std::runtime_error("Scaled native overlay damaged the guest image or failed to draw");
        presenter.SetOverlay({});presenter.Present(texture,&capture);presenter.Drain();
        capture.ReadRows(result.data(),result.size(),67*4);
        if(result!=pattern)throw std::runtime_error("Guest image did not recover after removing overlay");
        Resource invalid;bool rejected=false;try{presenter.Present(invalid);}catch(const std::invalid_argument&){rejected=true;}
        if(!rejected||GetModuleHandleW(L"vulkan-1.dll"))throw std::runtime_error("Guard/Vulkan test failed");
        ComPtr<ID3D12InfoQueue> diagnostics;
        if(SUCCEEDED(DX12_API_CALL(device.As(&diagnostics))))for(UINT64 i=0;i<DX12_API_CALL(diagnostics->GetNumStoredMessages());++i){
            SIZE_T bytes{};Check(DX12_API_CALL(diagnostics->GetMessage(i,nullptr,&bytes)));std::vector<std::byte> storage(bytes);
            auto* message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());Check(DX12_API_CALL(diagnostics->GetMessage(i,message,&bytes)));
            if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);
        }
    }
    DestroyWindow(window);
    puts("PASS native D3D12: uploaded texture -> DXGI backbuffer -> exact readback; 38 submissions, overlay composition/release verified; no Vulkan/FG. Hidden window does not measure visible pacing.");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
