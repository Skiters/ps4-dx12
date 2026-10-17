// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_compute.h"
#include "video_core/renderer_d3d12/backend_factory.h"
#include "shader_recompiler/backend/dxil/spirv_converter.h"
#include "shader_recompiler/backend/dxil/dxil_compiler.h"
#include "common/scope_exit.h"
#include <fstream>
#include <cstdio>
#include <cstring>
using namespace NativeDX12;
namespace DXIL=Shader::Backend::DX12;
static void RunDisjointNativeViews(ID3D12Device* device,const wchar_t* compiler){
    const auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    const auto shader=DXIL::CompileComputeDxil(compiler,R"(
ByteAddressBuffer source:register(t0);
RWByteAddressBuffer destination:register(u1);
[numthreads(1,1,1)] void main(){destination.Store(0,source.Load(0)+7);}
)");
    require(shader.success,"Cannot compile disjoint native shader views");
    ShaderPipeline pipeline(device,compiler,shader.bytecode);
    constexpr u64 bytes=81920;
    // Guest buffer sharps encode 40-bit addresses; a random host allocation
    // can lie above that range and would be truncated by the real descriptor.
    auto* memory=static_cast<u32*>(VirtualAlloc(reinterpret_cast<void*>(0x1100000000ULL),bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    require(memory!=nullptr,"Cannot allocate disjoint native-view guest backing");
    SCOPE_EXIT {VirtualFree(memory,0,MEM_RELEASE);};
    const auto base=reinterpret_cast<VAddr>(memory);
    for(const bool grow:{false,true}){
        std::fill_n(memory,bytes/4,0x01010101U);
        Rasterizer renderer(device);renderer.MapMemory(base,bytes);u64 committed{};
        renderer.SetBufferOwnership([](VAddr,u64){return true;},[&](VAddr address,std::span<const std::byte> data){
            committed+=data.size();memcpy(reinterpret_cast<void*>(address),data.data(),data.size());return true;
        },[](VAddr,u64){});
        const Rasterizer::ShaderBufferRequest initial{base,65536,false,true,true};
        renderer.PrepareShaderBuffers(std::span(&initial,1));
        renderer.FillPreparedShaderBuffer(base,65536,0x11223344,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        const Rasterizer::ShaderBufferRequest requests[]{{base+12,20480,false,false,true},{base+32768,4,true,true,true}};
        auto selected=renderer.PrepareShaderBuffers(requests);
        require(selected[0].read_snapshot&&selected[0].resource.Get()!=selected[1].resource.Get()&&
            renderer.CachedBufferBytes()==65536,"Disjoint views split/lost canonical GPU B or shared incompatible native states");
        renderer.FillPreparedShaderBuffer(base+12,4,0x31323334,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        if(grow){const Rasterizer::ShaderBufferRequest extension{base+65532,16388,false,true,true};
            renderer.PrepareShaderBuffers(std::span(&extension,1));}
        ResourceBindings bindings(device,pipeline.Layout());
        SCOPE_EXIT {renderer.Finish();};
        bindings.BeforeBind([&](ResourceBindings& table){
            const auto current=renderer.RefreshPreparedShaderBuffers(requests,selected);
            require(current[0].resource.Get()==selected[0].resource.Get(),"Final native read view allocated a replacement snapshot");
            D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R32_TYPELESS;srv.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;
            srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Buffer.FirstElement=current[0].offset/4;
            srv.Buffer.NumElements=UINT(current[0].size/4);srv.Buffer.Flags=D3D12_BUFFER_SRV_FLAG_RAW;
            table.Srv(0,0,current[0].resource.Get(),srv);
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.Format=DXGI_FORMAT_R32_TYPELESS;uav.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;
            uav.Buffer.FirstElement=current[1].offset/4;uav.Buffer.NumElements=UINT(current[1].size/4);uav.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
            table.Uav(1,0,current[1].resource.Get(),uav);
            for(const auto& view:current)table.RetainAllocation(view.allocation);
        });
        if(grow)renderer.ExecuteCompute(pipeline,bindings,1,1,1,false);
        else{
            Shader::Info info;info.sw_stage=Shader::SwStage::Compute;info.flattened_ud_buf.resize(8);
            for(u32 i=0;i<2;++i){
                AmdGpu::Buffer sharp{};sharp.base_address=i?base+32768:base+12;sharp.num_records=i?4:20480;
                memcpy(info.flattened_ud_buf.data()+i*4,&sharp,sizeof(sharp));
                Shader::BufferResource resource{};resource.buffer_type=Shader::BufferType::Guest;resource.is_written=i!=0;
                resource.sharp_fetch.summary=Shader::SharpFetch<AmdGpu::Buffer>::Summary::SingleLoad;
                resource.sharp_fetch.offsets[0]=u16(i*4);info.buffers.push_back(resource);
            }
            ExecuteGuestBufferCompute(device,renderer,pipeline,info,{},{{1,1,1}});
        }
        const u64 extent=grow?bytes:65536;
        auto snapshot=renderer.CapturePreparedShaderBuffer(base,extent);
        auto readback=Buffer(device,extent,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        snapshot.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        renderer.RecordingList()->CopyBufferRegion(readback.handle.Get(),0,snapshot.handle.Get(),0,extent);renderer.Finish();
        void* mapped{};D3D12_RANGE all{0,SIZE_T(extent)};Check(readback.handle->Map(0,&all,&mapped));
        bool correct=true;for(u64 offset=0;offset<extent;offset+=4){const auto word=static_cast<const u32*>(mapped)[offset/4];
            const auto expected=offset==12?0x31323334U:offset==32768?0x3132333bU:offset<65536?0x11223344U:0x01010101U;
            correct&=word==expected;}
        D3D12_RANGE empty{};readback.handle->Unmap(0,&empty);require(correct,"Disjoint native SRV read stale/CPU B, lost UAV stores or original canonical padding");
        require(committed==0,"Disjoint native-view preparation prematurely committed GPU contents");
        require(renderer.ReadMemory(base+32768,4)&&committed==4,"Disjoint native UAV write did not become exactly CPU-visible");
        for(u64 i=0;i<bytes/4;++i)require(memory[i]==(i==32768/4?0x3132333bU:0x01010101U),"Disjoint native views changed physical B0 outside the actual store");
        auto wrong=selected;++wrong[0].offset;bool rejected=false;
        try{renderer.RefreshPreparedShaderBuffers(requests,wrong);}catch(const std::logic_error&){rejected=true;}
        require(rejected,"Private native read snapshot accepted a substituted offset");
        auto overlap=requests[0];overlap.address=base+32768;overlap.size=4;
        const Rasterizer::ShaderBufferRequest aliases[]{overlap,requests[1]};rejected=false;
        try{renderer.PrepareShaderBuffers(aliases);}catch(const std::runtime_error&){rejected=true;}
        require(rejected,"Private native read snapshot hid a true same-command SRV/UAV alias");
        renderer.UnmapMemory(base,bytes);renderer.MapMemory(base,bytes);renderer.PrepareShaderBuffers(std::span(&initial,1));rejected=false;
        try{renderer.RefreshPreparedShaderBuffers(requests,selected);}catch(const std::logic_error&){rejected=true;}
        require(rejected,"Private native read snapshot accepted unrelated recreated B");renderer.ValidateDebugMessages();
    }
}
int wmain(int argc,wchar_t** argv){try{
    if(argc!=4)throw std::runtime_error("usage: buffer-alias-test fixture converter dxc");
    _putenv_s("SHADPS4_NATIVE_D3D12_VALIDATION","1");
    std::ifstream input(argv[1],std::ios::binary|std::ios::ate);
    const auto bytes=input.tellg();if(bytes<=0||bytes%4)throw std::runtime_error("Invalid alias fixture");
    std::vector<u32> code(size_t(bytes)/4);input.seekg(0);input.read(reinterpret_cast<char*>(code.data()),bytes);
    auto device=CreateHardwareDevice();
    RunDisjointNativeViews(device.Get(),argv[3]);
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
