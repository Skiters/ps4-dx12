// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_compute.h"
#include "video_core/renderer_d3d12/guest_image_readback.h"
#include "video_core/renderer_d3d12/page_watch.h"
#include "video_core/renderer_d3d12/backend_factory.h"
#include "shader_recompiler/backend/dxil/spirv_converter.h"
#include <d3d12sdklayers.h>
#include <fstream>
#include <future>
#include <chrono>
#include <cstdio>
#include <cstring>
using namespace NativeDX12;
int wmain(int argc,wchar_t** argv){try{
    if(argc!=4&&argc!=5)throw std::invalid_argument("Expected fixture, converter, DXC and optional sample fixture");
    std::ifstream file(argv[1],std::ios::binary|std::ios::ate);const auto bytes=file.tellg();
    if(bytes<=0||bytes%4)throw std::runtime_error("Invalid shader fixture");
    std::vector<u32> spirv(size_t(bytes)/4);file.seekg(0);file.read(reinterpret_cast<char*>(spirv.data()),bytes);
    Shader::Backend::DX12::SpirvConvertOptions options;options.stage=Shader::Backend::DX12::SpirvStage::Compute;
    auto dxil=Shader::Backend::DX12::ConvertSpirv(argv[2],spirv,options);
    if(!dxil.success)throw std::runtime_error(dxil.diagnostics);
    ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
    auto device=CreateHardwareDevice();
    ComPtr<ID3D12InfoQueue1> messages;DWORD message_cookie{};
    if(SUCCEEDED(device.As(&messages)))Check(messages->RegisterMessageCallback(
        [](D3D12_MESSAGE_CATEGORY,D3D12_MESSAGE_SEVERITY severity,D3D12_MESSAGE_ID,LPCSTR text,void*){
            if(severity<=D3D12_MESSAGE_SEVERITY_WARNING)fprintf(stderr,"D3D12: %s\n",text);
        },D3D12_MESSAGE_CALLBACK_FLAG_NONE,nullptr,&message_cookie));
    struct Mapping{
        HANDLE section{};std::byte* guest{};std::byte* alias{};
        ~Mapping(){if(guest)UnmapViewOfFile(guest);if(alias)UnmapViewOfFile(alias);if(section)CloseHandle(section);}
    } backing;
    backing.section=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,65536,nullptr);
    if(!backing.section)throw std::runtime_error("Cannot allocate physical guest backing");
    backing.guest=static_cast<std::byte*>(MapViewOfFileEx(backing.section,FILE_MAP_ALL_ACCESS,0,0,65536,reinterpret_cast<void*>(0x1300000000ULL)));
    backing.alias=static_cast<std::byte*>(MapViewOfFile(backing.section,FILE_MAP_ALL_ACCESS,0,0,65536));
    if(!backing.guest||!backing.alias)throw std::runtime_error("Cannot map guest/backing aliases");
    memset(backing.alias,0xee,65536);const u32 initial=7;memcpy(backing.alias,&initial,4);
    const auto address=reinterpret_cast<VAddr>(backing.guest);
    Rasterizer renderer(device.Get());renderer.MapMemory(address,65536);GuestTextureCache cache(device.Get());
    PageWatch* watcher{};std::atomic<u32> faults{},resolves{};
    const auto resolve=[&](GuestTextureCache::Entry& entry){
        std::lock_guard lock(renderer.ExecutionMutex());if(!entry.gpu_owned)return;
        renderer.Finish();const auto offset=entry.desc.address-address;
        std::vector<std::byte> result(backing.alias+offset,backing.alias+offset+entry.layout.guest_bytes);
        GuestImageReadback readback(device.Get(),entry,renderer.RecordingList());
        const auto fence=renderer.Flush();cache.Submitted(fence);readback.Submitted(fence);renderer.Finish();
        readback.Resolve(renderer.CompletedFence(),result);memcpy(backing.alias+offset,result.data(),result.size());
        entry.SetGpuOwned(false);watcher->Untrack(entry.desc.address,entry.layout.guest_bytes);++resolves;
    };
    PageWatch watch([&](u64 fault){++faults;for(auto& entry:cache.Entries())if(entry->gpu_owned&&
        fault>=entry->desc.address&&fault-entry->desc.address<entry->layout.guest_bytes)resolve(*entry);});watcher=&watch;
    cache.SetOwnershipAcquirer([&](auto& entry){return watch.Track(entry.desc.address,entry.layout.guest_bytes);});
    cache.SetOwnershipResolver([&](auto& entry){resolve(entry);renderer.RecordingList();});
    ShaderPipeline pipeline(device.Get(),argv[3],dxil.bytecode);
    std::unique_ptr<ShaderPipeline> sampled_pipeline;
    if(argc==5){
        std::ifstream input(argv[4],std::ios::binary|std::ios::ate);const auto length=input.tellg();
        if(length<=0||length%4)throw std::runtime_error("Invalid sampled fixture");
        std::vector<u32> code(size_t(length)/4);input.seekg(0);input.read(reinterpret_cast<char*>(code.data()),length);
        auto converted=Shader::Backend::DX12::ConvertSpirv(argv[2],code,options);
        if(!converted.success)throw std::runtime_error(converted.diagnostics);
        sampled_pipeline=std::make_unique<ShaderPipeline>(device.Get(),argv[3],converted.bytecode);
    }
    struct Retire{Rasterizer& renderer;~Retire(){try{renderer.Finish();}catch(...){}}} retire{renderer};
    Shader::Info info;info.sw_stage=Shader::SwStage::Compute;info.flattened_ud_buf.resize(8);
    Shader::ImageResource image{};image.is_written=true;
    image.sharp_fetch.summary=Shader::SharpFetch<AmdGpu::Image>::Summary::SingleLoad;image.sharp_fetch.offsets[0]=0;info.images.push_back(image);
    AmdGpu::Image sharp{};sharp.base_address=address>>8;sharp.width=sharp.height=sharp.pitch=31;
    sharp.type=u64(AmdGpu::ImageType::Color2D);sharp.data_format=4;sharp.num_format=4;sharp.tiling_index=13;
    sharp.dst_sel_x=4;sharp.dst_sel_y=5;sharp.dst_sel_z=6;sharp.dst_sel_w=7;
    memcpy(info.flattened_ud_buf.data(),&sharp,sizeof(sharp));
    ComPtr<ID3D12Fence> gate;Check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&gate)));
    renderer.WaitExternalFence(gate.Get(),1);
    auto commands=std::async(std::launch::async,[&]{for(u32 i=0;i<4;++i){
        ExecuteGuestBufferCompute(device.Get(),renderer,pipeline,info,{},ComputeInvocation{},&cache);
        const auto& entry=*cache.Entries().at(0);
        if(entry.pending||entry.fence!=renderer.SubmittedFence())throw std::runtime_error("Texture fence was not published at submission");
    }
        if(sampled_pipeline){
            Shader::Info sampled;sampled.sw_stage=Shader::SwStage::Compute;sampled.flattened_ud_buf.resize(16);
            auto target=sharp;target.base_address=(address+8192)>>8;
            memcpy(sampled.flattened_ud_buf.data(),&sharp,sizeof(sharp));
            memcpy(sampled.flattened_ud_buf.data()+8,&target,sizeof(target));
            auto source_image=image;source_image.is_written=false;
            sampled.images.push_back(source_image);auto target_image=image;target_image.sharp_fetch.offsets[0]=8;
            sampled.images.push_back(target_image);
            auto* original=cache.Entries().at(0).get();
            ExecuteGuestBufferCompute(device.Get(),renderer,*sampled_pipeline,sampled,{},ComputeInvocation{},&cache);
            if(cache.Entries().size()!=2||cache.Entries().at(0).get()!=original||!original->gpu_owned||resolves)
                throw std::runtime_error("Sampling owned UAV storage forced CPU resolution or duplicate upload");
            ExecuteGuestBufferCompute(device.Get(),renderer,pipeline,info,{},ComputeInvocation{},&cache);
            if(cache.Entries().size()!=2||!original->gpu_owned||resolves)
                throw std::runtime_error("Returning sampled storage to UAV forced CPU resolution");
        }
    });
    const bool returned=commands.wait_for(std::chrono::seconds(3))==std::future_status::ready;
    Check(gate->Signal(1));commands.get();
    if(!returned)throw std::runtime_error("Owned UAV dispatch waited for GPU completion");
    if(cache.Entries().size()!=(sampled_pipeline?2u:1u)||!cache.Entries()[0]->gpu_owned||cache.Entries()[0]->readback_pool||resolves)
        throw std::runtime_error("Owned dispatch used eager image readback");
    u32 value;memcpy(&value,backing.alias,4);if(value!=initial)throw std::runtime_error("Guest backing changed before ownership resolution");
    value=*reinterpret_cast<volatile const u32*>(backing.guest);
    if(value!=(sampled_pipeline?12u:11u)||faults!=1||resolves!=1)throw std::runtime_error("CPU read did not resolve all queued UAV writes: value="+std::to_string(value)+" faults="+std::to_string(faults.load())+" resolves="+std::to_string(resolves.load()));
    value=20;memcpy(backing.guest,&value,4);
    for(u32 i=0;i<3;++i)ExecuteGuestBufferCompute(device.Get(),renderer,pipeline,info,{},ComputeInvocation{},&cache);
    value=*reinterpret_cast<volatile const u32*>(backing.guest);
    if(value!=23||faults!=2||resolves!=2)throw std::runtime_error("CPU-edited image did not re-enter GPU ownership");
    if(sampled_pipeline){
        value=*reinterpret_cast<volatile const u32*>(backing.guest+8192);
        if(value!=111||faults!=3||resolves!=3)throw std::runtime_error("Owned SRV did not observe preceding UAV writes");
    }
    for(u32 i=4;i<4096;++i)if(backing.alias[i]!=std::byte{0xee})throw std::runtime_error("Owned retile changed untouched texture bytes");
    // Partial pages must retain the synchronous coherence fallback.
    sharp.base_address=(address+4096)>>8;sharp.width=sharp.height=sharp.pitch=7;
    memcpy(info.flattened_ud_buf.data(),&sharp,sizeof(sharp));value=40;memcpy(backing.alias+4096,&value,4);
    ExecuteGuestBufferCompute(device.Get(),renderer,pipeline,info,{},ComputeInvocation{},&cache);
    memcpy(&value,backing.guest+4096,4);
    if(value!=41||watch.Contains(address+4096))throw std::runtime_error("Partial-page UAV fallback lost CPU coherence");
    renderer.ValidateDebugMessages();puts("PASS: protected UAV images queue without GPU waits, resolve on CPU access and preserve partial-page fallback");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
