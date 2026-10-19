// SPDX-License-Identifier: GPL-2.0-or-later
#include "primitive_stream_arguments.h"
#include "primitive_stream_arguments_shader.h"
#include "rasterizer.h"
#include <limits>
#include <dxcapi.h>
#include <d3d12shader.h>

namespace NativeDX12 {
StreamOutputLayout ReflectPrimitiveStreamLayout(const std::wstring& dxc,std::span<const u8> bytecode){
    struct Library {HMODULE value;~Library(){if(value)FreeLibrary(value);}} library{LoadLibraryW(dxc.c_str())};
    if(!library.value)throw std::runtime_error("Cannot load pinned DXC for primitive stream reflection");
    const auto create=reinterpret_cast<DxcCreateInstanceProc>(GetProcAddress(library.value,"DxcCreateInstance"));
    if(!create)throw std::runtime_error("DXC primitive stream reflection entry point missing");
    ComPtr<IDxcUtils> utils;Check(create(CLSID_DxcUtils,IID_PPV_ARGS(&utils)));
    const DxcBuffer input{bytecode.data(),bytecode.size(),DXC_CP_ACP};
    ComPtr<ID3D12ShaderReflection> reflection;Check(utils->CreateReflection(&input,IID_PPV_ARGS(&reflection)));
    D3D12_SHADER_DESC desc{};Check(reflection->GetDesc(&desc));
    if(desc.OutputParameters!=1||desc.GSOutputTopology!=D3D_PRIMITIVE_TOPOLOGY_POINTLIST)
        throw std::invalid_argument("Primitive stream shader must output one uint4 point payload");
    D3D12_SIGNATURE_PARAMETER_DESC output{};Check(reflection->GetOutputParameterDesc(0,&output));
    if(output.ComponentType!=D3D_REGISTER_COMPONENT_UINT32||output.Mask!=15||output.Stream||
       output.SystemValueType!=D3D_NAME_UNDEFINED||!output.SemanticName||!*output.SemanticName)
        throw std::invalid_argument("Unexpected primitive stream payload signature");
    const D3D12_SO_DECLARATION_ENTRY entry{0,output.SemanticName,output.SemanticIndex,0,4,0};
    const UINT stride=16;
    return StreamOutputLayout({&entry,1,&stride,1,D3D12_SO_NO_RASTERIZED_STREAM});
}
PrimitiveStreamRange PrimitiveStreamRange::Make(u32 stride,u32 capacity,u32 primitive,u32 first,u32 limit){
    if(!stride||stride%4||stride>D3D12_SO_BUFFER_MAX_STRIDE_IN_BYTES||!capacity||
       !primitive||primitive>3||capacity%primitive||first%primitive||!limit||limit%primitive||
       first>capacity||limit>capacity-first||u64(capacity)*stride>std::numeric_limits<u32>::max())
        throw std::invalid_argument("Invalid bounded primitive stream range");
    return {stride,capacity,primitive,first,limit};
}
PrimitiveStreamArguments::PrimitiveStreamArguments(ID3D12Device* device){
    D3D12_ROOT_PARAMETER parameters[3]{};
    parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;parameters[0].Descriptor={0,0};
    parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;parameters[1].Descriptor={0,0};
    parameters[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;parameters[2].Constants={0,0,3};
    const D3D12_ROOT_SIGNATURE_DESC signature{3,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
    ComPtr<ID3DBlob> blob,errors;
    Check(D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors));
    Check(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root_)));
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};desc.pRootSignature=root_.Get();
    desc.CS={NativePrimitiveStreamArguments,sizeof(NativePrimitiveStreamArguments)};
    Check(device->CreateComputePipelineState(&desc,IID_PPV_ARGS(&pipeline_)));
}
void PrimitiveStreamArguments::Record(Rasterizer& renderer,Resource& filled,Resource& arguments,const PrimitiveStreamRange& range){
    std::lock_guard lock(renderer.ExecutionMutex());
    (void)PrimitiveStreamRange::Make(range.stride,range.capacity_vertices,range.vertices_per_primitive,range.first,range.limit);
    if(!filled.handle||!arguments.handle||filled.handle.Get()==arguments.handle.Get())
        throw std::invalid_argument("Primitive stream counter/arguments require distinct resources");
    const auto source=filled.handle->GetDesc(),destination=arguments.handle->GetDesc();
    if(source.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||source.Width<4||
       (source.Flags&D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)||
       destination.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||destination.Width<sizeof(D3D12_DRAW_ARGUMENTS)||
       !(destination.Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
        throw std::invalid_argument("Primitive stream counter/argument resource extent or usage");
    // Retain both native objects and placed allocations through actual queue
    // retirement; even a short-lived pass owner must not release its PSO early.
    renderer.RetainUntilFinish([source=filled.handle,destination=arguments.handle,
        source_allocation=filled.allocation,destination_allocation=arguments.allocation,
        root=root_,pipeline=pipeline_](u64){});
    auto* list=renderer.RecordingList();
    filled.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    arguments.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    list->SetComputeRootSignature(root_.Get());list->SetPipelineState(pipeline_.Get());
    list->SetComputeRootShaderResourceView(0,filled.handle->GetGPUVirtualAddress());
    list->SetComputeRootUnorderedAccessView(1,arguments.handle->GetGPUVirtualAddress());
    const u32 constants[]{range.stride,range.first,range.limit};
    list->SetComputeRoot32BitConstants(2,3,constants,0);list->Dispatch(1,1,1);
    arguments.UavBarrier(list);arguments.Transition(list,D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
}
} // namespace NativeDX12
