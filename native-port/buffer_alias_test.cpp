// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_compute.h"
#include "video_core/renderer_d3d12/backend_factory.h"
#include "shader_recompiler/backend/dxil/spirv_converter.h"
#include <fstream>
#include <cstdio>
#include <cstring>
using namespace NativeDX12;
namespace DXIL=Shader::Backend::DX12;
int wmain(int argc,wchar_t** argv){try{
    if(argc!=4)throw std::runtime_error("usage: buffer-alias-test fixture converter dxc");
    _putenv_s("SHADPS4_NATIVE_D3D12_VALIDATION","1");
    std::ifstream input(argv[1],std::ios::binary|std::ios::ate);
    const auto bytes=input.tellg();if(bytes<=0||bytes%4)throw std::runtime_error("Invalid alias fixture");
    std::vector<u32> code(size_t(bytes)/4);input.seekg(0);input.read(reinterpret_cast<char*>(code.data()),bytes);
    auto device=CreateHardwareDevice();
    // Both conversion entry points must emit the same access class; the game
    // uses linked graphics stages as well as standalone compute.
    DXIL::SpirvConvertOptions options;options.stage=DXIL::SpirvStage::Compute;
    auto ordinary=DXIL::ConvertSpirv(argv[2],code,options);
    if(!ordinary.success)throw std::runtime_error(ordinary.diagnostics);
    ShaderPipeline original(device.Get(),argv[3],ordinary.bytecode);
    bool original_srv=false;for(const auto& b:original.Layout().Bindings())
        original_srv|=b.space==0&&b.reg==0&&b.type==D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    if(!original_srv)throw std::runtime_error("Fixture did not exercise a read-only SSBO");
    options.native_storage_buffer_uavs=true;
    auto converted=DXIL::ConvertSpirv(argv[2],code,options);
    if(!converted.success)throw std::runtime_error(converted.diagnostics);
    DXIL::LinkedSpirvStage stage{DXIL::SpirvStage::Compute,code};
    auto linked=DXIL::ConvertLinkedStages(argv[2],std::span(&stage,1),options);
    if(!linked.success)throw std::runtime_error(linked.diagnostics);
    auto* memory=static_cast<u32*>(VirtualAlloc(reinterpret_cast<void*>(0x1000000000ULL),4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!memory)throw std::runtime_error("Cannot allocate guest alias memory");
    struct Allocation{void* p;~Allocation(){VirtualFree(p,0,MEM_RELEASE);}} owner{memory};
    const auto address=reinterpret_cast<VAddr>(memory);
    for(const auto* bytecode:{&converted.bytecode,&linked.stages.at(0).bytecode}){
        ShaderPipeline pipeline(device.Get(),argv[3],*bytecode);
        for(const auto& b:pipeline.Layout().Bindings())if(b.space==0&&b.reg<2&&b.type!=D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
            throw std::runtime_error("Aliased SSBO retained incompatible native descriptor class");
        for(u32 i=0;i<1024;++i)memory[i]=i+100;
        Rasterizer rasterizer(device.Get());rasterizer.MapMemory(address,4096);
        Shader::Info info;info.sw_stage=Shader::SwStage::Compute;info.flattened_ud_buf.resize(8);
        for(u32 i=0;i<2;++i){
            AmdGpu::Buffer descriptor{};descriptor.base_address=address;descriptor.num_records=4096;
            memcpy(info.flattened_ud_buf.data()+i*4,&descriptor,sizeof(descriptor));
            Shader::BufferResource resource{};resource.buffer_type=Shader::BufferType::Guest;resource.is_written=i==1;
            resource.sharp_fetch.summary=Shader::SharpFetch<AmdGpu::Buffer>::Summary::SingleLoad;
            resource.sharp_fetch.offsets[0]=u16(i*4);info.buffers.push_back(resource);
        }
        ComputeInvocation invocation{{4,1,1}};
        ExecuteGuestBufferCompute(device.Get(),rasterizer,pipeline,info,{},invocation);
        for(u32 i=0;i<1024;++i){const auto expected=i>=512&&i<768?(i-512+100)*3+7:i+100;
            if(memory[i]!=expected)throw std::runtime_error("Native aliased read/write buffer changed exact guest results");}
        // Exercise retired GPU-written backing again, then change CPU memory
        // without an invalidation callback. The native coherence comparison
        // must preserve the first GPU result and detect the subsequent write.
        ExecuteGuestBufferCompute(device.Get(),rasterizer,pipeline,info,{},invocation);
        for(u32 i=0;i<256;++i)memory[i]=i+2000;
        memory[900]=0x12345678;
        ExecuteGuestBufferCompute(device.Get(),rasterizer,pipeline,info,{},invocation);
        for(u32 i=0;i<1024;++i){
            const auto expected=i<256?i+2000:i>=512&&i<768?(i-512+2000)*3+7:i==900?0x12345678:i+100;
            if(memory[i]!=expected)throw std::runtime_error("Retired GPU buffer reuse missed a CPU write or overwrote untouched bytes");
        }
        rasterizer.ValidateDebugMessages();
    }
    if(GetModuleHandleW(L"vulkan-1.dll"))throw std::runtime_error("Unexpected Vulkan runtime");
    puts("PASS: read-only/writable SSBO aliases unified in standalone and linked DXIL; actual native GPU results and untouched bytes verified");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
