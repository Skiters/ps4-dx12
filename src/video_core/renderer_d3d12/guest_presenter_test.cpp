#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "backend_factory.h"
#include "texture_resources.h"
#include <cstdio>
using namespace NativeDX12;
struct Provider final:GuestSurfaceProvider {
    ID3D12Device* device{};ID3D12CommandQueue* queue{};unsigned registered{},ready{};
    void Register(const Libraries::VideoOut::BufferAttributeGroup&,VAddr)override{++registered;}
    GuestSurfaceSnapshot Snapshot(const Libraries::VideoOut::BufferAttributeGroup& group,VAddr)override{
        auto texture=std::make_shared<Resource>(Texture2D(device,group.attrib.width,group.attrib.height,DXGI_FORMAT_R8G8B8A8_UNORM,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST));
        std::vector<std::byte> bytes(size_t(group.attrib.width)*group.attrib.height*4,std::byte{0x65});
        auto upload=TextureTransfer::Create(device,*texture,false);upload.WriteRows(bytes.data(),bytes.size(),UINT64(group.attrib.width)*4);
        ComPtr<ID3D12CommandAllocator> alloc;Check(DX12_API_CALL(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&alloc))));
        ComPtr<ID3D12GraphicsCommandList> list;Check(DX12_API_CALL(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,alloc.Get(),nullptr,IID_PPV_ARGS(&list))));
        upload.Upload(list.Get(),*texture);Check(DX12_API_CALL(list->Close()));Timeline timeline(device);timeline.Wait(timeline.Submit(queue,list.Get()));
        return {texture,[this]{++ready;}};
    }
};
LRESULT CALLBACK WindowProc(HWND h,UINT m,WPARAM w,LPARAM l){return DefWindowProcW(h,m,w,l);}
int main(){try{
    WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=WindowProc;wc.lpszClassName=L"NativeGuestVideoOutTest";
    if(!RegisterClassW(&wc))throw std::runtime_error("register window failed");
    HWND window=CreateWindowW(wc.lpszClassName,L"Native VideoOut test",WS_POPUP,0,0,67,39,nullptr,nullptr,wc.hInstance,nullptr);
    if(!window)throw std::runtime_error("create window failed");
    {
        SetEnvironmentVariableW(L"SHADPS4_DX12_INDEPENDENT_VBLANK",nullptr);
        Provider provider;auto backend=CreateBackend({"dx12",window,67,39},provider);
        if(backend->presenter->IndependentVblank()||backend->presenter->UngatedDisplayCadence())
            throw std::runtime_error("Default native presenter changed original guest timing");
        provider.device=backend->device.Get();provider.queue=backend->presentation_queue.Get();
        Libraries::VideoOut::BufferAttributeGroup group{};group.attrib.width=67;group.attrib.height=39;group.attrib.pitch_in_pixel=67;group.attrib.tiling_mode=Libraries::VideoOut::TilingMode::Linear;group.attrib.pixel_format=Libraries::VideoOut::PixelFormat::A8B8G8R8Srgb;
        backend->presenter->RegisterSurface(group,0x10000);
        auto* first=backend->presenter->PrepareFrame(group,0x10000);DX12_API_CALL(backend->presenter->Present(first));
        if(provider.registered!=1||provider.ready!=1||backend->presenter->PrepareLastFrame()!=first)throw std::runtime_error("guest lifecycle mismatch");
        DX12_API_CALL(backend->presenter->Present(first,true));
        DX12_API_CALL(backend->presenter->Present(backend->presenter->PrepareBlankFrame(false)));
        if(provider.ready!=2)throw std::runtime_error("producer completion missing");
        bool guard=false;
        if(backend->presenter->IsHDRSupported()){
            backend->presenter->SetHDR(true);backend->presenter->SetHDR(false);
        }else{
            try{backend->presenter->SetHDR(true);}catch(const std::runtime_error&){guard=true;}
            if(!guard)throw std::runtime_error("Unsupported HDR silently accepted");
        }
        guard=false;try{CreateBackend({"vulkan",window,67,39},provider);}catch(const std::invalid_argument&){guard=true;}
        if(!guard||GetModuleHandleW(L"vulkan-1.dll"))throw std::runtime_error("backend selection/Vulkan guard failed");
    }
    {
        SetEnvironmentVariableW(L"SHADPS4_DX12_INDEPENDENT_VBLANK",L"1");
        Provider provider;auto backend=CreateBackend({"dx12",window,67,39},provider);
        SetEnvironmentVariableW(L"SHADPS4_DX12_INDEPENDENT_VBLANK",nullptr);
        if(!backend->presenter->IndependentVblank()||!backend->presenter->UngatedDisplayCadence())
            throw std::runtime_error("Explicit native timing extension was not retained for its lifetime");
    }
    DestroyWindow(window);puts("PASS native DX12 backend startup, guest VideoOut register/snapshot/producer-ready/present/GPU-drain/repeat/blank lifecycle and default/explicit timing; Vulkan not loaded. Synthetic provider, not game texture cache.");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
