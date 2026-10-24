// SPDX-License-Identifier: GPL-2.0-or-later
#include "primitive_stream_arguments.h"
#include "primitive_stream_arguments_shader.h"
#include "primitive_stream_partition_shader.h"
#include "rasterizer.h"
#include <limits>
#include <algorithm>
#include <cstring>
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
       (primitive!=1&&primitive!=2&&primitive!=3&&primitive!=4&&primitive!=6)||capacity%primitive||first%primitive||!limit||limit%primitive||
       first>capacity||limit>capacity-first||u64(capacity)*stride>std::numeric_limits<u32>::max())
        throw std::invalid_argument("Invalid bounded primitive stream range");
    return {stride,capacity,primitive,first,limit};
}
PrimitiveStreamRange AuxiliaryPointStreamRange(u32 patch,u32 vertices,u32 instances,u32 words){
    if(patch!=3&&patch!=4)throw std::invalid_argument("Auxiliary point stream requires rectangle/quad patches");
    const u64 primitives=std::max<u64>(1,u64(vertices/patch)*2*instances);
    if(primitives>UINT32_MAX/3)throw std::overflow_error("Auxiliary tessellation capture exceeds native vertex addressing");
    const u32 capacity=u32(primitives)*3;
    return PrimitiveStreamRange::Make(Shader::NativePointCapture::StreamRecordWords(words)*4,capacity,3,0,capacity);
}
PrimitiveStreamRange TessellatedPointStreamRange(u32 patch,u32 vertices,u32 instances,u32 words,u32 primitive){
    if(!patch||patch>32||(primitive!=1&&primitive!=2&&primitive!=3))
        throw std::invalid_argument("Invalid tessellated source patch or primitive width");
    // D3D hardware tessellation is bounded by factor64. Microsoft's fixed
    // function reference (bundled Mesa tessellator.hpp MAX_INDEX_COUNT) reserves
    // 64*64*2*3 indices and at most65*65 domain points. Serializing complete
    // GS input primitives duplicates their vertices: use the index bound for
    // every output topology, including point/line modes, rather than a unique
    // domain-point count.
    constexpr u32 serialized_vertices_per_patch=64*64*2*3;
    const u32 stride=Shader::NativePointCapture::StreamRecordWords(words)*4;
    const u64 patches=u64(vertices/patch)*instances;
    // Check before multiplying; UINT32_MAX input counts and instance counts
    // must not wrap a 64-bit product or silently drop later patches.
    if(patches>(UINT32_MAX/stride)/serialized_vertices_per_patch)
        throw std::overflow_error("Complete tessellated source exceeds 32-bit SO byte addressing");
    const u32 capacity=u32(std::max<u64>(primitive,patches*serialized_vertices_per_patch));
    return PrimitiveStreamRange::Make(stride,capacity,primitive,0,capacity);
}
PrimitiveStreamPartitions::PrimitiveStreamPartitions(const PrimitiveStreamRange& input,u32 calls,u32 emissions,u64 budget)
    :input_(PrimitiveStreamRange::Make(input.stride,input.capacity_vertices,input.vertices_per_primitive,input.first,input.limit)),
    calls_(calls),emissions_(emissions){
    if(!calls||!emissions||!budget)throw std::invalid_argument("Invalid primitive stream capture partition budget");
    const u64 records=std::min<u64>(budget,UINT32_MAX)/Shader::NativePointCapture::RecordBytes;
    // Divide before multiplying: even UINT32_MAX invocation/emission inputs
    // must fail cleanly instead of wrapping an intermediate byte product.
    const u64 primitives=records/calls/emissions;
    if(!primitives)throw std::overflow_error("One original GS primitive exceeds capture partition budget");
    vertices_=u32(std::min<u64>(input.limit,u64(input.vertices_per_primitive)*primitives));
    count_=u32((u64(input.limit)+vertices_-1)/vertices_);
}
PrimitiveStreamPartition PrimitiveStreamPartitions::At(u32 index)const{
    if(index>=count_)throw std::out_of_range("Primitive stream partition index exceeds complete coverage");
    const u32 consumed=u32(u64(index)*vertices_);
    const auto range=PrimitiveStreamRange::Make(input_.stride,input_.capacity_vertices,input_.vertices_per_primitive,
        input_.first+consumed,std::min(vertices_,input_.limit-consumed));
    return {range,Shader::NativePointCapture::Plan::Make(range.limit/range.vertices_per_primitive,calls_,emissions_,1),
        index==0,index+1==count_};
}
PrimitiveStreamArguments::PrimitiveStreamArguments(ID3D12Device* device){
    D3D12_ROOT_PARAMETER parameters[3]{};
    parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;parameters[0].Descriptor={0,0};
    parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;parameters[1].Descriptor={0,0};
    parameters[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;parameters[2].Constants={0,0,8};
    const D3D12_ROOT_SIGNATURE_DESC signature{3,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
    ComPtr<ID3DBlob> blob,errors;
    Check(D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors));
    Check(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root_)));
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};desc.pRootSignature=root_.Get();
    desc.CS={NativePrimitiveStreamArguments,sizeof(NativePrimitiveStreamArguments)};
    Check(device->CreateComputePipelineState(&desc,IID_PPV_ARGS(&pipeline_)));
    desc.CS={NativePrimitiveStreamPartition,sizeof(NativePrimitiveStreamPartition)};
    Check(device->CreateComputePipelineState(&desc,IID_PPV_ARGS(&partition_pipeline_)));
}
void PrimitiveStreamArguments::Record(Rasterizer& renderer,Resource& filled,Resource& arguments,const PrimitiveStreamRange& range){
    (void)PrimitiveStreamRange::Make(range.stride,range.capacity_vertices,range.vertices_per_primitive,range.first,range.limit);
    RecordCount(renderer,filled,arguments,range.stride,range.first,range.limit,1);
}
void PrimitiveStreamArguments::RecordPartition(Rasterizer& renderer,Resource& filled,Resource& output,
    const PrimitiveStreamPartition& part){
    const auto& plan=part.capture;const auto& range=part.input;
    (void)PrimitiveStreamRange::Make(range.stride,range.capacity_vertices,range.vertices_per_primitive,range.first,range.limit);
    const u32 width=range.vertices_per_primitive;
    const auto checked=Shader::NativePointCapture::Plan::Make(plan.primitive_bound,plan.invocations,plan.emissions,plan.instances);
    if(plan.instances!=1||checked.records!=plan.records||checked.bytes!=plan.bytes||!plan.primitive_bound||
       plan.primitive_bound!=range.limit/width)throw std::invalid_argument("Invalid captured replay count plan");
    const u64 multiplier=u64(plan.invocations)*plan.emissions;
    if(multiplier>UINT32_MAX)throw std::overflow_error("Captured replay count multiplier overflow");
    RecordCount(renderer,filled,output,range.stride,range.first,range.limit,u32(multiplier),width);
}
void PrimitiveStreamArguments::RecordCount(Rasterizer& renderer,Resource& filled,Resource& arguments,
    u32 divisor,u32 first,u32 limit,u32 multiplier,u32 primitive_width){
    std::lock_guard lock(renderer.ExecutionMutex());
    if(!divisor||!limit||!multiplier||(primitive_width?limit/primitive_width:limit)>UINT32_MAX/multiplier)
        throw std::invalid_argument("Invalid bounded GPU draw count conversion");
    if(!filled.handle||!arguments.handle||filled.handle.Get()==arguments.handle.Get())
        throw std::invalid_argument("Primitive stream counter/arguments require distinct resources");
    const auto source=filled.handle->GetDesc(),destination=arguments.handle->GetDesc();
    if(source.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||source.Width<4||
       (source.Flags&D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)||
       destination.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||destination.Width<(primitive_width?48:sizeof(D3D12_DRAW_ARGUMENTS))||
       !(destination.Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
        throw std::invalid_argument("Primitive stream counter/argument resource extent or usage");
    // Retain both native objects and placed allocations through actual queue
    // retirement; even a short-lived pass owner must not release its PSO early.
    renderer.RetainUntilFinish([source=filled.handle,destination=arguments.handle,
        source_allocation=filled.allocation,destination_allocation=arguments.allocation,
        root=root_,pipeline=primitive_width?partition_pipeline_:pipeline_](u64){});
    auto* list=renderer.RecordingList();
    filled.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    arguments.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    list->SetComputeRootSignature(root_.Get());list->SetPipelineState(primitive_width?partition_pipeline_.Get():pipeline_.Get());
    list->SetComputeRootShaderResourceView(0,filled.handle->GetGPUVirtualAddress());
    list->SetComputeRootUnorderedAccessView(1,arguments.handle->GetGPUVirtualAddress());
    const u32 constants[]{divisor,first,limit,multiplier,primitive_width,0,0,0};
    list->SetComputeRoot32BitConstants(2,8,constants,0);list->Dispatch(1,1,1);
    arguments.UavBarrier(list);arguments.Transition(list,D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
}
PrimitiveStreamPass::PrimitiveStreamPass(ID3D12Device* device):generator_(device),
    zero_(Buffer(device,8,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ)){
    void* mapped{};D3D12_RANGE none{};Check(zero_.handle->Map(0,&none,&mapped));
    const u64 zero=0;memcpy(mapped,&zero,8);D3D12_RANGE written{0,8};zero_.handle->Unmap(0,&written);
}
std::shared_ptr<PrimitiveStreamLease> PrimitiveStreamPass::Capture(Rasterizer& renderer,u32 stride,u32 capacity,
    u32 primitive,const std::function<void(PrimitiveStreamLease&)>& producer){
    if(!producer)throw std::invalid_argument("Missing primitive stream producer");
    const auto range=PrimitiveStreamRange::Make(stride,capacity,primitive,0,capacity);
    const u64 bytes=u64(stride)*capacity;
    std::lock_guard lock(renderer.ExecutionMutex());renderer.CollectRetirements();
    const auto idle=[](const auto& arena){
        return arena.use_count()==1&&
            (!arena->data.allocation||arena->data.allocation.use_count()==1)&&
            (!arena->filled_size.allocation||arena->filled_size.allocation.use_count()==1)&&
            (!arena->arguments.allocation||arena->arguments.allocation.use_count()==1);
    };
    std::shared_ptr<PrimitiveStreamLease>* best{};
    for(auto& arena:arenas_)if(idle(arena)&&arena->data.handle->GetDesc().Width>=bytes&&
       (!best||arena->data.handle->GetDesc().Width<(*best)->data.handle->GetDesc().Width))best=&arena;
    std::shared_ptr<PrimitiveStreamLease> arena;
    if(best)arena=*best;
    else{
        std::erase_if(arenas_,idle);
        arena=std::make_shared<PrimitiveStreamLease>();
        arena->data=renderer.AcquireTransientBuffer(bytes);
        // Current D3D12 validation requires an eight-byte backing interval for
        // BufferFilledSizeLocation. The bounded count shader reads its low DWORD.
        arena->filled_size=renderer.AcquireTransientBuffer(8);
        arena->arguments=renderer.AcquireTransientBuffer(48);
        arenas_.push_back(arena);
    }
    arena->range=range;
    // The pass may die or ownership preparation may Finish before the draw.
    // The local lease spans that callback; draw/generator retain allocations
    // through later submission, preventing reuse while work is in flight.
    renderer.RetainUntilFinish([arena,zero=zero_.handle](u64){});
    try{
        auto* list=renderer.RecordingList();arena->filled_size.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
        list->CopyBufferRegion(arena->filled_size.handle.Get(),0,zero_.handle.Get(),0,8);
        producer(*arena);
        arena->data.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        generator_.Record(renderer,arena->filled_size,arena->arguments,range);
        renderer.RetainUntilFinish([arena](u64){});
        return arena;
    }catch(...){try{renderer.Finish();}catch(...){}throw;}
}
void PrimitiveStreamPass::Replay(Rasterizer& renderer,const std::shared_ptr<PrimitiveStreamLease>& captured,
    u32 calls,u32 emissions,u64 budget,const std::function<void(PrimitiveStreamLease&,const PrimitiveStreamPartition&)>& consumer){
    auto lease=captured;
    if(!lease||!consumer||!lease->data.handle||!lease->filled_size.handle||!lease->arguments.handle)
        throw std::invalid_argument("Invalid captured primitive stream replay");
    const auto& range=lease->range;
    if(range.first||range.limit!=range.capacity_vertices||lease->data.handle->GetDesc().Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||
       u64(range.stride)*range.capacity_vertices>lease->data.handle->GetDesc().Width)
        throw std::invalid_argument("Primitive stream replay requires the complete captured capacity");
    const PrimitiveStreamPartitions partitions(range,calls,emissions,budget);
    std::lock_guard lock(renderer.ExecutionMutex());renderer.RetainUntilFinish([lease](u64){});
    try{
        for(u32 index=0;index<partitions.Count();++index){
            const auto partition=partitions.At(index);
            generator_.RecordPartition(renderer,lease->filled_size,lease->arguments,partition);
            consumer(*lease,partition);
        }
        // A consumer may Finish and consume the initial lease callback. Pin
        // the final queued replay as well, including committed allocations.
        renderer.RetainUntilFinish([lease](u64){});
    }catch(...){try{renderer.Finish();}catch(...){}throw;}
}
} // namespace NativeDX12
