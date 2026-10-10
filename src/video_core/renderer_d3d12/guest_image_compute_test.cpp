#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_compute.h"
#include "guest_texture_cache.h"
#include "shader_recompiler/backend/dxil/spirv_converter.h"
#include <dxgi1_6.h>
#include <fstream>
#include <cstdio>
#include <cstring>
using namespace NativeDX12;
int wmain(int argc,wchar_t** argv){try {
    if(argc!=4)throw std::invalid_argument("usage: guest-image-compute fixture.spv converter.dll dxc.dll");
    std::ifstream file(argv[1],std::ios::binary|std::ios::ate);if(!file)throw std::runtime_error("Cannot read fixture");
    auto bytes=file.tellg();if(bytes<=0||bytes%4)throw std::runtime_error("Invalid fixture");
    std::vector<u32> spirv(size_t(bytes)/4);file.seekg(0);file.read(reinterpret_cast<char*>(spirv.data()),bytes);
    Shader::Backend::DX12::SpirvConvertOptions options;options.stage=Shader::Backend::DX12::SpirvStage::Compute;
    options.native_storage_buffer_uavs=true;
    auto dxil=Shader::Backend::DX12::ConvertSpirv(argv[2],spirv,options);if(!dxil.success)throw std::runtime_error(dxil.diagnostics);
    ComPtr<IDXGIFactory6> factory;Check(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))));ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1>a;auto hr=DX12_API_CALL(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&a)));if(hr==DXGI_ERROR_NOT_FOUND)break;Check(hr);DXGI_ADAPTER_DESC1 d{};Check(DX12_API_CALL(a->GetDesc1(&d)));if(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;if(SUCCEEDED(DX12_API_CALL(D3D12CreateDevice(a.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)))))break;}
    if(!device)throw std::runtime_error("No hardware device");
    auto* memory=static_cast<std::byte*>(VirtualAlloc(reinterpret_cast<void*>(0x1000000000ULL),0x30000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!memory)throw std::runtime_error("Cannot allocate 40-bit guest memory");
    struct Allocation{void* p;~Allocation(){VirtualFree(p,0,MEM_RELEASE);}} owner{memory};
    memset(memory,0xee,0x30000);Rasterizer rasterizer(device.Get());rasterizer.MapMemory(reinterpret_cast<VAddr>(memory),0x30000);
    GuestTextureCache textures(device.Get());ShaderPipeline pipeline(device.Get(),argv[3],dxil.bytecode);
    Shader::Info info;info.sw_stage=Shader::SwStage::Compute;info.flattened_ud_buf.resize(24);
    Shader::BufferResource output{};output.buffer_type=Shader::BufferType::Guest;output.is_written=true;
    output.sharp_fetch.summary=Shader::SharpFetch<AmdGpu::Buffer>::Summary::SingleLoad;output.sharp_fetch.offsets[0]=0;
    info.buffers.push_back(output);AmdGpu::Buffer buffer{};buffer.base_address=reinterpret_cast<VAddr>(memory);buffer.num_records=256;
    memcpy(info.flattened_ud_buf.data(),&buffer,sizeof(buffer));
    auto make_image=[&](u32 offset,u32 data,u32 number){AmdGpu::Image sharp{};sharp.base_address=reinterpret_cast<VAddr>(memory+offset)>>8;
        sharp.width=7;sharp.height=7;sharp.pitch=7;sharp.type=u64(AmdGpu::ImageType::Color2D);sharp.data_format=data;sharp.num_format=number;sharp.tiling_index=13;
        sharp.dst_sel_x=4;sharp.dst_sel_y=5;sharp.dst_sel_z=6;sharp.dst_sel_w=7;return sharp;};
    auto target=make_image(0x10000,4,4),input=make_image(0x20000,10,0);
    Shader::ImageResource image{};image.sharp_fetch.summary=Shader::SharpFetch<AmdGpu::Image>::Summary::SingleLoad;
    image.sharp_fetch.offsets[0]=4;image.is_written=true;info.images.push_back(image);memcpy(info.flattened_ud_buf.data()+4,&target,sizeof(target));
    image.sharp_fetch.offsets[0]=12;image.is_written=false;info.images.push_back(image);memcpy(info.flattened_ud_buf.data()+12,&input,sizeof(input));
    Shader::SamplerResource sampler{};sampler.sharp_fetch.summary=Shader::SharpFetch<AmdGpu::Sampler>::Summary::SingleLoad;sampler.sharp_fetch.offsets[0]=20;info.samplers.push_back(sampler);
    AmdGpu::Sampler sampler_sharp{};sampler_sharp.clamp_x.Assign(AmdGpu::ClampMode::ClampLastTexel);sampler_sharp.clamp_y.Assign(AmdGpu::ClampMode::ClampLastTexel);
    sampler_sharp.clamp_z.Assign(AmdGpu::ClampMode::ClampLastTexel);sampler_sharp.mip_filter.Assign(AmdGpu::MipFilter::None);
    memcpy(info.flattened_ud_buf.data()+20,&sampler_sharp,sizeof(sampler_sharp));
    const auto input_desc=DescribeGuestTexture(input),target_desc=DescribeGuestTexture(target);
    const auto input_layout=LayoutGuestTexture(input_desc),target_layout=LayoutGuestTexture(target_desc);
    for(UINT y=0;y<8;++y)for(UINT x=0;x<8;++x){auto offset=GuestPixelOffset(input_desc,input_layout,0,0,x,y);
        memory[0x20000+offset]=std::byte(y*8+x);memory[0x20000+offset+3]=std::byte{255};}
    for(UINT repeat=0;repeat<2;++repeat) {
        ExecuteGuestBufferCompute(device.Get(),rasterizer,pipeline,info,{},ComputeInvocation{},&textures);
        for(UINT y=0;y<8;++y)for(UINT x=0;x<8;++x) {
            auto index=y*8+x,expected=index*4+7+repeat*5;u32 value{};
            memcpy(&value,memory+index*4,4);if(value!=expected)throw std::runtime_error("Native sampled guest buffer result mismatch");
            auto offset=GuestPixelOffset(target_desc,target_layout,0,0,x,y);memcpy(&value,memory+0x10000+offset,4);
            if(value!=expected)throw std::runtime_error("Native writable guest image retile mismatch");
        }
        // Change input without touching the cache API; next guest submission
        // must observe CPU writes and never reuse stale texture contents.
        if(!repeat)for(UINT y=0;y<8;++y)for(UINT x=0;x<8;++x){auto offset=GuestPixelOffset(input_desc,input_layout,0,0,x,y);memory[0x20000+offset]=std::byte(y*8+x+5);}
    }
    // Null PS4 image descriptors have no guest backing. Verify hardware zero
    // sampling, discarded null image stores, and preservation of prior storage.
    input.base_address=0;memcpy(info.flattened_ud_buf.data()+12,&input,sizeof(input));
    ExecuteGuestBufferCompute(device.Get(),rasterizer,pipeline,info,{},ComputeInvocation{},&textures);
    for(UINT index=0;index<64;++index){u32 value{};memcpy(&value,memory+index*4,4);
        if(value!=index*3+7)throw std::runtime_error("Native null SRV did not return zero");}
    std::vector<std::byte> preserved(memory+0x10000,memory+0x10000+target_layout.guest_bytes);
    target.base_address=0;memcpy(info.flattened_ud_buf.data()+4,&target,sizeof(target));
    ExecuteGuestBufferCompute(device.Get(),rasterizer,pipeline,info,{},ComputeInvocation{},&textures);
    if(memcmp(preserved.data(),memory+0x10000,preserved.size()))throw std::runtime_error("Null UAV changed prior image storage");
    if(GetModuleHandleW(L"vulkan-1.dll"))throw std::runtime_error("Vulkan runtime unexpectedly loaded");
    puts("PASS: PS4 buffer/image/sampler sharp metadata -> native sampled compute -> exact buffer+retiled image writes; repeated cache reuse, native null SRV/UAV behavior, no Vulkan runtime");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
