// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <sirit/sirit.h>
#include <cstdio>
#include "video_core/renderer_d3d12/guest_graphics_runtime.h"
namespace NativeDX12::Tests {
inline std::vector<u32> DrawSysvalVertex(){
    Sirit::Module m(0x00010500);m.AddCapability(spv::Capability::Shader);m.AddCapability(spv::Capability::DrawParameters);
    const auto u=m.TypeUInt(32),f=m.TypeFloat(32),u4=m.TypeVector(u,4),f4=m.TypeVector(f,4),v=m.TypeVoid();
    const auto input=[&](spv::BuiltIn builtin){auto id=m.AddGlobalVariable(m.TypePointer(spv::StorageClass::Input,u),spv::StorageClass::Input);
        m.Decorate(id,spv::Decoration::BuiltIn,builtin);return id;};
    const auto vertex=input(spv::BuiltIn::VertexIndex),base=input(spv::BuiltIn::BaseVertex),
        instance=input(spv::BuiltIn::InstanceIndex),base_instance=input(spv::BuiltIn::BaseInstance);
    const auto position=m.AddGlobalVariable(m.TypePointer(spv::StorageClass::Output,f4),spv::StorageClass::Output);
    m.Decorate(position,spv::Decoration::BuiltIn,spv::BuiltIn::Position);
    const auto data=m.AddGlobalVariable(m.TypePointer(spv::StorageClass::Output,u4),spv::StorageClass::Output);
    m.Decorate(data,spv::Decoration::Location,0u);
    const auto main=m.OpFunction(v,spv::FunctionControlMask::MaskNone,m.TypeFunction(v));
    const std::array interfaces{vertex,base,instance,base_instance,position,data};m.AddEntryPoint(spv::ExecutionModel::Vertex,main,"main",interfaces);
    m.AddLabel(m.OpLabel());const std::array words{m.OpLoad(u,vertex),m.OpLoad(u,base),m.OpLoad(u,instance),m.OpLoad(u,base_instance)};
    m.OpStore(data,m.OpCompositeConstruct(u4,words));
    const auto zero=m.Constant(f,0.f),one=m.Constant(f,1.f);const std::array point{zero,zero,zero,one};
    m.OpStore(position,m.OpCompositeConstruct(f4,point));m.OpReturn();m.OpFunctionEnd();return m.Assemble();
}
inline void CheckDrawSysvalsGpu(ID3D12Device* device,Rasterizer& renderer,const std::wstring& dxc,std::span<const u8> vertex){
    namespace D=Shader::Backend::DX12;
    const auto gs=D::CompileGeometryDxil(dxc,R"(
struct V {float4 position:SV_Position;uint4 value:TEXCOORD0;};
struct O {float4 position:SV_Position;uint4 value:CAPTURE0;};
[maxvertexcount(1)] void main(point V input[1],inout PointStream<O> stream){O o;o.position=input[0].position;o.value=input[0].value;stream.Append(o);}
)");if(!gs.success)throw std::runtime_error(gs.diagnostics);
    AmdGpu::Regs regs{};regs.primitive_type=AmdGpu::PrimitiveType::PointList;auto state=BuildGuestGraphicsState(regs,{});state.rasterizer_discard=true;
    const D3D12_SO_DECLARATION_ENTRY entry{0,"CAPTURE",0,0,4,0};const UINT stride=16;
    state.pipeline.StreamOutput={&entry,1,&stride,1,D3D12_SO_NO_RASTERIZED_STREAM};
    ShaderPipeline pipeline(device,dxc,{vertex,{}, {},{},gs.bytecode},state.pipeline);
    for(bool expanded:std::array{false,true}){
        const u32 count=expanded?6:3,records=count*2,bytes=records*16;
        auto seed=Buffer(device,32+bytes+16,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        auto output=renderer.AcquireTransientBuffer(bytes+16),counter=renderer.AcquireTransientBuffer(4);
        auto result=Buffer(device,bytes+16,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        void* mapped{};D3D12_RANGE none{};Check(seed.handle->Map(0,&none,&mapped));memset(mapped,0xcd,32+bytes+16);
        const u32 indices[]{7,8,8,9,9,7};memcpy(mapped,indices,sizeof(indices));const u32 zero=0;memcpy(static_cast<std::byte*>(mapped)+24,&zero,4);seed.handle->Unmap(0,nullptr);
        auto* list=renderer.RecordingList();output.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);counter.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
        list->CopyBufferRegion(output.handle.Get(),0,seed.handle.Get(),32,bytes+16);list->CopyBufferRegion(counter.handle.Get(),0,seed.handle.Get(),24,4);
        GraphicsRuntime runtime{};runtime.first_vertex=expanded?0:7;runtime.base_instance=11;
        ResourceBindings bindings(device,pipeline.Layout());auto constants=Buffer(device,1024,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        Check(constants.handle->Map(0,&none,&mapped));memset(mapped,0,1024);memcpy(mapped,&runtime,sizeof(runtime));constants.handle->Unmap(0,nullptr);
        bindings.Cbv(0,31,constants.handle.Get(),0,1024);
        const Rasterizer::IndexBinding index{&seed,0,24,DXGI_FORMAT_R32_UINT};
        const Rasterizer::StreamOutputBinding target{&output,&counter,0,bytes,0};
        const D3D12_VIEWPORT viewport{0,0,1,1,0,1};const D3D12_RECT scissor{0,0,1,1};const float blend[4]{};
        renderer.ExecuteGraphics(pipeline,bindings,state,{},nullptr,std::span(&viewport,1),std::span(&scissor,1),blend,
            {count,2,expanded?0u:7u,11,0},{},expanded?&index:nullptr,false,nullptr,&target);
        output.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        renderer.RecordingList()->CopyBufferRegion(result.handle.Get(),0,output.handle.Get(),0,bytes+16);renderer.Finish();
        std::vector<std::byte> expected(bytes+16,std::byte{0xcd});
        for(u32 instance=0;instance<2;++instance)for(u32 i=0;i<count;++i){const u32 words[]{expanded?indices[i]:7+i,7,11+instance,11};memcpy(expected.data()+(instance*count+i)*16,words,16);}
        D3D12_RANGE all{0,bytes+16};Check(result.handle->Map(0,&all,&mapped));const bool equal=!memcmp(mapped,expected.data(),expected.size());
        if(!equal)for(u32 record=0;record<records;++record){
            std::array<u32,4> actual{},wanted{};memcpy(actual.data(),static_cast<std::byte*>(mapped)+record*16,16);memcpy(wanted.data(),expected.data()+record*16,16);
            if(actual!=wanted){fprintf(stderr,"draw-sysvals expanded=%u record=%u actual=%08x/%08x/%08x/%08x expected=%08x/%08x/%08x/%08x\n",
                unsigned(expanded),record,actual[0],actual[1],actual[2],actual[3],wanted[0],wanted[1],wanted[2],wanted[3]);break;}
        }
        result.handle->Unmap(0,&none);
        if(!equal)throw std::runtime_error(expanded?"Expanded nonindexed draw lost Vulkan VertexIndex/BaseVertex/instance contract":"Direct draw sysval contract mismatch");
    }
}
}
