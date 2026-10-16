// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_compute.h"
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "shader_recompiler/backend/dxil/spirv_converter.h"
#include <dxgi1_6.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string_view>
using namespace NativeDX12;
int wmain(int argc,wchar_t** argv){try {
    if(argc!=4&&argc!=5)throw std::invalid_argument("usage: buffer-reader-image-writer fixture.spv converter.dll dxc.dll [partial|reject-both-writers]");
    const std::wstring_view mode=argc==5?argv[4]:L"full";
    if(mode!=L"full"&&mode!=L"partial"&&mode!=L"reject-both-writers")throw std::invalid_argument("Unknown buffer/image fixture mode");
    std::ifstream file(argv[1],std::ios::binary|std::ios::ate);if(!file)throw std::runtime_error("Cannot read fixture");
    const auto bytes=file.tellg();if(bytes<=0||bytes%4)throw std::runtime_error("Invalid SPIR-V fixture");
    std::vector<u32> spirv(size_t(bytes)/4);file.seekg(0);file.read(reinterpret_cast<char*>(spirv.data()),bytes);
    Shader::Backend::DX12::SpirvConvertOptions options;options.stage=Shader::Backend::DX12::SpirvStage::Compute;
    options.native_storage_buffer_uavs=true;
    auto dxil=Shader::Backend::DX12::ConvertSpirv(argv[2],spirv,options);if(!dxil.success)throw std::runtime_error(dxil.diagnostics);
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> adapter;const auto result=factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter));
        if(result==DXGI_ERROR_NOT_FOUND)break;Check(result);DXGI_ADAPTER_DESC1 desc{};Check(adapter->GetDesc1(&desc));
        if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;if(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))break;}
    if(!device)throw std::runtime_error("No hardware device");
    auto* memory=static_cast<std::byte*>(VirtualAlloc(reinterpret_cast<void*>(0x1000000000ULL),0x30000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!memory)throw std::runtime_error("Cannot allocate 40-bit guest memory");
    struct Allocation{void* data;~Allocation(){VirtualFree(data,0,MEM_RELEASE);}} allocation{memory};
    memset(memory,0xee,0x30000);Rasterizer rasterizer(device.Get());rasterizer.MapMemory(reinterpret_cast<VAddr>(memory),0x30000);
    GuestTextureCache textures(device.Get());ShaderPipeline pipeline(device.Get(),argv[3],dxil.bytecode);
    Shader::Info info;info.sw_stage=Shader::SwStage::Compute;info.flattened_ud_buf.resize(16);
    const u32 input_offset=mode==L"partial"?128:0;
    for(UINT i=0;i<2;++i){Shader::BufferResource resource{};resource.buffer_type=Shader::BufferType::Guest;resource.is_written=i==1;
        resource.sharp_fetch.summary=Shader::SharpFetch<AmdGpu::Buffer>::Summary::SingleLoad;resource.sharp_fetch.offsets[0]=i*4;info.buffers.push_back(resource);
        AmdGpu::Buffer sharp{};sharp.base_address=reinterpret_cast<VAddr>(memory+(i?0x10000:input_offset));sharp.num_records=256;
        memcpy(info.flattened_ud_buf.data()+i*4,&sharp,sizeof(sharp));}
    AmdGpu::Image sharp{};sharp.base_address=reinterpret_cast<VAddr>(memory)>>8;sharp.width=7;sharp.height=7;sharp.pitch=7;
    sharp.type=u64(AmdGpu::ImageType::Color2D);sharp.data_format=4;sharp.num_format=4;sharp.tiling_index=13;
    sharp.dst_sel_x=4;sharp.dst_sel_y=5;sharp.dst_sel_z=6;sharp.dst_sel_w=7;
    Shader::ImageResource image{};image.is_written=true;image.sharp_fetch.summary=Shader::SharpFetch<AmdGpu::Image>::Summary::SingleLoad;
    image.sharp_fetch.offsets[0]=8;info.images.push_back(image);memcpy(info.flattened_ud_buf.data()+8,&sharp,sizeof(sharp));
    const auto desc=DescribeGuestTexture(sharp);const auto layout=LayoutGuestTexture(desc);
    if(layout.guest_bytes>0x10000)throw std::runtime_error("Fixture image unexpectedly overlaps separate output");
    for(UINT index=0;index<64;++index){const u32 value=0x40000000+index*17;memcpy(memory+input_offset+index*4,&value,4);}
    for(UINT repeat=0;repeat<3;++repeat){
        std::vector<std::byte> before(memory,memory+0x30000),expected=before;
        if(mode==L"reject-both-writers"){
            info.buffers.front().is_written=true;bool rejected=false;
            try{ExecuteGuestBufferCompute(device.Get(),rasterizer,pipeline,info,{},ComputeInvocation{},&textures);}
            catch(const std::runtime_error& error){if(std::string_view(error.what()).find("writable buffer/image alias")==std::string_view::npos)throw;rejected=true;}
            if(!rejected||memcmp(before.data(),memory,before.size()))throw std::runtime_error("Writable buffer/image ownership collision was not rejected before execution");
            break;
        }
        for(UINT y=0;y<8;++y)for(UINT x=0;x<8;++x){const auto index=y*8+x;u32 value{};
            memcpy(&value,before.data()+input_offset+index*4,4);value^=0x1f000000+index*3;
            memcpy(expected.data()+0x10000+index*4,&value,4);memcpy(expected.data()+GuestPixelOffset(desc,layout,0,0,x,y),&value,4);}
        ExecuteGuestBufferCompute(device.Get(),rasterizer,pipeline,info,{},ComputeInvocation{},&textures);
        if(memcmp(expected.data(),memory,expected.size()))throw std::runtime_error("Buffer reader/image writer changed prewrite inputs, outputs or padding");
        if(repeat==1){const u32 value=0xe0001234;memcpy(memory+input_offset+11*4,&value,4);}
    }
    if(GetModuleHandleW(L"vulkan-1.dll"))throw std::runtime_error("Vulkan runtime unexpectedly loaded");
    puts("PASS: buffer reader retains pre-dispatch aliased image contents; full/partial tiled writes, warm cache, CPU edit, exact allocation/padding; two writers rejected before execution");return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
