// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/shader_pipeline.h"
#include "video_core/renderer_d3d12/guest_graphics_state.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include "shader_recompiler/backend/dxil/dxil_compiler.h"
#include "core/emulator_settings.h"
#include "stream_output_layout_checks.h"
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <cstdio>
#include <cstring>

using namespace NativeDX12;
namespace DXIL=Shader::Backend::DX12;
int main(int argc,char** argv){try{
    if(argc!=4)throw std::invalid_argument("Expected converter, DXC and primitive stream mode");
    Tests::CheckStreamOutputLayout();
    EmulatorSettings.SetPipelineCacheEnabled(false);_putenv_s("SHADPS4_NATIVE_SHADER_CACHE","off");
    const auto dxc=std::filesystem::path(argv[2]).wstring();
    const auto vertex=DXIL::CompileDxil(dxc,R"(
struct V {float4 position:SV_Position;uint4 payload:CAPTURE0;};
V main(uint id:SV_VertexID,uint instance:SV_InstanceID){
    V o;o.position=float4(0,0,0,1);
    o.payload=uint4(id,instance,0x7fc12345u,0x80000000u);return o;
})",DXIL::Stage::Vertex);
    const auto geometry=DXIL::CompileGeometryDxil(dxc,R"(
struct V {float4 position:SV_Position;uint4 payload:CAPTURE0;};
[maxvertexcount(3)] void main(triangle V input[3],uint primitive:SV_PrimitiveID,inout PointStream<V> output){
    for(uint i=0;i<3;++i){V o=input[i];o.payload.z^=primitive;output.Append(o);}
})");
    if(!vertex.success||!geometry.success)throw std::runtime_error(vertex.diagnostics+geometry.diagnostics);
    // This explicit mode is CPU-only. Return before any D3D12 device, adapter,
    // command list, resource or PSO is created; it does not prove stream contents.
    if(std::string_view(argv[3])=="compile-only"){
        puts("PASS: internal primitive stream layout and host VS/GS DXIL compilation; GPU NOT_RUN");return 0;
    }
    ComPtr<ID3D12Debug> debug;Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));
    ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> adapter;
        const auto hr=factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter));
        if(hr==DXGI_ERROR_NOT_FOUND)break;Check(hr);DXGI_ADAPTER_DESC1 desc{};Check(adapter->GetDesc1(&desc));
        if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
        if(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))break;
    }
    if(!device)throw std::runtime_error("No hardware D3D12 device");
    ComPtr<ID3D12InfoQueue> messages;Check(device.As(&messages));
    AmdGpu::Regs regs{};regs.stage_enable.raw=AmdGpu::ShaderStageEnable::Vs;
    regs.primitive_type=AmdGpu::PrimitiveType::TriangleList;
    regs.color_control.mode=AmdGpu::ColorControl::OperationMode::Normal;
    regs.color_control.rop3=AmdGpu::ColorControl::LogicOp::Copy;
    auto state=BuildGuestGraphicsState(regs,{});state.rasterizer_discard=true;
    Rasterizer renderer(device.Get());
    for(unsigned variant=0;variant<3;++variant){
        std::array<D3D12_SO_DECLARATION_ENTRY,2> declarations{{{0,"CAPTURE",0,0,4,0},{0,nullptr,0,0,1,0}}};
        UINT stride=variant?8:16;
        if(variant==1){declarations[0].StartComponent=1;declarations[0].ComponentCount=2;}
        if(variant==2)declarations[0].ComponentCount=1;
        StreamOutputLayout layout({declarations.data(),variant==2?2u:1u,&stride,1,D3D12_SO_NO_RASTERIZED_STREAM});
        state.pipeline.StreamOutput=layout.Desc();
        ShaderPipeline::GraphicsShaders shaders;shaders.vertex=vertex.bytecode;shaders.geometry=geometry.bytecode;
        ShaderPipeline pipeline(device.Get(),dxc,shaders,state.pipeline);
        ResourceBindings bindings(device.Get(),pipeline.Layout());
        auto output=Buffer(device.Get(),256,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
        auto counter=Buffer(device.Get(),8,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
        auto upload=Buffer(device.Get(),264,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        auto readback=Buffer(device.Get(),264,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        void* mapped{};D3D12_RANGE none{};Check(upload.handle->Map(0,&none,&mapped));
        memset(mapped,0xcd,256);memset(static_cast<std::byte*>(mapped)+256,0,8);
        D3D12_RANGE written{0,264};upload.handle->Unmap(0,&written);
        auto* list=renderer.RecordingList();list->CopyBufferRegion(output.handle.Get(),0,upload.handle.Get(),0,256);
        list->CopyBufferRegion(counter.handle.Get(),0,upload.handle.Get(),256,8);
        output.Transition(list,D3D12_RESOURCE_STATE_STREAM_OUT);counter.Transition(list,D3D12_RESOURCE_STATE_STREAM_OUT);
        const D3D12_STREAM_OUTPUT_BUFFER_VIEW view{output.handle->GetGPUVirtualAddress()+16,UINT64(12)*stride,counter.handle->GetGPUVirtualAddress()};
        list->SOSetTargets(0,1,&view);
        const D3D12_VIEWPORT viewport{0,0,1,1,0,1};const D3D12_RECT scissor{0,0,1,1};const float blend[4]{};
        renderer.ExecuteGraphics(pipeline,bindings,state,{},nullptr,std::span(&viewport,1),std::span(&scissor,1),blend,{6,2});
        list=renderer.RecordingList();list->SOSetTargets(0,0,nullptr);
        output.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);counter.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
        list->CopyBufferRegion(readback.handle.Get(),0,output.handle.Get(),0,256);
        list->CopyBufferRegion(readback.handle.Get(),256,counter.handle.Get(),0,8);renderer.Finish();
        std::array<std::byte,264> expected;expected.fill(std::byte{0xcd});
        for(u32 instance=0;instance<2;++instance)for(u32 vertex_id=0;vertex_id<6;++vertex_id){
            const u32 payload[]{vertex_id,instance,0x7fc12345u^(vertex_id/3),0x80000000u};
            const auto offset=16+(instance*6+vertex_id)*stride;
            if(!variant)memcpy(expected.data()+offset,payload,16);
            else if(variant==1)memcpy(expected.data()+offset,payload+1,8);
            else memcpy(expected.data()+offset,payload,4); // Gap DWORD must remain untouched.
        }
        const u64 bytes=u64(12)*stride;memcpy(expected.data()+256,&bytes,8);
        D3D12_RANGE all{0,264};Check(readback.handle->Map(0,&all,&mapped));
        const bool equal=!memcmp(mapped,expected.data(),expected.size());readback.handle->Unmap(0,&none);
        if(!equal)throw std::runtime_error("Primitive stream records/order/counter/gap/guard mismatch");
    }
    for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T bytes{};Check(messages->GetMessage(i,nullptr,&bytes));
        std::vector<std::byte> data(bytes);auto* message=reinterpret_cast<D3D12_MESSAGE*>(data.data());
        Check(messages->GetMessage(i,message,&bytes));
        if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);
    }
    puts("PASS: internal primitive stream full/sliced/gap records, instance/primitive order, filled size and exact guards");return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
