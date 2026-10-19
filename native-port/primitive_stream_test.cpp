// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/shader_pipeline.h"
#include "video_core/renderer_d3d12/guest_graphics_state.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include "shader_recompiler/backend/dxil/dxil_compiler.h"
#include "core/emulator_settings.h"
#include "stream_output_layout_checks.h"
#include "video_core/renderer_d3d12/primitive_stream_arguments.h"
#include "video_core/renderer_d3d12/guest_point_shader.h"
#include "shader_recompiler/backend/dxil/spirv_converter.h"
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
    const auto reader_vertex=DXIL::CompileDxil(dxc,R"(
ByteAddressBuffer records:register(t0);
struct V {float4 position:SV_Position;uint4 payload:CAPTURE0;};
V main(uint id:SV_VertexID){V o;o.position=float4(0,0,0,1);o.payload=records.Load4(id*16);return o;}
)",DXIL::Stage::Vertex);
    const auto reader_geometry=DXIL::CompileGeometryDxil(dxc,R"(
struct V {float4 position:SV_Position;uint4 payload:CAPTURE0;};
[maxvertexcount(3)] void main(triangle V input[3],inout PointStream<V> output){
    for(uint i=0;i<3;++i)output.Append(input[i]);
})");
    if(!vertex.success||!geometry.success||!reader_vertex.success||!reader_geometry.success)
        throw std::runtime_error(vertex.diagnostics+geometry.diagnostics+reader_vertex.diagnostics+reader_geometry.diagnostics);
    const auto serialized_vertex=DXIL::CompileDxil(dxc,R"(
float4 main(uint id:SV_VertexID,uint instance:SV_InstanceID):SV_Position{
    return asfloat(uint4(id,instance,0x7fc12345u,0x80000000u));
})",DXIL::Stage::Vertex);
    Shader::HwGeometryRuntimeInfo serialized_geometry{};
    serialized_geometry.in_vertex_data_size=4;serialized_geometry.in_primitive=AmdGpu::PrimitiveType::TriangleList;
    const auto serialized_words=EmitNativePrimitiveStream(serialized_geometry);
    DXIL::SpirvConvertOptions serialized_options;serialized_options.stage=DXIL::SpirvStage::Geometry;
    const auto serialized=DXIL::ConvertSpirv(std::filesystem::path(argv[1]).wstring(),serialized_words,serialized_options);
    if(!serialized.success||!serialized_vertex.success)
        throw std::runtime_error(serialized.diagnostics+serialized_vertex.diagnostics);
    const auto serialized_layout=ReflectPrimitiveStreamLayout(dxc,serialized.bytecode);
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
        if(!variant){
            ShaderPipeline::GraphicsShaders reader_shaders;reader_shaders.vertex=reader_vertex.bytecode;
            reader_shaders.geometry=reader_geometry.bytecode;
            ShaderPipeline reader_pipeline(device.Get(),dxc,reader_shaders,state.pipeline);
            for(unsigned part=0;part<4;++part){
                auto destination=Buffer(device.Get(),256,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
                auto destination_counter=Buffer(device.Get(),8,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
                auto arguments=renderer.AcquireTransientBuffer(64);
                auto empty_counter=Buffer(device.Get(),4,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
                auto result=Buffer(device.Get(),328,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
                list=renderer.RecordingList();list->CopyBufferRegion(destination.handle.Get(),0,upload.handle.Get(),0,256);
                list->CopyBufferRegion(destination_counter.handle.Get(),0,upload.handle.Get(),256,8);
                list->CopyBufferRegion(empty_counter.handle.Get(),0,upload.handle.Get(),256,4);
                arguments.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
                list->CopyBufferRegion(arguments.handle.Get(),0,upload.handle.Get(),0,64);
                const u32 first=part==1?3:part==2?9:0,limit=part==1?6:part==2?3:12;
                const auto range=PrimitiveStreamRange::Make(16,12,3,first,limit);
                {
                    PrimitiveStreamArguments generate(device.Get());
                    generate.Record(renderer,part==3?empty_counter:counter,arguments,range);
                } // PSO/root must remain alive until the queued compute retires.
                ResourceBindings reader_bindings(device.Get(),reader_pipeline.Layout());
                D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R32_TYPELESS;
                srv.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srv.Buffer.FirstElement=4;srv.Buffer.NumElements=48;srv.Buffer.Flags=D3D12_BUFFER_SRV_FLAG_RAW;
                output.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                reader_bindings.Srv(0,0,output.handle.Get(),srv);
                destination.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_STREAM_OUT);
                destination_counter.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_STREAM_OUT);
                const D3D12_STREAM_OUTPUT_BUFFER_VIEW target{destination.handle->GetGPUVirtualAddress()+16,192,
                    destination_counter.handle->GetGPUVirtualAddress()};
                renderer.RecordingList()->SOSetTargets(0,1,&target);
                const Rasterizer::StreamDrawArguments indirect{&arguments,0};
                // CPU counts deliberately zero: only GPU-produced arguments
                // decide the draw. This is not a CPU filled-size approximation.
                renderer.ExecuteGraphics(reader_pipeline,reader_bindings,state,{},nullptr,
                    std::span(&viewport,1),std::span(&scissor,1),blend,{0,0},{},nullptr,false,&indirect);
                list=renderer.RecordingList();list->SOSetTargets(0,0,nullptr);
                destination.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
                destination_counter.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
                arguments.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
                list->CopyBufferRegion(result.handle.Get(),0,destination.handle.Get(),0,256);
                list->CopyBufferRegion(result.handle.Get(),256,destination_counter.handle.Get(),0,8);
                list->CopyBufferRegion(result.handle.Get(),264,arguments.handle.Get(),0,64);renderer.Finish();
                std::array<std::byte,328> replay_expected;replay_expected.fill(std::byte{0xcd});
                const u32 count=part==3?0:limit;
                for(u32 record=0;record<count;++record)
                    memcpy(replay_expected.data()+16+record*16,expected.data()+16+(first+record)*16,16);
                const u64 replay_bytes=u64(count)*16;memcpy(replay_expected.data()+256,&replay_bytes,8);
                const D3D12_DRAW_ARGUMENTS expected_arguments{count,1,first,0};
                memcpy(replay_expected.data()+264,&expected_arguments,sizeof(expected_arguments));
                D3D12_RANGE replay_all{0,328};Check(result.handle->Map(0,&replay_all,&mapped));
                const bool replay_equal=!memcmp(mapped,replay_expected.data(),replay_expected.size());
                result.handle->Unmap(0,&none);
                if(!replay_equal)throw std::runtime_error("GPU primitive count/indirect slice/empty draw/order/lifetime/guard mismatch");
            }
        }
    }
    {
        // Actual production serialized writer. Two triangles, two instances,
        // exact patch/vertex/word order, float payload bits and padded records.
        state.pipeline.StreamOutput=serialized_layout.Desc();
        const D3D12_VIEWPORT viewport{0,0,1,1,0,1};const D3D12_RECT scissor{0,0,1,1};
        const float blend[]{1,1,1,1};
        ShaderPipeline::GraphicsShaders shaders;shaders.vertex=serialized_vertex.bytecode;shaders.geometry=serialized.bytecode;
        ShaderPipeline pipeline(device.Get(),dxc,shaders,state.pipeline);ResourceBindings bindings(device.Get(),pipeline.Layout());
        constexpr u32 stream_bytes=12*32,guarded_bytes=16+stream_bytes+16,total=guarded_bytes+8;
        auto output=Buffer(device.Get(),guarded_bytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
        auto counter=Buffer(device.Get(),8,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
        auto upload=Buffer(device.Get(),total,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        auto result=Buffer(device.Get(),total,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        void* mapped{};D3D12_RANGE none{};Check(upload.handle->Map(0,&none,&mapped));
        memset(mapped,0xcd,total);const u32 initial_counter[]{0,0xa5a5a5a5};
        memcpy(static_cast<std::byte*>(mapped)+guarded_bytes,initial_counter,8);
        D3D12_RANGE written{0,total};upload.handle->Unmap(0,&written);
        auto* list=renderer.RecordingList();list->CopyBufferRegion(output.handle.Get(),0,upload.handle.Get(),0,guarded_bytes);
        list->CopyBufferRegion(counter.handle.Get(),0,upload.handle.Get(),guarded_bytes,8);
        output.Transition(list,D3D12_RESOURCE_STATE_STREAM_OUT);counter.Transition(list,D3D12_RESOURCE_STATE_STREAM_OUT);
        const D3D12_STREAM_OUTPUT_BUFFER_VIEW view{output.handle->GetGPUVirtualAddress()+16,stream_bytes,counter.handle->GetGPUVirtualAddress()};
        list->SOSetTargets(0,1,&view);
        renderer.ExecuteGraphics(pipeline,bindings,state,{},nullptr,std::span(&viewport,1),std::span(&scissor,1),blend,{6,2},{},nullptr,false);
        list=renderer.RecordingList();list->SOSetTargets(0,0,nullptr);
        output.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);counter.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
        list->CopyBufferRegion(result.handle.Get(),0,output.handle.Get(),0,guarded_bytes);
        list->CopyBufferRegion(result.handle.Get(),guarded_bytes,counter.handle.Get(),0,8);renderer.Finish();
        std::array<std::byte,total> expected;expected.fill(std::byte{0xcd});
        for(u32 instance=0;instance<2;++instance)for(u32 primitive=0;primitive<2;++primitive)for(u32 vertex=0;vertex<3;++vertex){
            const u32 words[]{primitive,primitive*3+vertex,instance,0x7fc12345u,0x80000000u,0,0,0};
            const u32 record=(instance*2+primitive)*3+vertex;memcpy(expected.data()+16+record*32,words,32);
        }
        const u32 final_counter[]{stream_bytes,0xa5a5a5a5};memcpy(expected.data()+guarded_bytes,final_counter,8);
        D3D12_RANGE all{0,total};Check(result.handle->Map(0,&all,&mapped));const bool equal=!memcmp(mapped,expected.data(),total);
        result.handle->Unmap(0,&none);
        if(!equal)throw std::runtime_error("Serialized primitive writer lost patch/vertex/word order, exact bits, padding or32-bit counter guards");
    }
    for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T bytes{};Check(messages->GetMessage(i,nullptr,&bytes));
        std::vector<std::byte> data(bytes);auto* message=reinterpret_cast<D3D12_MESSAGE*>(data.data());
        Check(messages->GetMessage(i,message,&bytes));
        if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);
    }
    puts("PASS: internal primitive stream full/sliced/gap records, GPU count/indirect slices/empty replay, lifetime and exact guards");return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
