#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_compute.h"
#include "guest_depth_metadata.h"
#include "shader_recompiler/backend/dxil/spirv_converter.h"
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <fstream>
#include <cstdio>
#include <cstring>
using namespace NativeDX12;
int wmain(int argc,wchar_t** argv){try {
    GuestDepthMetadata metadata;metadata.Register(0);metadata.Register(0x1000);
    if(metadata.Contains(0)||!metadata.Consume(0x1000,0)||metadata.Consume(0x1000,0)||
       !metadata.Consume(0x1000,1))throw std::runtime_error("HTile initial clear/slice consumption mismatch");
    metadata.Register(0x1000);
    if(metadata.Consume(0x1000,0)||!metadata.Clear(0x1000)||!metadata.Consume(0x1000,0)||
       metadata.Clear(0x2000)||metadata.Consume(0x1000,32))throw std::runtime_error("HTile repeat clear/registration mismatch");
    metadata.Remove(0x1000,1);
    if(metadata.Contains(0x1000))throw std::runtime_error("Unmapped HTile metadata survived retirement");
    ComPtr<ID3D12Debug> debug;if(SUCCEEDED(DX12_API_CALL(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))))DX12_API_CALL(debug->EnableDebugLayer());
    if(argc!=4&&argc!=5)throw std::invalid_argument("usage: guest-metadata-dispatch fixture.spv converter.dll dxc.dll [wave32]");
    std::ifstream input(argv[1],std::ios::binary|std::ios::ate);if(!input)throw std::runtime_error("Cannot read fixture");
    auto bytes=input.tellg();if(bytes<=0||bytes%4)throw std::runtime_error("Invalid fixture");
    std::vector<u32> code(size_t(bytes)/4);input.seekg(0);input.read(reinterpret_cast<char*>(code.data()),bytes);
    Shader::Backend::DX12::SpirvConvertOptions options;options.stage=Shader::Backend::DX12::SpirvStage::Compute;
    if(argc==5){options.compute_wave_size=32;options.shader_model_minor=6;}
    auto dxil=Shader::Backend::DX12::ConvertSpirv(argv[2],code,options);if(!dxil.success)throw std::runtime_error(dxil.diagnostics);
    ComPtr<IDXGIFactory6> factory;Check(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))));ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1>a;auto hr=DX12_API_CALL(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&a)));if(hr==DXGI_ERROR_NOT_FOUND)break;Check(hr);DXGI_ADAPTER_DESC1 d{};Check(DX12_API_CALL(a->GetDesc1(&d)));if(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;if(SUCCEEDED(DX12_API_CALL(D3D12CreateDevice(a.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)))))break;}
    if(!device)throw std::runtime_error("No hardware device");
    // Real PS4 buffer addresses have 40 bits. Reserve an actual accessible
    // range inside that domain instead of truncating a high host heap pointer.
    void* allocation=VirtualAlloc(reinterpret_cast<void*>(0x1000000000ULL),8192,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!allocation)throw std::runtime_error("Cannot allocate low-address guest test memory");
    struct Allocation {void* p;~Allocation(){VirtualFree(p,0,MEM_RELEASE);}} owner{allocation};
    auto* guest=static_cast<u32*>(allocation);std::fill_n(guest,2048,0xdeadbeef);
    Rasterizer rasterizer(device.Get());rasterizer.MapMemory(reinterpret_cast<VAddr>(guest),8192);
    rasterizer.EnableHostSpanCache(true);
    const auto address=reinterpret_cast<VAddr>(guest);
    rasterizer.NotifyHostMapping(address,8192,true,true);
    rasterizer.ValidateHostMemory(address,8192,true);rasterizer.ValidateHostMemory(address,8192,true);
    DWORD previous{};Check(VirtualProtect(guest+1024,4096,PAGE_READONLY,&previous)?S_OK:E_FAIL);
    rasterizer.NotifyMemoryProtection(address+4096,4096);
    rasterizer.NotifyHostMapping(address+4096,4096,true,false);
    bool inaccessible=false;try{rasterizer.ValidateHostMemory(address,8192,true);}catch(const std::runtime_error&){inaccessible=true;}
    if(!inaccessible)throw std::runtime_error("Cached validation ignored changed write permissions");
    rasterizer.ValidateHostMemory(address,8192,false);
    Check(VirtualProtect(guest+1024,4096,PAGE_READWRITE,&previous)?S_OK:E_FAIL);
    rasterizer.NotifyMemoryProtection(address+4096,4096);rasterizer.ValidateHostMemory(address,8192,true);
    Check(VirtualFree(guest+1024,4096,MEM_DECOMMIT)?S_OK:E_FAIL);rasterizer.RegisterMemory(address+4096,4096);
    inaccessible=false;try{rasterizer.ValidateHostMemory(address,8192,false);}catch(const std::runtime_error&){inaccessible=true;}
    if(!inaccessible)throw std::runtime_error("Cached validation ignored decommitted backing");
    if(!VirtualAlloc(guest+1024,4096,MEM_COMMIT,PAGE_READWRITE))throw std::runtime_error("Cannot recommit test backing");
    rasterizer.RegisterMemory(address+4096,4096);rasterizer.ValidateHostMemory(address,8192,true);
    std::fill_n(guest,2048,0xdeadbeef);
    ShaderPipeline pipeline(device.Get(),argv[3],dxil.bytecode);
    Shader::Info info;info.sw_stage=Shader::SwStage::Compute;
    Shader::BufferResource source{};source.buffer_type=Shader::BufferType::Guest;source.is_written=true;
    source.sharp_fetch.summary=Shader::SharpFetch<AmdGpu::Buffer>::Summary::SingleLoad;
    source.sharp_fetch.offsets[0]=0;info.buffers.push_back(source);
    AmdGpu::Buffer sharp{};sharp.base_address=reinterpret_cast<VAddr>(guest);sharp.num_records=8192;
    info.flattened_ud_buf.resize(4);memcpy(info.flattened_ud_buf.data(),&sharp,sizeof(sharp));
    ComputeInvocation invocation{{16,1,1}};
    ExecuteGuestBufferCompute(device.Get(),rasterizer,pipeline,info,{},invocation);
    for(UINT i=0;i<1024;++i)if(guest[i]!=i*3+7)throw std::runtime_error("Actual guest descriptor dispatch/readback mismatch");
    for(UINT i=1024;i<2048;++i)if(guest[i]!=0xdeadbeef)throw std::runtime_error("Compute wrote beyond requested invocation count");
    std::fill_n(guest,2048,0xdeadbeef);invocation.base[0]=1;
    ExecuteGuestBufferCompute(device.Get(),rasterizer,pipeline,info,{},invocation);
    for(UINT i=0;i<2048;++i)if(guest[i]!=(i>=64&&i<1088?i*3+7:0xdeadbeef))
        throw std::runtime_error("Guest nonzero base workgroup/runtime CBV mismatch");
    invocation.base[0]=0;
    // A real 4K clear asks for 130560 groups. Cross the D3D12 65535
    // boundary on hardware, including a nonzero guest base and untouched tail.
    constexpr u32 split_groups=65536,split_words=split_groups*64+128;
    auto* split_memory=static_cast<u32*>(VirtualAlloc(reinterpret_cast<void*>(0x1001000000ULL),
        u64(split_words)*4,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!split_memory)throw std::runtime_error("Cannot allocate split dispatch fixture");
    Allocation split_owner{split_memory};std::fill_n(split_memory,split_words,0xdeadbeef);
    const auto split_address=reinterpret_cast<VAddr>(split_memory);
    rasterizer.MapMemory(split_address,u64(split_words)*4);
    AmdGpu::Buffer split_sharp{};split_sharp.base_address=split_address;split_sharp.num_records=split_words*4;
    memcpy(info.flattened_ud_buf.data(),&split_sharp,sizeof(split_sharp));
    ComputeInvocation split_invocation{{split_groups,1,1},{1,0,0}};
    ExecuteGuestBufferCompute(device.Get(),rasterizer,pipeline,info,{},split_invocation);
    for(u32 i=0;i<split_words;++i)if(split_memory[i]!=(i>=64&&i<split_groups*64+64?i*3+7:0xdeadbeef))
        throw std::runtime_error("Split native dispatch lost base/group boundary or changed untouched bytes");
    rasterizer.UnmapMemory(split_address,u64(split_words)*4);
    memcpy(info.flattened_ud_buf.data(),&sharp,sizeof(sharp));
    // Very large guest extents must never become unbounded native allocations.
    sharp.stride=16383;sharp.num_records=UINT_MAX;memcpy(info.flattened_ud_buf.data(),&sharp,sizeof(sharp));
    bool rejected=false;try{ExecuteGuestBufferCompute(device.Get(),rasterizer,pipeline,info,{},invocation);}catch(const std::out_of_range&){rejected=true;}
    if(!rejected)throw std::runtime_error("Overflowing guest buffer extent accepted");
    // The same dispatch can write GDS and a temporary shader scratch buffer.
    info.buffers[0].buffer_type=Shader::BufferType::GdsBuffer;
    ExecuteGuestBufferCompute(device.Get(),rasterizer,pipeline,info,{},invocation);
    if(rasterizer.ReadDataFromGds(4092)!=1023*3+7)throw std::runtime_error("Guest GDS result mismatch");
    info.buffers[0].buffer_type=Shader::BufferType::SharedMemory;invocation.shared_bytes=4096;
    ExecuteGuestBufferCompute(device.Get(),rasterizer,pipeline,info,{},invocation);
    info.buffers[0].buffer_type=Shader::BufferType::Flatbuf;rejected=false;
    try{ExecuteGuestBufferCompute(device.Get(),rasterizer,pipeline,info,{},invocation);}catch(const std::runtime_error&){rejected=true;}
    if(!rejected)throw std::runtime_error("Writable flat user data accepted");
    Shader::Info copy_info;copy_info.pgm_hash=0xfefebf9f;copy_info.flattened_ud_buf.resize(12);
    const VAddr copy_bases[]{address,address+1024,address+4096};
    for(u32 index=0;index<3;++index) {
        AmdGpu::Buffer copy_sharp{};copy_sharp.base_address=copy_bases[index];copy_sharp.stride=index?4:12;
        copy_sharp.num_records=UINT_MAX;
        memcpy(copy_info.flattened_ud_buf.data()+index*4,&copy_sharp,sizeof(copy_sharp));
        Shader::BufferResource resource{};resource.sharp_fetch.summary=Shader::SharpFetch<AmdGpu::Buffer>::Summary::SingleLoad;
        resource.sharp_fetch.offsets[0]=u16(index*4);copy_info.buffers.push_back(resource);
    }
    const u32 copy_commands[]{4,2,7,16,12,3};memcpy(guest,copy_commands,sizeof(copy_commands));
    for(u32 index=0;index<32;++index)guest[256+index]=index+100;
    std::fill_n(guest+1024,32,0xdeadbeef);
    ComputeInvocation copy_invocation{{2,1,1}};
    if(!ExecuteGuestCopyShader(rasterizer,copy_info,copy_invocation))throw std::runtime_error("Known guest copy shader not handled");
    for(u32 index=0;index<32;++index){
        const u32 expected=index>=4&&index<12?index-4+2+100:index>=16&&index<20?index-16+12+100:0xdeadbeef;
        if(guest[1024+index]!=expected)throw std::runtime_error("Native HLE copy shader changed untouched bytes or copied wrong offsets");
    }
    copy_info.pgm_hash=0;
    if(ExecuteGuestCopyShader(rasterizer,copy_info,copy_invocation))throw std::runtime_error("Unknown guest shader treated as copy");
    // CPU HLE must retire a preceding GPU write, then invalidate cached
    // destinations so the following native shader sees exactly the new bytes.
    copy_info.pgm_hash=0xfefebf9f;
    const u32 gpu_hle_commands[]{0,0,3};memcpy(guest,gpu_hle_commands,sizeof(gpu_hle_commands));
    rasterizer.FillBuffer(address+1024,16,0x11223344,false);
    copy_invocation.groups[0]=1;
    ExecuteGuestCopyShader(rasterizer,copy_info,copy_invocation);
    for(u32 i=0;i<4;++i)if(guest[1024+i]!=0x11223344)throw std::runtime_error("CPU copy HLE read stale GPU source");
    const Rasterizer::BufferCopy hle_readback[]{{address+2048,address+4096,16}};
    rasterizer.CopyBufferBatch(hle_readback);
    for(u32 i=0;i<4;++i)if(guest[512+i]!=0x11223344)throw std::runtime_error("Native copy read stale CPU HLE destination");
    copy_invocation.groups[0]=2;
    // Every packet must preserve ordered overlapping memmove behavior, even
    // when a later packet reads a destination produced by the preceding one.
    for(u32 i=0;i<2048;++i)guest[i]=i+700;
    std::vector<u32> expected(guest,guest+2048);
    std::vector<Rasterizer::BufferCopy> packet;
    for(u32 i=0;i<257;++i){
        const u32 from=(i*7)%128,to=(i*11)%128,count=16;
        packet.push_back({address+to*4,address+from*4,count*4});
        memmove(expected.data()+to,expected.data()+from,count*4);
    }
    rasterizer.CopyBufferBatch(packet);
    if(memcmp(guest,expected.data(),8192))throw std::runtime_error("Native GPU copy packets lost ordered aliases/untouched bytes");
    // The source can still reside in a dirty GPU buffer from a prior operation.
    rasterizer.FillBuffer(address+4096,64,0x12345678,false);
    const Rasterizer::BufferCopy gpu_source[]{ {address+1024,address+4096,64} };
    rasterizer.CopyBufferBatch(gpu_source);
    for(u32 i=0;i<16;++i)if(guest[256+i]!=0x12345678)
        throw std::runtime_error("Native GPU copy used a stale CPU source");
    const auto unchanged=guest[256];
    const Rasterizer::BufferCopy invalid_packet[]{ {address+1024,address+4096,64},{address+8192,address,4} };
    bool invalid_copy=false;try{rasterizer.CopyBufferBatch(invalid_packet);}catch(const std::exception&){invalid_copy=true;}
    if(!invalid_copy||guest[256]!=unchanged)throw std::runtime_error("Invalid copy packet mutated a destination before validation");
    // Guest control arithmetic wraps at 32 bits before adding the base address.
    copy_info.pgm_hash=0xfefebf9f;
    const u32 wrapped_commands[]{0,0x40000001,0,1,0,UINT_MAX};
    memcpy(guest,wrapped_commands,sizeof(wrapped_commands));guest[257]=0xaabbccdd;
    guest[1024]=0;guest[1025]=0x76543210;
    if(!ExecuteGuestCopyShader(rasterizer,copy_info,copy_invocation)||guest[1024]!=0xaabbccdd||guest[1025]!=0x76543210)
        throw std::runtime_error("Native copy HLE lost u32 offset wrap/zero-byte semantics");
    // Odd byte strides retain the CPU fallback, without expanding into padding.
    for(u32 index=1;index<3;++index){AmdGpu::Buffer descriptor{};
        memcpy(&descriptor,copy_info.flattened_ud_buf.data()+index*4,sizeof(descriptor));descriptor.stride=3;
        memcpy(copy_info.flattened_ud_buf.data()+index*4,&descriptor,sizeof(descriptor));}
    const u32 byte_commands[]{1,2,0};memcpy(guest,byte_commands,sizeof(byte_commands));
    auto* raw=reinterpret_cast<std::byte*>(guest);raw[1030]=std::byte{17};raw[1031]=std::byte{23};raw[1032]=std::byte{31};
    memset(raw+4096,0x6e,12);copy_invocation.groups[0]=1;
    ExecuteGuestCopyShader(rasterizer,copy_info,copy_invocation);
    for(u32 i=0;i<12;++i){const auto value=i>=3&&i<6?raw[1030+i-3]:std::byte{0x6e};
        if(raw[4096+i]!=value)throw std::runtime_error("Byte-stride copy fallback changed untouched padding");}
    rasterizer.ValidateDebugMessages();
    puts("PASS: original PS4 guest sharp metadata -> native reflected descriptors -> actual GPU dispatch -> exact guest/GDS results; scratch lifetime, extent overflow and invalid writes checked");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
