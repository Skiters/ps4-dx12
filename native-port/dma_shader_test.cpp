// SPDX-License-Identifier: GPL-2.0-or-later
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"
#include "shader_recompiler/backend/dxil/spirv_converter.h"
#include "shader_recompiler/frontend/fetch_shader.h"
#include "common/logging/log.h"
#include "video_core/renderer_d3d12/shader_pipeline.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include "video_core/renderer_d3d12/compute_pipeline.h"
#include "video_core/renderer_d3d12/guest_compute.h"
#include "video_core/renderer_d3d12/dma_fault_compactor.h"
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <cstdio>
#include <cstring>
#include <future>
#include <chrono>
using namespace NativeDX12;
namespace Dma=Shader::NativeDma;
using Context=Shader::Backend::SPIRV::EmitContext;
// Standalone diagnostic/unused vertex dependency. Fail if this compute-only
// fixture accidentally enters a fetch-shader path; do not emulate it in tests.
namespace Common::Log { std::array<Level,size_t(Class::Count)> g_class_levels{}; }
namespace Shader::Gcn {
bool ParseFetchShader(const Shader::Info&,FetchShaderData&){throw std::logic_error("Unexpected vertex fetch in DMA compute fixture");}
}

std::vector<u32> MakeShader(bool alias=false,bool scattered=false){
    Shader::Profile profile{};profile.supported_spirv=0x00010500;profile.native_dma_buffers=true;
    profile.sparse_page_shift=Dma::PageShift;profile.support_int64=true;
    Shader::RuntimeInfo runtime{};runtime.sw_stage=Shader::SwStage::Compute;
    Shader::Info info;info.hw_stage=Shader::HwStage::Compute;info.sw_stage=Shader::SwStage::Compute;
    info.uses_dma=true;info.readconst_types=Shader::Info::ReadConstType::Immediate|Shader::Info::ReadConstType::Dynamic;
    info.buffers.push_back({.used_types=Shader::IR::Type::U32,.buffer_type=Shader::BufferType::Flatbuf});
    info.buffers.push_back({.used_types=Shader::IR::Type::U64,.buffer_type=Shader::BufferType::BdaPagetable,.is_written=true});
    info.buffers.push_back({.used_types=Shader::IR::Type::U32,.buffer_type=Shader::BufferType::FaultBuffer,.is_written=true});
    info.buffers.push_back({.used_types=Shader::IR::Type::U32,.buffer_type=Shader::BufferType::SharedMemory,.is_written=true});
    if(alias)info.buffers.push_back({.used_types=Shader::IR::Type::U32,.buffer_type=Shader::BufferType::Guest,.is_written=true});
    Shader::Backend::Bindings binding{};Context c(profile,runtime,info,binding);c.AddCapability(spv::Capability::Int64);
    const auto invocation=c.DefineInput(c.U32[3],{},spv::BuiltIn::LocalInvocationId);
    const auto fn=c.OpFunction(c.void_id,spv::FunctionControlMask::MaskNone,c.TypeFunction(c.void_id));c.AddLabel();
    const auto lane=c.OpCompositeExtract(c.U32[1],c.OpLoad(c.U32[3],invocation),0);
    if(alias){
        const auto [buffer,pointer]=c.buffers[4].Alias(Context::PointerType::U32);
        // The ordinary buffer starts four bytes before the first DMA page.
        const auto word=c.OpIMul(c.U32[1],c.OpIAdd(c.U32[1],lane,c.ConstU32(1U)),c.ConstU32(Dma::PageSize/4));
        c.OpStore(c.OpAccessChain(pointer,buffer,c.u32_zero_value,word),c.OpIAdd(c.U32[1],lane,c.ConstU32(0xcafe0000U)));
        c.OpControlBarrier(c.ConstU32(u32(spv::Scope::Workgroup)),c.ConstU32(u32(spv::Scope::Device)),
            c.ConstU32(u32(spv::MemorySemanticsMask::AcquireRelease|spv::MemorySemanticsMask::UniformMemory)));
    }
    auto low=c.OpShiftLeftLogical(c.U32[1],lane,c.ConstU32(Dma::PageShift+(scattered?1U:0U)));
    // Pages above 4 GiB, with the final lanes testing invalid address/alignment.
    low=c.OpSelect(c.U32[1],c.OpIEqual(c.U1[1],lane,c.ConstU32(63U)),c.OpIAdd(c.U32[1],low,c.ConstU32(1U)),low);
    const auto high=c.OpSelect(c.U32[1],c.OpIEqual(c.U1[1],lane,c.ConstU32(62U)),c.ConstU32(256U),c.ConstU32(16U));
    const auto base=c.OpCompositeConstruct(c.U32[2],low,high);
    const auto offset=c.ConstU32(Dma::PageSize/4-1);
    const auto dynamic=c.OpFunctionCall(c.U32[1],c.read_const_dynamic,base,offset);
    const auto immediate=c.OpFunctionCall(c.U32[1],c.read_const,base,offset,c.ConstU32(0U));
    const auto [output,pointer]=c.buffers[3].Alias(Context::PointerType::U32);
    c.OpStore(c.OpAccessChain(pointer,output,c.u32_zero_value,lane),dynamic);
    c.OpStore(c.OpAccessChain(pointer,output,c.u32_zero_value,c.OpIAdd(c.U32[1],lane,c.ConstU32(64U))),immediate);
    const auto second_base=scattered?c.OpCompositeConstruct(c.U32[2],
        c.OpIAdd(c.U32[1],c.OpShiftLeftLogical(c.U32[1],lane,c.ConstU32(Dma::PageShift+1)),c.ConstU32(128U*Dma::PageSize)),c.ConstU32(16U)):c.ConstU32(0U,16U);
    const auto wide_offset=c.OpFunctionCall(c.U32[1],c.read_const_dynamic,second_base,
        c.ConstU32(scattered?Dma::PageSize/4-1:0x40003fffU));
    c.OpStore(c.OpAccessChain(pointer,output,c.u32_zero_value,c.OpIAdd(c.U32[1],lane,c.ConstU32(128U))),wide_offset);
    const auto carry=c.OpFunctionCall(c.U32[1],c.read_const_dynamic,c.ConstU32(0xfffffffcU,15U),c.ConstU32(1U));
    c.OpStore(c.OpAccessChain(pointer,output,c.u32_zero_value,c.OpIAdd(c.U32[1],lane,c.ConstU32(192U))),carry);
    c.OpReturn();c.OpFunctionEnd();c.AddEntryPoint(spv::ExecutionModel::GLCompute,fn,"main",c.interfaces);
    c.AddExecutionMode(fn,spv::ExecutionMode::LocalSize,64U,1U,1U);
    return c.Assemble();
}
int wmain(int argc,wchar_t** argv){try{
    if(argc!=3)throw std::runtime_error("usage: dma-shader-test converter.dll dxcompiler.dll");
    ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
    const auto spirv=MakeShader();
    for(size_t i=5;i<spirv.size();){const auto word=spirv[i],count=word>>16;
        if(!count||i+count>spirv.size())throw std::runtime_error("Malformed DMA intermediate");
        if((word&65535)==u32(spv::Op::OpConvertUToPtr)||
           ((word&65535)==u32(spv::Op::OpMemoryModel)&&spirv[i+1]!=u32(spv::AddressingModel::Logical)))
            throw std::runtime_error("Native DMA retained physical SPIR-V addressing");
        i+=count;
    }
    Shader::Backend::DX12::SpirvConvertOptions options;options.stage=Shader::Backend::DX12::SpirvStage::Compute;
    options.native_storage_buffer_uavs=true;
    auto dxil=Shader::Backend::DX12::ConvertSpirv(argv[1],spirv,options);
    if(!dxil.success)throw std::runtime_error(dxil.diagnostics);
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> a;if(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&a))==DXGI_ERROR_NOT_FOUND)break;
        DXGI_ADAPTER_DESC1 d{};a->GetDesc1(&d);if(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
        if(SUCCEEDED(D3D12CreateDevice(a.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))break;}
    if(!device)throw std::runtime_error("No native hardware device");
    ShaderPipeline pipeline(device.Get(),argv[2],dxil.bytecode);Rasterizer rasterizer(device.Get());
    ResourceBindings bindings(device.Get(),pipeline.Layout());
    std::vector<Resource> retained;
    const auto upload=[&](std::span<const std::byte> bytes){
        auto target=UavBuffer(device.Get(),bytes.size());target.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
        auto staging=Buffer(device.Get(),bytes.size(),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        void* data{};D3D12_RANGE empty{};Check(staging.handle->Map(0,&empty,&data));memcpy(data,bytes.data(),bytes.size());staging.handle->Unmap(0,nullptr);
        rasterizer.RecordingList()->CopyBufferRegion(target.handle.Get(),0,staging.handle.Get(),0,bytes.size());
        target.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS);retained.push_back(std::move(staging));return target;
    };
    const u32 fallback=0xdeadbeef;
    auto flat=upload(std::as_bytes(std::span(&fallback,1)));
    std::vector<u64> page_table(Dma::TableEntries);const u32 first_page=16u<<(32-Dma::PageShift);
    page_table[Dma::BankCountEntry]=Dma::BankCount;
    page_table[first_page]=1;page_table[first_page+1]=u64(Dma::BankSize)+1;
    page_table[first_page+2]=u64(Dma::BankSize)*(Dma::BankCount-1)+1;
    page_table[17u<<(32-Dma::PageShift)]=u64(Dma::BankSize)+1;
    page_table[first_page+3]=Dma::CacheBytes+1;page_table[first_page+4]=2;
    page_table[first_page+5]=u64(Dma::BankSize)-Dma::PageSize+5; // aligned but crosses a bank
    page_table[first_page+6]=Dma::PartialFlag|u64(Dma::PartialPageCount+1); // invalid secondary slot
    auto table=upload(std::as_bytes(std::span(page_table)));
    std::vector<u32> zeros(Dma::PageCount/32);auto faults=upload(std::as_bytes(std::span(zeros)));
    zeros.resize(256);auto output=upload(std::as_bytes(std::span(zeros)));
    std::array<Resource,3> banks;
    for(UINT i=0;i<3;++i){std::vector<u32> data(Dma::PageSize/4);data.front()=0x12340000+i;data.back()=0xabcd0000+i;banks[i]=upload(std::as_bytes(std::span(data)));}
    const auto bind=[&](UINT reg,UINT space,Resource* resource,UINT index=0){
        D3D12_UNORDERED_ACCESS_VIEW_DESC view{};view.Format=DXGI_FORMAT_R32_TYPELESS;view.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;
        view.Buffer.NumElements=resource?UINT(resource->handle->GetDesc().Width/4):Dma::PageSize/4;view.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
        bindings.Uav(reg,space,resource?resource->handle.Get():nullptr,view,index);
    };
    bind(0,0,&flat);bind(1,0,&table);bind(2,0,&faults);bind(3,0,&output);
    for(UINT i=0;i<Dma::BankCount;++i)bind(0,Dma::DescriptorSet,i==0?&banks[0]:i==1?&banks[1]:i==Dma::BankCount-1?&banks[2]:nullptr,i);
    rasterizer.ExecuteCompute(pipeline,bindings,1,1,1);
    const auto read=[&](Resource& source,UINT64 offset,UINT bytes,void* result){
        auto rb=Buffer(device.Get(),bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        source.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        rasterizer.RecordingList()->CopyBufferRegion(rb.handle.Get(),0,source.handle.Get(),offset,bytes);rasterizer.Finish();
        void* data{};D3D12_RANGE range{0,bytes};Check(rb.handle->Map(0,&range,&data));memcpy(result,data,bytes);D3D12_RANGE empty{};rb.handle->Unmap(0,&empty);
    };
    std::array<u32,256> values{};read(output,0,sizeof(values),values.data());
    for(UINT i=0;i<64;++i)if(values[i]!=(i<3?0xabcd0000+i:0)||values[i+64]!=(i<3?0xabcd0000+i:fallback))
        throw std::runtime_error("DMA read/fallback mismatch at lane "+std::to_string(i));
    for(UINT i=0;i<64;++i)if(values[i+128]!=0xabcd0001||values[i+192]!=0x12340000)
        throw std::runtime_error("DMA offset was truncated or 32-bit address carry was lost");
    u32 bitmap[2]{};read(faults,u64(first_page/32)*4,sizeof(bitmap),bitmap);
    if(bitmap[0]!=0xfffffff8||bitmap[1]!=0x3fffffff)throw std::runtime_error("Concurrent DMA faults lost bits or marked invalid addresses");
    // Corrupt/stale entries must not reach a declared but unpopulated bank.
    // Leave every descriptor after bank 1 uninitialized on tier-3 hardware.
    ResourceBindings sparse_bindings(device.Get(),pipeline.Layout());
    const auto sparse_bind=[&](UINT reg,UINT space,Resource& resource,UINT index=0){
        D3D12_UNORDERED_ACCESS_VIEW_DESC view{};view.Format=DXGI_FORMAT_R32_TYPELESS;view.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;
        view.Buffer.NumElements=UINT(resource.handle->GetDesc().Width/4);view.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
        sparse_bindings.Uav(reg,space,resource.handle.Get(),view,index);
    };
    sparse_bind(0,0,flat);sparse_bind(1,0,table);sparse_bind(2,0,faults);sparse_bind(3,0,output);
    sparse_bind(0,Dma::DescriptorSet,banks[0],0);sparse_bind(0,Dma::DescriptorSet,banks[1],1);
    sparse_bindings.UnreachableUavTail(0,Dma::DescriptorSet,2);
    const u64 live_banks=2;auto live_count=upload(std::as_bytes(std::span(&live_banks,1)));
    live_count.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
    table.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
    rasterizer.RecordingList()->CopyBufferRegion(table.handle.Get(),u64(Dma::BankCountEntry)*8,live_count.handle.Get(),0,8);
    table.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    faults.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    output.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    rasterizer.ExecuteCompute(pipeline,sparse_bindings,1,1,1);read(output,0,sizeof(values),values.data());
    if(values[0]!=0xabcd0000||values[1]!=0xabcd0001||values[2]!=0||values[66]!=fallback)
        throw std::runtime_error("DMA active-bank guard reached an unpopulated descriptor");
    rasterizer.ValidateDebugMessages();
    // Overflow must preserve every page bit for the next GPU compaction pass.
    // Include the final page in the 40-bit address domain and guard the output.
    std::vector<u32> source_bits(Dma::PageCount/32);
    for(u32 i=0;i<DmaFaultCompactor::Capacity+37;++i)source_bits[i/32]|=1u<<(i%32);
    source_bits.back()|=1u<<31;
    auto compact_faults=upload(std::as_bytes(std::span(source_bits)));
    std::vector<u32> compact_words(DmaFaultCompactor::Capacity+5,0xdeadbeef);
    auto compact_output=upload(std::as_bytes(std::span(compact_words)));
    auto compact_zero=Buffer(device.Get(),4,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
    void* zero_data{};D3D12_RANGE no_read{};Check(compact_zero.handle->Map(0,&no_read,&zero_data));
    memset(zero_data,0,4);compact_zero.handle->Unmap(0,nullptr);
    DmaFaultCompactor compactor(device.Get());std::set<u32> exported_pages;
    for(u32 pass=0;pass<3;++pass){
        compactor.Dispatch(rasterizer.RecordingList(),compact_faults,compact_output,compact_zero);
        read(compact_output,0,UINT(compact_words.size()*4),compact_words.data());
        const auto count=std::min(compact_words[0],DmaFaultCompactor::Capacity);
        if(count!=(pass==0?DmaFaultCompactor::Capacity:pass==1?38u:0u))
            throw std::runtime_error("GPU DMA compaction lost overflow or retained consumed faults");
        for(u32 i=1;i<=count;++i)if(!exported_pages.insert(compact_words[i]).second)
            throw std::runtime_error("GPU DMA compaction exported a page twice");
        for(u32 i=DmaFaultCompactor::Capacity+1;i<compact_words.size();++i)
            if(compact_words[i]!=0xdeadbeef)throw std::runtime_error("GPU DMA compaction overran its output queue");
    }
    if(exported_pages.size()!=DmaFaultCompactor::Capacity+38||!exported_pages.contains(Dma::PageCount-1))
        throw std::runtime_error("GPU DMA compaction did not cover the complete address domain");
    for(u32 i=0;i<DmaFaultCompactor::Capacity+37;++i)if(!exported_pages.contains(i))
        throw std::runtime_error("GPU DMA compaction omitted a requested page");
    // Exercise the production resource adapter, including cold faults, shared
    // backing, CPU changes and unmap/remap. No test-created DMA table or banks.
    constexpr VAddr guest_base=0x1000000000ULL,output_base=0x1200000000ULL;
    constexpr u64 guest_bytes=u64(Dma::PageSize)*256;
    struct Allocation {void* p;~Allocation(){if(p)VirtualFree(p,0,MEM_RELEASE);}};
    Allocation guest_owner{VirtualAlloc(reinterpret_cast<void*>(guest_base-Dma::PageSize),guest_bytes+Dma::PageSize,
        MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)};
    Allocation output_owner{VirtualAlloc(reinterpret_cast<void*>(output_base),Dma::PageSize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)};
    if(!guest_owner.p||!output_owner.p)throw std::runtime_error("Cannot allocate DMA guest fixture");
    auto* guest=reinterpret_cast<u32*>(guest_base);auto* result=reinterpret_cast<u32*>(output_base);
    for(u32 i=0;i<256;++i)guest[(i+1)*Dma::PageSize/4-1]=0xface0000+i;
    Rasterizer runtime(device.Get());runtime.MapMemory(guest_base-4,guest_bytes+4);runtime.MapMemory(output_base,1024);
    Shader::Info metadata;metadata.sw_stage=Shader::SwStage::Compute;metadata.uses_dma=true;
    metadata.flattened_ud_buf.resize(12);metadata.flattened_ud_buf[0]=fallback;
    metadata.buffers.push_back({.buffer_type=Shader::BufferType::Flatbuf});
    metadata.buffers.push_back({.buffer_type=Shader::BufferType::BdaPagetable,.is_written=true});
    metadata.buffers.push_back({.buffer_type=Shader::BufferType::FaultBuffer,.is_written=true});
    Shader::BufferResource output_metadata{};output_metadata.is_written=true;
    output_metadata.sharp_fetch.summary=Shader::SharpFetch<AmdGpu::Buffer>::Summary::SingleLoad;
    output_metadata.sharp_fetch.offsets[0]=4;metadata.buffers.push_back(output_metadata);
    AmdGpu::Buffer sharp{};sharp.base_address=output_base;sharp.num_records=1024;
    memcpy(metadata.flattened_ud_buf.data()+4,&sharp,sizeof(sharp));
    const auto run=[&](const ShaderPipeline& active){ExecuteGuestBufferCompute(device.Get(),runtime,active,metadata,{},{{1,1,1}});};
    run(pipeline);
    for(u32 i=0;i<64;++i)if(result[i]!=0||result[i+64]!=fallback)throw std::runtime_error("Cold runtime DMA fallback mismatch");
    run(pipeline);
    for(u32 i=0;i<64;++i)if(result[i]!=(i<62?0xface0000+i:0)||result[i+64]!=(i<62?0xface0000+i:fallback))
        throw std::runtime_error("Runtime DMA faults did not materialize resident pages");
    guest[Dma::PageSize/4-1]=0x1234abcd;guest[0]=0x76543210;run(pipeline);
    if(result[0]!=0x1234abcd||result[192]!=0x76543210)throw std::runtime_error("DMA cache ignored CPU modification");
    runtime.UnmapMemory(guest_base,Dma::PageSize);run(pipeline);
    if(result[0]!=0||result[64]!=fallback)throw std::runtime_error("DMA retained a stale unmapped page");
    runtime.MapMemory(guest_base,Dma::PageSize);run(pipeline);run(pipeline);
    if(result[0]!=0x1234abcd)throw std::runtime_error("DMA failed to rematerialize remapped memory");
    Check(VirtualFree(reinterpret_cast<void*>(guest_base),4096,MEM_DECOMMIT)?S_OK:E_FAIL);
    runtime.RegisterMemory(guest_base,4096);run(pipeline);
    if(result[0]!=0x1234abcd||result[192]!=0)throw std::runtime_error("DMA partial page lost valid data or accessed an uncommitted hole");
    if(!VirtualAlloc(reinterpret_cast<void*>(guest_base),4096,MEM_COMMIT,PAGE_READWRITE))
        throw std::runtime_error("Cannot recommit DMA fixture page");
    guest[0]=0x13579bdf;runtime.RegisterMemory(guest_base,4096);run(pipeline);run(pipeline);
    if(result[0]!=0x1234abcd||result[192]!=0x13579bdf)throw std::runtime_error("DMA failed to recover after host recommit");
    const VAddr tail=guest_base+Dma::PageSize-Dma::SubpageSize;
    Check(VirtualFree(reinterpret_cast<void*>(tail),Dma::SubpageSize,MEM_DECOMMIT)?S_OK:E_FAIL);
    runtime.RegisterMemory(tail,Dma::SubpageSize);run(pipeline);
    if(result[0]!=0||result[64]!=fallback||result[192]!=0x13579bdf)
        throw std::runtime_error("DMA hole fallback was not confined to the inaccessible subpage");
    if(!VirtualAlloc(reinterpret_cast<void*>(tail),Dma::SubpageSize,MEM_COMMIT,PAGE_READWRITE))
        throw std::runtime_error("Cannot recommit DMA tail subpage");
    guest[Dma::PageSize/4-1]=0x1234abcd;runtime.RegisterMemory(tail,Dma::SubpageSize);run(pipeline);
    if(result[0]!=0x1234abcd)throw std::runtime_error("DMA partial-to-full page translation retained a stale hole");
    runtime.UnmapMemory(guest_base,16384);run(pipeline);
    if(result[0]!=0x1234abcd||result[192]!=0)throw std::runtime_error("Partial guest unmap invalidated accessible neighboring subpages");
    runtime.MapMemory(guest_base,16384);run(pipeline);
    if(result[0]!=0x1234abcd||result[192]!=0x13579bdf)throw std::runtime_error("Partial guest remap retained stale subpage translation");
    auto alias_dxil=Shader::Backend::DX12::ConvertSpirv(argv[1],MakeShader(true),options);
    if(!alias_dxil.success)throw std::runtime_error(alias_dxil.diagnostics);
    ShaderPipeline alias_pipeline(device.Get(),argv[2],alias_dxil.bytecode);
    auto alias_metadata=output_metadata;alias_metadata.sharp_fetch.offsets[0]=8;metadata.buffers.push_back(alias_metadata);
    sharp.base_address=guest_base-4;sharp.num_records=guest_bytes+4;
    memcpy(metadata.flattened_ud_buf.data()+8,&sharp,sizeof(sharp));run(alias_pipeline);
    for(u32 i=0;i<62;++i)if(result[i]!=0xcafe0000+i)
        throw std::runtime_error("DMA did not observe same-dispatch UAV alias write at lane "+std::to_string(i)+
            " got="+std::to_string(result[i])+" cpu="+std::to_string(guest[(i+1)*Dma::PageSize/4-1]));
    runtime.ValidateDebugMessages();
    // A deliberately held GPU queue proves that four feedback areas can be
    // submitted without a CPU fence wait or premature mapping/reuse. Timeout
    // always releases the gate before joining, so a regression fails cleanly.
    metadata.buffers.pop_back();
    auto scattered_dxil=Shader::Backend::DX12::ConvertSpirv(argv[1],MakeShader(false,true),options);
    if(!scattered_dxil.success)throw std::runtime_error(scattered_dxil.diagnostics);
    ShaderPipeline scattered_pipeline(device.Get(),argv[2],scattered_dxil.bytecode);
    Rasterizer scattered_runtime(device.Get());scattered_runtime.MapMemory(guest_base,guest_bytes);scattered_runtime.MapMemory(output_base,1024);
    for(u32 pass=0;pass<2;++pass)
        ExecuteGuestBufferCompute(device.Get(),scattered_runtime,scattered_pipeline,metadata,{},{{1,1,1}});
    for(u32 i=0;i<64;++i){
        const auto expected=i<62?(i<32?0xcafe0000+i*2:0xface0000+i*2):0;
        if(result[i]!=expected||result[i+128]!=0xface0000+128+i*2)
            throw std::runtime_error("Scattered DMA backing failed beyond 64 independent resource banks");
    }
    scattered_runtime.ValidateDebugMessages();
    Rasterizer deferred(device.Get());deferred.MapMemory(guest_base,guest_bytes);deferred.MapMemory(output_base,1024);
    deferred.SetBufferOwnership([&](VAddr address,u64 bytes){return address==output_base&&bytes==1024;},
        [](VAddr address,std::span<const std::byte> bytes){memcpy(reinterpret_cast<void*>(address),bytes.data(),bytes.size());return true;},
        [](VAddr,u64){});
    ComPtr<ID3D12Fence> gate;Check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&gate)));
    deferred.WaitExternalFence(gate.Get(),1);
    auto submissions=std::async(std::launch::async,[&]{
        for(u32 i=0;i<4;++i)ExecuteGuestBufferCompute(device.Get(),deferred,pipeline,metadata,{},{{1,1,1}});
    });
    const bool returned=submissions.wait_for(std::chrono::seconds(3))==std::future_status::ready;
    Check(gate->Signal(1));submissions.get();
    if(!returned)throw std::runtime_error("DMA feedback waited for an unfinished GPU fence before ring capacity");
    deferred.ReadMemory(output_base,1024);
    for(u32 i=0;i<64;++i)if(result[i]!=0||result[i+64]!=fallback)
        throw std::runtime_error("Deferred cold DMA command consumed unfinished feedback");
    for(u32 i=0;i<12;++i)ExecuteGuestBufferCompute(device.Get(),deferred,pipeline,metadata,{},{{1,1,1}});
    deferred.ReadMemory(output_base,1024);
    for(u32 i=0;i<62;++i)if(result[i]!=0xcafe0000+i)
        throw std::runtime_error("DMA feedback ring lost resident pages after wraparound");
    deferred.ValidateDebugMessages();
    if(GetModuleHandleW(L"vulkan-1.dll"))throw std::runtime_error("Vulkan runtime unexpectedly loaded");
    puts("PASS: production ReadConst -> DXIL -> hardware; nonuniform banks, 64-bit addresses, atomic faults; runtime cold/warm pages, CPU refresh, unmap/remap, same-dispatch aliases, held-queue nonblocking feedback and ring wraparound");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
