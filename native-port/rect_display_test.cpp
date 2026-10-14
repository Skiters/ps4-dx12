// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/backend_factory.h"
#include "video_core/renderer_d3d12/guest_compute.h"
#include "video_core/renderer_d3d12/guest_graphics_state.h"
#include "video_core/renderer_d3d12/texture_resources.h"
#include "shader_recompiler/backend/spirv/emit_spirv_quad_rect.h"
#include "shader_recompiler/backend/dxil/dxil_compiler.h"
#include "shader_recompiler/backend/dxil/spirv_converter.h"
#include "shader_recompiler/runtime_info.h"
#include <sirit/sirit.h>
#include <cstdio>
#include <fstream>
using namespace NativeDX12;
static std::vector<u32> Vertex(){
    Sirit::Module m(0x00010500);m.AddCapability(spv::Capability::Shader);
    const auto f=m.TypeFloat(32),u=m.TypeUInt(32),v=m.TypeVector(f,4),b=m.TypeBool();
    const auto input=m.AddGlobalVariable(m.TypePointer(spv::StorageClass::Input,u),spv::StorageClass::Input);
    m.Decorate(input,spv::Decoration::BuiltIn,spv::BuiltIn::VertexIndex);
    const auto output=m.AddGlobalVariable(m.TypePointer(spv::StorageClass::Output,v),spv::StorageClass::Output);
    m.Decorate(output,spv::Decoration::BuiltIn,spv::BuiltIn::Position);
    const auto main=m.OpFunction(m.TypeVoid(),spv::FunctionControlMask::MaskNone,m.TypeFunction(m.TypeVoid()));
    const std::array interfaces{input,output};m.AddEntryPoint(spv::ExecutionModel::Vertex,main,"main",interfaces);
    m.AddLabel(m.OpLabel());const auto id=m.OpLoad(u,input);
    const auto x=m.OpSelect(f,m.OpIEqual(b,id,m.Constant(u,1u)),m.Constant(f,1.f),m.Constant(f,-1.f));
    const auto y=m.OpSelect(f,m.OpIEqual(b,id,m.Constant(u,2u)),m.Constant(f,1.f),m.Constant(f,-1.f));
    const std::array components{x,y,m.Constant(f,0.5f),m.Constant(f,1.f)};
    m.OpStore(output,m.OpCompositeConstruct(v,components));m.OpReturn();m.OpFunctionEnd();return m.Assemble();
}
int wmain(int argc,wchar_t** argv){try{
    if(argc!=3)throw std::invalid_argument("Expected converter and DXC");
    _putenv_s("SHADPS4_NATIVE_D3D12_VALIDATION","1");auto device=CreateHardwareDevice();
    const auto vs=Vertex();const Shader::HwFragmentRuntimeInfo fragment{};
    const auto hs=Shader::Backend::SPIRV::EmitAuxilaryTessShader(Shader::Backend::SPIRV::AuxShaderType::RectListTCS,fragment,3);
    const auto ds=Shader::Backend::SPIRV::EmitAuxilaryTessShader(Shader::Backend::SPIRV::AuxShaderType::PassthroughTES,fragment,4);
    using namespace Shader::Backend::DX12;
    const std::array<LinkedSpirvStage,3> stages{{{SpirvStage::Vertex,vs},{SpirvStage::TessellationControl,hs},{SpirvStage::TessellationEvaluation,ds}}};
    SpirvConvertOptions options;options.runtime_viewport_flips=true;
    const auto linked=ConvertLinkedStages(argv[1],stages,options);
    if(!linked.success)throw std::runtime_error(linked.diagnostics);
    for(size_t i=0;i<linked.stages.size();++i){std::ofstream file("rect-stage-"+std::to_string(i)+".dxil",std::ios::binary);
        file.write(reinterpret_cast<const char*>(linked.stages[i].bytecode.data()),linked.stages[i].bytecode.size());}
    // Use exact UNORM endpoints: half values can legally round differently
    // between adapters and do not help detect lost homogeneous coordinates.
    const auto pixel=CompileDxil(argv[2],"float4 main():SV_Target0{return float4(1,0,0,1);}",Stage::Pixel);
    if(!pixel.success)throw std::runtime_error(pixel.diagnostics);
    for(u32 flip:{0u,1u}){
        Rasterizer renderer(device.Get());auto texture=Texture2D(device.Get(),8,8,DXGI_FORMAT_R8G8B8A8_UNORM,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_COMMON);
        const DXGI_FORMAT formats[]{DXGI_FORMAT_R8G8B8A8_UNORM};AmdGpu::Regs regs{};
        regs.primitive_type=AmdGpu::PrimitiveType::RectList;regs.color_target_mask.raw=regs.color_shader_mask.raw=15;
        regs.color_control.mode=AmdGpu::ColorControl::OperationMode::Normal;regs.color_control.rop3=AmdGpu::ColorControl::LogicOp::Copy;
        const auto state=BuildGuestGraphicsState(regs,formats);
        ShaderPipeline pipeline(device.Get(),argv[2],{linked.stages[0].bytecode,pixel.bytecode,linked.stages[1].bytecode,linked.stages[2].bytecode,{}},state.pipeline);
        ResourceBindings bindings(device.Get(),pipeline.Layout());GraphicsRuntime runtime{};runtime.yz_flip_mask=flip;
        auto constants=Buffer(device.Get(),1024,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        void* mapped{};Check(constants.handle->Map(0,nullptr,&mapped));memcpy(mapped,&runtime,sizeof(runtime));constants.handle->Unmap(0,nullptr);
        for(const auto& binding:pipeline.Layout().Bindings())if(binding.space==31)bindings.Cbv(binding.reg,binding.space,constants.handle.Get(),0,1024);
        const Rasterizer::ColorAttachment target{&texture};const D3D12_VIEWPORT viewport{0,0,8,8,0,1};const D3D12_RECT scissor{0,0,8,8};
        const std::array<float,4> blend{};
        ComPtr<ID3D12QueryHeap> query;D3D12_QUERY_HEAP_DESC q{};q.Type=D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;q.Count=1;
        Check(device->CreateQueryHeap(&q,IID_PPV_ARGS(&query)));auto result=Buffer(device.Get(),sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS),D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        renderer.RecordingList()->BeginQuery(query.Get(),D3D12_QUERY_TYPE_PIPELINE_STATISTICS,0);
        renderer.ExecuteGraphics(pipeline,bindings,state,std::span(&target,1),nullptr,std::span(&viewport,1),std::span(&scissor,1),blend, {3},{},nullptr,false);
        renderer.RecordingList()->EndQuery(query.Get(),D3D12_QUERY_TYPE_PIPELINE_STATISTICS,0);
        renderer.RecordingList()->ResolveQueryData(query.Get(),D3D12_QUERY_TYPE_PIPELINE_STATISTICS,0,1,result.handle.Get(),0);
        auto transfer=TextureTransfer::Create(device.Get(),texture,true);transfer.Readback(renderer.RecordingList(),texture);renderer.Finish();
        std::array<u8,256> bytes{};transfer.ReadRows(bytes.data(),bytes.size(),32);
        void* stats{};Check(result.handle->Map(0,nullptr,&stats));const auto s=*static_cast<D3D12_QUERY_DATA_PIPELINE_STATISTICS*>(stats);result.handle->Unmap(0,nullptr);
        printf("rect flip=%u IA=%llu VS=%llu HS=%llu DS=%llu clip_in=%llu clip_out=%llu PS=%llu topology=%u mask=%u discard=%u\n",flip,s.IAVertices,s.VSInvocations,s.HSInvocations,s.DSInvocations,s.CInvocations,s.CPrimitives,s.PSInvocations,unsigned(state.topology),state.pipeline.BlendState.RenderTarget[0].RenderTargetWriteMask,unsigned(state.rasterizer_discard));
        renderer.ValidateDebugMessages();
        for(size_t i=0;i<64;++i)if(bytes[i*4]!=255||bytes[i*4+1]!=0||bytes[i*4+2]!=0||bytes[i*4+3]!=255)
            throw std::runtime_error("Production RectList output clipped/incorrect at pixel "+std::to_string(i)+" flip "+std::to_string(flip)+" RGBA "+std::to_string(bytes[i*4])+","+std::to_string(bytes[i*4+1])+","+std::to_string(bytes[i*4+2])+","+std::to_string(bytes[i*4+3]));
        renderer.ValidateDebugMessages();
    }
    puts("PASS: production rectangle hull/domain stages produce all64 pixels with both viewport orientations");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
