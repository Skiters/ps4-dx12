// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/shader_pipeline.h"
#include "video_core/renderer_d3d12/guest_graphics_state.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include "shader_recompiler/backend/dxil/dxil_compiler.h"
#include "core/emulator_settings.h"
#include "stream_output_layout_checks.h"
#include "guest_indirect_argument_checks.h"
#include "guest_index_expand_checks.h"
#include "draw_sysval_checks.h"
#include "guest_indirect_pack_checks.h"
#include "video_core/renderer_d3d12/primitive_stream_arguments.h"
#include "video_core/renderer_d3d12/guest_point_shader.h"
#include "video_core/renderer_d3d12/guest_point_capture.h"
#include "shader_recompiler/backend/dxil/spirv_converter.h"
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <cstdio>
#include <cstring>
#include <fstream>

using namespace NativeDX12;
namespace DXIL=Shader::Backend::DX12;
int main(int argc,char** argv){try{
    if(argc!=4)throw std::invalid_argument("Expected converter, DXC and primitive stream mode");
    Tests::CheckStreamOutputLayout();
    Tests::CheckGuestIndirectArguments();
    Tests::CheckGuestIndexExpansionPlan();
    puts("CHECKED: guest indirect indexed/nonindexed ABI, immutable snapshot, stride/address and GPU pack size limits (CPU)");
    GuestGraphicsState patch_state;patch_state.topology=D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
    patch_state.pipeline.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
    patch_state.pipeline.IBStripCutValue=D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF;
    for(const u32 width:std::array<u32,5>{1,2,3,4,6}){
        const auto consumer=NativePointCapturePass::StreamConsumerState(patch_state,width);
        if(NativePointCapturePass::PrimitiveBound(consumer.topology,width*2)!=2||
           consumer.pipeline.PrimitiveTopologyType==D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH||
           consumer.pipeline.IBStripCutValue!=D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED||!consumer.rasterizer_discard)
            throw std::runtime_error("Buffered GS state/primitive-bound contract changed the original invocation count");
    }
    bool rejected_width{};try{(void)NativePointCapturePass::StreamConsumerState(patch_state,5);}
    catch(const std::invalid_argument&){rejected_width=true;}
    if(!rejected_width)throw std::runtime_error("Invalid buffered GS primitive width accepted");
    EmulatorSettings.SetPipelineCacheEnabled(false);_putenv_s("SHADPS4_NATIVE_SHADER_CACHE","off");
    const auto dxc=std::filesystem::path(argv[2]).wstring();
    DXIL::SpirvConvertOptions sysval_options;sysval_options.stage=DXIL::SpirvStage::Vertex;
    const auto sysval_vertex=DXIL::ConvertSpirv(std::filesystem::path(argv[1]).wstring(),Tests::DrawSysvalVertex(),sysval_options);
    if(!sysval_vertex.success)throw std::runtime_error(sysval_vertex.diagnostics);
    if(std::string_view(argv[3])=="compile-sysvals"){
        std::ofstream file("draw-sysvals-vertex.dxil",std::ios::binary);
        file.write(reinterpret_cast<const char*>(sysval_vertex.bytecode.data()),sysval_vertex.bytecode.size());
        if(!file)throw std::runtime_error("Cannot save draw sysval DXIL");
        puts("PASS: draw sysval SPIR-V converted to signed DXIL; GPU NOT_RUN");return 0;
    }
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
    const auto batch_vertex=DXIL::CompileDxil(dxc,R"(
cbuffer Runtime:register(b0,space31){uint tag;};
struct V {float4 position:SV_Position;uint4 payload:CAPTURE0;};
V main(uint vertex:TEXCOORD0,uint instance:TEXCOORD1){
    V o;o.position=float4(0,0,0,1);o.payload=uint4(vertex,instance,0x7fc12345u^tag,0x80000000u);return o;
})",DXIL::Stage::Vertex);
    if(!batch_vertex.success)throw std::runtime_error(batch_vertex.diagnostics);
    const std::array<std::span<const u8>,1> runtime_code{batch_vertex.bytecode},plain_code{vertex.bytecode};
    const auto reflected=ResourceLayout::ReflectBindings(dxc,runtime_code);
    if(ResourceLayout::ClassifyGraphicsRuntime(reflected)!=GraphicsRuntimeBinding::Root||
       reflected.size()!=1||reflected[0].type!=D3D12_DESCRIPTOR_RANGE_TYPE_CBV||
       reflected[0].reg||reflected[0].space!=31||reflected[0].count!=1||
       !ResourceLayout::ReflectBindings(dxc,plain_code).empty())
        throw std::runtime_error("CPU indirect admission reflection lost the runtime CBV or invented a binding");
    const auto absent=ResourceLayout::ReflectBindings(dxc,plain_code);
    const ShaderBinding array_runtime{D3D12_DESCRIPTOR_RANGE_TYPE_CBV,0,31,2};
    if(ResourceLayout::ClassifyGraphicsRuntime(absent)!=GraphicsRuntimeBinding::Absent||
       ResourceLayout::ClassifyGraphicsRuntime(std::span(&array_runtime,1))!=GraphicsRuntimeBinding::Unsupported)
        throw std::runtime_error("Runtime-free shader admission confused an unsupported CBV array with an absent runtime");
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
    if(std::string_view(argv[3])=="draw-sysvals"){
        Tests::CheckDrawSysvalsGpu(device.Get(),renderer,dxc,sysval_vertex.bytecode);
        renderer.ValidateDebugMessages();puts("PASS: Vulkan direct/expanded draw sysvals");return 0;
    }
    Tests::CheckGuestIndexExpansionGpu(device.Get(),renderer);
    if(std::string_view(argv[3])=="large-grid"){
        Tests::CheckGuestIndirectLargeGridGpu(device.Get(),renderer);renderer.ValidateDebugMessages();
        puts("PASS: GPU indirect pack second dispatch row and exact guards; no runtime arena");return 0;
    }
    Tests::CheckGuestIndirectPackGpu(device.Get(),renderer);
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
        const Rasterizer::StreamOutputBinding target{&output,&counter,16,UINT64(12)*stride,0};
        bindings.BeforeBind([&](ResourceBindings&){renderer.Finish();});
        const D3D12_VIEWPORT viewport{0,0,1,1,0,1};const D3D12_RECT scissor{0,0,1,1};const float blend[4]{};
        renderer.ExecuteGraphics(pipeline,bindings,state,{},nullptr,std::span(&viewport,1),std::span(&scissor,1),blend,{6,2},{},nullptr,true,nullptr,&target);
        list=renderer.RecordingList();
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
                const Rasterizer::StreamOutputBinding target{&destination,&destination_counter,16,192,0};
                const Rasterizer::StreamDrawArguments indirect{&arguments,0};
                // CPU counts deliberately zero: only GPU-produced arguments
                // decide the draw. This is not a CPU filled-size approximation.
                renderer.ExecuteGraphics(reader_pipeline,reader_bindings,state,{},nullptr,
                    std::span(&viewport,1),std::span(&scissor,1),blend,{0,0},{},nullptr,false,&indirect,&target);
                list=renderer.RecordingList();
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
        // Native multi-command DRAW/INDEXED and optional GPU count. Capture
        // VS payload directly so the expected index/base/instance values are
        // independent of guest translation and GS primitive-ID conventions.
        D3D12_SO_DECLARATION_ENTRY declaration{0,"CAPTURE",0,0,4,0};UINT stride=16;
        state.pipeline.StreamOutput={&declaration,1,&stride,1,D3D12_SO_NO_RASTERIZED_STREAM};
        // IA values observe API base offsets directly. Bare SV_InstanceID
        // excludes StartInstanceLocation (HLSL extended-command-info spec).
        const D3D12_INPUT_ELEMENT_DESC elements[]{
            {"TEXCOORD",0,DXGI_FORMAT_R32_UINT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
            {"TEXCOORD",1,DXGI_FORMAT_R32_UINT,1,0,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1}};
        state.pipeline.InputLayout={elements,2};
        for(const bool root_runtime:std::array{false,true}){
        ShaderPipeline::GraphicsShaders shaders;shaders.vertex=batch_vertex.bytecode;
        shaders.root_graphics_runtime=root_runtime;
        ShaderPipeline pipeline(device.Get(),dxc,shaders,state.pipeline);
        const std::array<std::span<const u8>,1> bytecodes{batch_vertex.bytecode};
        ResourceLayout source_layout(device.Get(),dxc,bytecodes,!root_runtime);
        ResourceBindings source(device.Get(),source_layout,nullptr,true);
        auto fallback=Buffer(device.Get(),256,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        void* fallback_data{};D3D12_RANGE empty{};Check(fallback.handle->Map(0,&empty,&fallback_data));
        memset(fallback_data,0,256);fallback.handle->Unmap(0,nullptr);
        source.Cbv(0,31,fallback.handle.Get(),0,256);
        auto projected=source.ForkForReplay(pipeline.Layout());auto& bindings=*projected;
        auto runtime_seed=Buffer(device.Get(),768,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        Check(runtime_seed.handle->Map(0,&empty,&fallback_data));memset(fallback_data,0,768);
        for(u32 command=0;command<3;++command)static_cast<u32*>(fallback_data)[command*64]=101+command;
        runtime_seed.handle->Unmap(0,nullptr);
        const D3D12_VIEWPORT viewport{0,0,1,1,0,1};const D3D12_RECT scissor{0,0,1,1};const float blend[]{1,1,1,1};
        const std::array<u32,10> indices{99,9,7,8,11,10,12,17,18,19};
        auto index=Buffer(device.Get(),sizeof(indices),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        void* mapped{};D3D12_RANGE none{};Check(index.handle->Map(0,&none,&mapped));
        memcpy(mapped,indices.data(),sizeof(indices));D3D12_RANGE index_written{0,sizeof(indices)};index.handle->Unmap(0,&index_written);
        const Rasterizer::IndexBinding index_view{&index,0,sizeof(indices),DXGI_FORMAT_R32_UINT};
        auto vertex_data=Buffer(device.Get(),128,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        auto instance_data=Buffer(device.Get(),128,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        for(auto [resource,base]:std::array<std::pair<Resource*,u32>,2>{{{&vertex_data,100},{&instance_data,200}}}){
            Check(resource->handle->Map(0,&none,&mapped));
            for(u32 i=0;i<32;++i)static_cast<u32*>(mapped)[i]=base+i;
            D3D12_RANGE written{0,128};resource->handle->Unmap(0,&written);
        }
        const std::array<Rasterizer::VertexBinding,2> vertices{{{&vertex_data,0,128,4},{&instance_data,0,128,4}}};
        for(const bool indexed:std::array{false,true})for(const u32 spacing:std::array<u32,2>{0,32})
        for(const u32 active:std::array<u32,6>{0,1,2,3,99,UINT32_MAX}){
            const auto layout=NativeIndirectLayout::Make(indexed,root_runtime?8:4,spacing,3,root_runtime);
            const u32 command_stride=layout.stride,first=u32(layout.offset);
            constexpr u32 guarded=272;
            const u32 packet_bytes=first+3*command_stride;
            auto upload=Buffer(device.Get(),guarded+8+packet_bytes+4,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
            Check(upload.handle->Map(0,&none,&mapped));memset(mapped,0xcd,size_t(upload.handle->GetDesc().Width));
            const u64 zero=0;memcpy(static_cast<std::byte*>(mapped)+guarded,&zero,8);
            for(u32 command=0;command<3;++command){
                const std::array<u32,5> words{3,command?1u:2u,indexed?1+command*3:5+command*6,
                    indexed?std::bit_cast<u32>(command==0?-4:command==1?2:-1):7+command*2,
                    7+command*2};
                memcpy(static_cast<std::byte*>(mapped)+guarded+8+first+command*command_stride+(root_runtime?8:0),words.data(),indexed?20:16);
            }
            memcpy(static_cast<std::byte*>(mapped)+guarded+8+packet_bytes,&active,4);
            upload.handle->Unmap(0,nullptr);
            auto output=Buffer(device.Get(),guarded,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
            auto filled=Buffer(device.Get(),8,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
            auto result=Buffer(device.Get(),guarded+8,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            auto* commands=renderer.RecordingList();
            commands->CopyBufferRegion(output.handle.Get(),0,upload.handle.Get(),0,guarded);
            commands->CopyBufferRegion(filled.handle.Get(),0,upload.handle.Get(),guarded,8);
            {
                auto arguments=Buffer(device.Get(),packet_bytes+4,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
                auto count=Buffer(device.Get(),4,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
                auto constants=Buffer(device.Get(),768,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
                if(root_runtime){
                    commands->CopyBufferRegion(constants.handle.Get(),0,runtime_seed.handle.Get(),0,768);
                    // These are native GPU addresses, not CPU draw constants.
                    Check(upload.handle->Map(0,&none,&mapped));
                    for(u32 command=0;command<3;++command){
                        const u64 address=constants.handle->GetGPUVirtualAddress()+command*256;
                        memcpy(static_cast<std::byte*>(mapped)+guarded+8+first+command*command_stride,&address,8);
                    }
                    upload.handle->Unmap(0,nullptr);
                }
                commands->CopyBufferRegion(arguments.handle.Get(),0,upload.handle.Get(),guarded+8,packet_bytes+4);
                commands->CopyBufferRegion(count.handle.Get(),0,upload.handle.Get(),guarded+8+packet_bytes,4);
                const Rasterizer::StreamDrawArguments batch{&arguments,first,3,spacing,
                    active==UINT32_MAX?nullptr:spacing?&arguments:&count,active==UINT32_MAX?0u:spacing?packet_bytes:0u,
                    root_runtime?&constants:nullptr};
                const Rasterizer::StreamOutputBinding target{&output,&filled,16,240,0};
                const auto submit=[&](const Rasterizer::StreamDrawArguments& candidate){
                    renderer.ExecuteGraphics(pipeline,bindings,state,{},nullptr,std::span(&viewport,1),std::span(&scissor,1),
                        blend,{0,0},vertices,indexed?&index_view:nullptr,false,&candidate,&target);
                };
                if(!active){
                    auto bad=batch;bad.max_count=UINT32_MAX;bool rejected{};
                    try{submit(bad);}catch(const std::out_of_range&){rejected=true;}
                    if(!rejected)throw std::runtime_error("Native indirect batch accepted an out-of-bounds maximum count");
                    bad=batch;bad.count_offset=2;rejected=false;
                    try{submit(bad);}catch(const std::out_of_range&){rejected=true;}
                    if(!rejected)throw std::runtime_error("Native indirect batch accepted an unaligned GPU count");
                    bad=batch;bad.count_resource=&filled;bad.count_offset=0;rejected=false;
                    try{submit(bad);}catch(const std::invalid_argument&){rejected=true;}
                    if(!rejected)throw std::runtime_error("Native indirect count overlapped stream-output writes");
                }
                submit(batch);
                if(root_runtime)renderer.ExecuteGraphics(pipeline,bindings,state,{},nullptr,std::span(&viewport,1),std::span(&scissor,1),
                    blend,{3,1},vertices,nullptr,false,nullptr,&target);
            } // Native argument/count allocations must survive until Finish.
            commands=renderer.RecordingList();output.Transition(commands,D3D12_RESOURCE_STATE_COPY_SOURCE);
            filled.Transition(commands,D3D12_RESOURCE_STATE_COPY_SOURCE);
            commands->CopyBufferRegion(result.handle.Get(),0,output.handle.Get(),0,guarded);
            commands->CopyBufferRegion(result.handle.Get(),guarded,filled.handle.Get(),0,8);
            renderer.Finish();
            std::array<std::byte,guarded+8> expected;expected.fill(std::byte{0xcd});u32 records{};
            const u32 indexed_ids[3][3]{{5,3,4},{13,12,14},{16,17,18}};
            for(u32 command=0;command<std::min(active,3u);++command)
            for(u32 instance=0;instance<(command?1u:2u);++instance)for(u32 corner=0;corner<3;++corner){
                const std::array<u32,4> payload{100+(indexed?indexed_ids[command][corner]:5+command*6+corner),
                    200+7+command*2+instance,0x7fc12345u^(root_runtime?101+command:0),0x80000000u};
                memcpy(expected.data()+16+records++*16,payload.data(),16);
            }
            if(root_runtime)for(u32 corner=0;corner<3;++corner){
                const std::array<u32,4> payload{100+corner,200,0x7fc12345u,0x80000000u};
                memcpy(expected.data()+16+records++*16,payload.data(),16);
            }
            const u64 bytes=u64(records)*16;memcpy(expected.data()+guarded,&bytes,8);
            D3D12_RANGE all{0,expected.size()};Check(result.handle->Map(0,&all,&mapped));
            const bool equal=!memcmp(mapped,expected.data(),expected.size());result.handle->Unmap(0,&none);
            if(!equal)throw std::runtime_error("Native indexed/nonindexed batch count/stride/base/instance/order/lifetime/guard mismatch");
        }
        }
        state.pipeline.InputLayout={};
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
        const Rasterizer::StreamOutputBinding target{&output,&counter,16,stream_bytes,0};
        renderer.ExecuteGraphics(pipeline,bindings,state,{},nullptr,std::span(&viewport,1),std::span(&scissor,1),blend,{6,2},{},nullptr,false,nullptr,&target);
        list=renderer.RecordingList();
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
        std::shared_ptr<PrimitiveStreamLease> captured;
        {
            PrimitiveStreamPass pass(device.Get());
            ResourceBindings capture_bindings(device.Get(),pipeline.Layout());
            capture_bindings.BeforeBind([&](ResourceBindings&){renderer.Finish();});
            captured=pass.Capture(renderer,32,18,3,[&](PrimitiveStreamLease& lease){
                const Rasterizer::StreamOutputBinding target{&lease.data,&lease.filled_size,0,u64(lease.range.stride)*lease.range.capacity_vertices,0};
                renderer.ExecuteGraphics(pipeline,capture_bindings,state,{},nullptr,std::span(&viewport,1),
                    std::span(&scissor,1),blend,{6,2},{},nullptr,false,nullptr,&target);
            });
        } // Production pass and its root/PSO/upload owner die before completion.
        constexpr u32 captured_total=stream_bytes+4+sizeof(D3D12_DRAW_ARGUMENTS);
        auto captured_result=Buffer(device.Get(),captured_total,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        list=renderer.RecordingList();
        captured->data.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
        captured->filled_size.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
        captured->arguments.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
        list->CopyBufferRegion(captured_result.handle.Get(),0,captured->data.handle.Get(),0,stream_bytes);
        list->CopyBufferRegion(captured_result.handle.Get(),stream_bytes,captured->filled_size.handle.Get(),0,4);
        list->CopyBufferRegion(captured_result.handle.Get(),stream_bytes+4,captured->arguments.handle.Get(),0,sizeof(D3D12_DRAW_ARGUMENTS));
        renderer.Finish();D3D12_RANGE captured_all{0,captured_total};Check(captured_result.handle->Map(0,&captured_all,&mapped));
        std::array<std::byte,captured_total> captured_expected{};
        memcpy(captured_expected.data(),expected.data()+16,stream_bytes);
        memcpy(captured_expected.data()+stream_bytes,&stream_bytes,4);
        const D3D12_DRAW_ARGUMENTS draw{12,1,0,0};
        memcpy(captured_expected.data()+stream_bytes+4,&draw,sizeof(draw));
        const bool captured_equal=!memcmp(mapped,captured_expected.data(),captured_total);captured_result.handle->Unmap(0,&none);
        if(!captured_equal)throw std::runtime_error("Production stream capture lost contents/count/arguments or lifetime across ownership Finish");
        auto partition_result=Buffer(device.Get(),6*sizeof(D3D12_DRAW_ARGUMENTS),D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        u32 partition_index{};
        {
            PrimitiveStreamPass replay(device.Get());
            replay.Replay(renderer,captured,2,256,622592,[&](PrimitiveStreamLease& lease,const PrimitiveStreamPartition& part){
                if(part.input.first!=partition_index*6||part.input.limit!=6||part.first!=(partition_index==0)||part.last!=(partition_index==2))
                    throw std::runtime_error("Production replay skipped or reordered a primitive partition");
                auto* commands=renderer.RecordingList();lease.arguments.Transition(commands,D3D12_RESOURCE_STATE_COPY_SOURCE);
                commands->CopyBufferRegion(partition_result.handle.Get(),partition_index*sizeof(D3D12_DRAW_ARGUMENTS),
                    lease.arguments.handle.Get(),0,sizeof(D3D12_DRAW_ARGUMENTS));
                commands->CopyBufferRegion(partition_result.handle.Get(),(3+partition_index)*sizeof(D3D12_DRAW_ARGUMENTS),
                    lease.arguments.handle.Get(),sizeof(D3D12_DRAW_ARGUMENTS),sizeof(D3D12_DRAW_ARGUMENTS));++partition_index;
            });
        }
        renderer.Finish();
        if(partition_index!=3)throw std::runtime_error("Production replay did not cover its complete reserved stream");
        const D3D12_DRAW_ARGUMENTS partition_expected[]{{6,1,0,0},{6,1,6,0},{0,1,12,0},
            {1024,1,0,0},{1024,1,0,0},{0,1,0,0}};
        D3D12_RANGE partition_all{0,sizeof(partition_expected)};Check(partition_result.handle->Map(0,&partition_all,&mapped));
        const bool partition_equal=!memcmp(mapped,partition_expected,sizeof(partition_expected));partition_result.handle->Unmap(0,&none);
        if(!partition_equal)throw std::runtime_error("Partition replay replaced the GPU count with capacity or lost the empty tail/lifetime");
    }
    // Fused packets retain the old independent oracle, including incomplete
    // primitives and an oversized hardware count; trailing bytes are guards.
    for(const auto sample:std::array<std::pair<u32,u32>,6>{{{0,0},{6,0},{7,1},{9,3},{12,6},{99,6}}}){
        auto seed=Buffer(device.Get(),68,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        auto filled=renderer.AcquireTransientBuffer(4),packets=renderer.AcquireTransientBuffer(64);
        auto readback=Buffer(device.Get(),64,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        void* mapped{};D3D12_RANGE none{};Check(seed.handle->Map(0,&none,&mapped));
        memset(mapped,0xcd,68);const u32 bytes=sample.first*16;memcpy(mapped,&bytes,4);seed.handle->Unmap(0,nullptr);
        auto* list=renderer.RecordingList();filled.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
        packets.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
        list->CopyBufferRegion(filled.handle.Get(),0,seed.handle.Get(),0,4);
        list->CopyBufferRegion(packets.handle.Get(),0,seed.handle.Get(),4,64);
        const PrimitiveStreamPartition part{PrimitiveStreamRange::Make(16,18,3,6,6),
            Shader::NativePointCapture::Plan::Make(2,2,256,1),false,false};
        {
            PrimitiveStreamArguments generate(device.Get());
            auto bad=part;bad.capture.primitive_bound=1;
            bool rejected{};try{generate.RecordPartition(renderer,filled,packets,bad);}catch(const std::invalid_argument&){rejected=true;}
            if(!rejected)throw std::runtime_error("Fused partition accepted inconsistent capture bounds");
            generate.RecordPartition(renderer,filled,packets,part);
        } // The fused PSO and resources must survive their owner.
        packets.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
        list->CopyBufferRegion(readback.handle.Get(),0,packets.handle.Get(),0,64);renderer.Finish();
        std::array<std::byte,64> expected;expected.fill(std::byte{0xcd});
        const D3D12_DRAW_ARGUMENTS draws[]{{sample.second,1,6,0},{(sample.second/3)*512,1,0,0}};
        const D3D12_DISPATCH_ARGUMENTS dispatch{(sample.second/3)*2,1,1};
        memcpy(expected.data(),draws,sizeof(draws));memcpy(expected.data()+32,&dispatch,sizeof(dispatch));
        D3D12_RANGE all{0,64};Check(readback.handle->Map(0,&all,&mapped));
        const bool equal=!memcmp(mapped,expected.data(),64);readback.handle->Unmap(0,&none);
        if(!equal)throw std::runtime_error("Fused partition lost count/first vertex, primitive rounding, lifetime or guards");
    }
    // Clear-only GPU regression: empty/partial indirect replay must leave
    // unused records, payload words and the following record unchanged.
    for(const bool prepared:std::array{false,true})
    for(const u32 active:std::array<u32,9>{0,1,2,255,256,257,258,259,UINT32_MAX}){
        constexpr u32 capacity=258;
        constexpr UINT bytes=UINT(Shader::NativePointCapture::RecordBytes*(capacity+1));
        auto seed=Buffer(device.Get(),bytes+32,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        void* data{};D3D12_RANGE no_read{};Check(seed.handle->Map(0,&no_read,&data));
        memset(data,0x6d,bytes+32);const D3D12_DRAW_ARGUMENTS count{active,1,0,0};
        memcpy(static_cast<std::byte*>(data)+bytes,&count,sizeof(count));
        const D3D12_DISPATCH_ARGUMENTS dispatch{(std::min(active,capacity)+255)/256,1,1};
        memcpy(static_cast<std::byte*>(data)+bytes+16,&dispatch,sizeof(dispatch));
        D3D12_RANGE written{0,bytes+32};seed.handle->Unmap(0,&written);
        auto counter=renderer.AcquireTransientBuffer(32);
        auto result=Buffer(device.Get(),bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        ComPtr<ID3D12QueryHeap> query;
        const D3D12_QUERY_HEAP_DESC query_desc{D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS,1,0};
        Check(device->CreateQueryHeap(&query_desc,IID_PPV_ARGS(&query)));
        auto statistics=Buffer(device.Get(),sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS),
            D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        std::shared_ptr<Resource> arena;
        {
            NativePointCapturePass clear(device.Get());arena=clear.Reserve(renderer,bytes);
            auto* commands=renderer.RecordingList();arena->Transition(commands,D3D12_RESOURCE_STATE_COPY_DEST);
            counter.Transition(commands,D3D12_RESOURCE_STATE_COPY_DEST);
            commands->CopyBufferRegion(arena->handle.Get(),0,seed.handle.Get(),0,bytes);
            commands->CopyBufferRegion(counter.handle.Get(),0,seed.handle.Get(),bytes,32);
            const Rasterizer::StreamDrawArguments actual{&counter,0};
            const NativePointCapturePass::IndirectClear clear_dispatch{&counter,16};
            if(prepared){
                auto alias=counter;
                const NativePointCapturePass::IndirectClear invalids[]{{&counter,24},{&alias,16}};
                for(const auto& invalid:invalids){
                    bool rejected{};
                    try{clear.Execute(renderer,Shader::NativePointCapture::Plan::Make(capacity,1,1,1),{capacity,1},D3D_PRIMITIVE_TOPOLOGY_POINTLIST,
                        [](Resource&,const Rasterizer::DrawArguments&,u64,u64){},
                        [](Resource&,const Rasterizer::DrawArguments&){},arena,&actual,&invalid);}
                    catch(const std::invalid_argument&){rejected=true;}
                    if(!rejected)throw std::runtime_error("Prepared clear accepted truncated packet or duplicate authoritative state");
                }
            }
            commands->BeginQuery(query.Get(),D3D12_QUERY_TYPE_PIPELINE_STATISTICS,0);
            for(u32 repeat=0;repeat<2;++repeat)
            clear.Execute(renderer,Shader::NativePointCapture::Plan::Make(capacity,1,1,1),{capacity,1},D3D_PRIMITIVE_TOPOLOGY_POINTLIST,
                [](Resource&,const Rasterizer::DrawArguments&,u64,u64){},
                [](Resource&,const Rasterizer::DrawArguments&){},arena,&actual,prepared?&clear_dispatch:nullptr);
        } // Queued clear root/PSO survives owner destruction before submission.
        renderer.RecordingList()->EndQuery(query.Get(),D3D12_QUERY_TYPE_PIPELINE_STATISTICS,0);
        renderer.RecordingList()->ResolveQueryData(query.Get(),D3D12_QUERY_TYPE_PIPELINE_STATISTICS,0,1,statistics.handle.Get(),0);
        arena->Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        renderer.RecordingList()->CopyBufferRegion(result.handle.Get(),0,arena->handle.Get(),0,bytes);
        renderer.Finish();D3D12_RANGE all{0,bytes};Check(result.handle->Map(0,&all,&data));
        std::array<std::byte,bytes> expected;expected.fill(std::byte{0x6d});
        for(u32 record=0;record<std::min(active,capacity);++record)memset(expected.data()+record*Shader::NativePointCapture::RecordBytes,0,4);
        const bool equal=!memcmp(data,expected.data(),bytes);result.handle->Unmap(0,&no_read);
        if(!equal)throw std::runtime_error("GPU indirect capture clear changed inactive records, payload or guards");
        D3D12_RANGE stats_all{0,sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS)};
        Check(statistics.handle->Map(0,&stats_all,&data));
        const auto invocations=static_cast<const D3D12_QUERY_DATA_PIPELINE_STATISTICS*>(data)->CSInvocations;
        statistics.handle->Unmap(0,&no_read);
        // Two one-thread argument builders plus exactly the active clear
        // groups. A capacity-sized dispatch would fail empty/partial cases.
        const u64 expected_invocations=2*((prepared?0:1)+u64((std::min(active,capacity)+255)/256)*256);
        if(invocations!=expected_invocations)
            throw std::runtime_error("Capture clear dispatched capacity instead of the actual GPU prefix");
    }
    for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T bytes{};Check(messages->GetMessage(i,nullptr,&bytes));
        std::vector<std::byte> data(bytes);auto* message=reinterpret_cast<D3D12_MESSAGE*>(data.data());
        Check(messages->GetMessage(i,message,&bytes));
        if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);
    }
    puts("PASS: internal primitive stream full/sliced/gap records, GPU count/indirect slices/empty replay, lifetime and exact guards");return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
