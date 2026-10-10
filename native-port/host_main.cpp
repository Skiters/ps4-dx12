#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/backend_factory.h"
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <process.h>
using namespace NativeDX12;
namespace {
std::string JsonString(std::string_view value) {
    std::string out="\"";
    for(unsigned char c:value) {
        if(c=='"'||c=='\\'){out+='\\';out+=char(c);}
        else if(c<32){constexpr char hex[]="0123456789abcdef";out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}
        else out+=char(c);
    }
    return out+'"';
}
std::string Utf8(const wchar_t* value) {
    int size=WideCharToMultiByte(CP_UTF8,0,value,-1,nullptr,0,nullptr,nullptr);
    if(size<=0)throw std::runtime_error("Adapter name conversion failed");
    std::string result(size,'\0');
    WideCharToMultiByte(CP_UTF8,0,value,-1,result.data(),size,nullptr,nullptr);
    result.pop_back();return result;
}
struct Window {
    HWND handle{};
    Window() {
        WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);
        cls.lpfnWndProc=DefWindowProcW;cls.lpszClassName=L"ShadPS4NativeDX12Host";
        if(!RegisterClassW(&cls)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)
            throw std::runtime_error("Cannot register native presentation window");
        handle=CreateWindowW(cls.lpszClassName,L"shadPS4 Native DX12",WS_POPUP,
            0,0,67,39,nullptr,nullptr,cls.hInstance,nullptr);
        if(!handle)throw std::runtime_error("Cannot create native presentation window");
    }
    ~Window(){if(handle)DestroyWindow(handle);}
};
std::filesystem::path CorePath() {
    wchar_t module[32768];DWORD size=GetModuleFileNameW(nullptr,module,DWORD(std::size(module)));
    if(!size||size>=std::size(module))throw std::runtime_error("Cannot resolve native launcher directory");
    auto directory=std::filesystem::path(std::wstring(module,size)).parent_path();
    std::ifstream configured(directory/L"native-core-path.txt",std::ios::binary);
    std::string path;
    if(configured&&std::getline(configured,path)&&!path.empty()) {
        if(path.back()=='\r')path.pop_back();
        auto value=std::filesystem::u8path(path);
        return value.is_absolute()?value:directory/value;
    }
    return directory/L"shadPS4-DX12-Core.exe";
}
// The presentation path can be checked before a game resource provider exists.
// Never manufacture a guest surface or claim that emulation is ready.
struct UnconnectedSurfaces final:GuestSurfaceProvider {
    void Register(const Libraries::VideoOut::BufferAttributeGroup&,VAddr)override {
        throw std::runtime_error("Guest render target provider is not connected");
    }
    GuestSurfaceSnapshot Snapshot(const Libraries::VideoOut::BufferAttributeGroup&,VAddr)override {
        throw std::runtime_error("Guest render target provider is not connected");
    }
};
}
int wmain(int argc,wchar_t**argv) {
    try {
        if(argc==3&&std::wstring_view(argv[1])==L"--game") {
            const auto core=CorePath();
            const auto game=std::filesystem::absolute(argv[2]);
            if(!std::filesystem::is_regular_file(game))throw std::runtime_error("Game eboot.bin was not found");
            if(!std::filesystem::is_regular_file(core))throw std::runtime_error("Native DX12 core is still being built; launch connection is installed");
            // The Windows CRT joins spawn arguments without quoting them.
            // Both validated filesystem paths end in filenames, so neither
            // contains a quote or a trailing backslash requiring extra escape.
            const std::wstring core_argument=L"\""+core.native()+L"\"";
            const std::wstring game_argument=L"\""+game.native()+L"\"";
            const wchar_t* child_args[]{core_argument.c_str(),L"--game",game_argument.c_str(),nullptr};
            const intptr_t result=_wspawnv(_P_WAIT,core.c_str(),child_args);
            if(result==-1)throw std::runtime_error("Cannot start native DX12 core (missing dependency or invalid executable)");
            return static_cast<int>(result);
        }
        if(argc!=2||std::wstring_view(argv[1])!=L"--probe") {
            std::cout<<"{\"protocol\":1,\"backend\":\"dx12\",\"gameLaunchReady\":false,"
                "\"error\":\"Usage: --probe or --game <eboot.bin>. Native game rendering is experimental.\"}\n";
            return 2;
        }
        Window window;UnconnectedSurfaces surfaces;
        auto backend=CreateBackend({"dx12",window.handle,67,39},surfaces);
        DX12_API_CALL(backend->presenter->Present(backend->presenter->PrepareBlankFrame(false)));
        if(GetModuleHandleW(L"vulkan-1.dll"))throw std::runtime_error("Unexpected Vulkan runtime in native host");
        ComPtr<IDXGIFactory4> factory;Check(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))));
        ComPtr<IDXGIAdapter1> adapter;
        Check(DX12_API_CALL(factory->EnumAdapterByLuid(DX12_API_CALL(backend->device->GetAdapterLuid()),IID_PPV_ARGS(&adapter))));
        DXGI_ADAPTER_DESC1 description{};Check(DX12_API_CALL(adapter->GetDesc1(&description)));
        const auto& caps=backend->capabilities;
        std::cout<<"{\"protocol\":1,\"backend\":\"dx12\",\"initialized\":true,"
            "\"presentationVerified\":true,\"vulkanLoaded\":false,\"gameLaunchReady\":false,"
            "\"adapter\":"<<JsonString(Utf8(description.Description))<<",\"shaderModel\":"
            <<unsigned(caps.shader_model)<<",\"barycentrics\":"<<(caps.barycentrics?"true":"false")
            <<",\"waveOperations\":"<<(caps.wave_operations?"true":"false")
            <<",\"experimentalLaunchAvailable\":true,\"coreInstalled\":"
            <<(std::filesystem::is_regular_file(CorePath())?"true":"false")
            <<",\"blocker\":\"native-game-validation\"}\n";
        return 0;
    }catch(const std::exception& error) {
        std::cout<<"{\"protocol\":1,\"backend\":\"dx12\",\"initialized\":false,"
            "\"gameLaunchReady\":false,\"error\":"<<JsonString(error.what())<<"}\n";
        return 1;
    }
}
