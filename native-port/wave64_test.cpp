// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_pipeline_cache.h"
#include "video_core/renderer_d3d12/guest_compute.h"
#include "video_core/renderer_d3d12/capabilities.h"
#include "video_core/amdgpu/liverpool.h"
#include "common/serdes.h"
#include "core/emulator_settings.h"
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <cstdio>

using namespace NativeDX12;
static ComPtr<ID3D12Device> HardwareDevice(){
    ComPtr<ID3D12Debug> debug;Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> adapter;
        const auto hr=factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter));
        if(hr==DXGI_ERROR_NOT_FOUND)break;Check(hr);DXGI_ADAPTER_DESC1 desc{};Check(adapter->GetDesc1(&desc));
        if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;ComPtr<ID3D12Device> device;
        if(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))return device;
    }
    throw std::runtime_error("No hardware D3D12 device");
}
static std::vector<u32> Program(bool dynamic,bool divergent){
    std::vector<u32> words{0xBEEB03FF,127};
    const auto vop1=[&](u32 dst,u32 src){words.push_back(0x7e000200U|(dst<<17)|src);};
    const auto vop2=[&](u32 op,u32 dst,u32 src0,u32 src1){words.push_back((op<<25)|(dst<<17)|(src1<<9)|src0);};
    const auto vop3=[&](u32 op,u32 dst,u32 src0,u32 src1){
        words.push_back(0xd0000000U|(op<<17)|dst);words.push_back(src0|(src1<<9));
    };
    const auto store=[&](u32 column,u32 value){
        words.push_back(0xe0002000U|(28U<<18)|(column*4));
        words.push_back((128U<<24)|(value<<8));
    };
    vop3(291,1,193,128);vop3(292,1,193,257);store(0,1);
    vop2(1,8,256,191);vop1(2,8);store(1,2);
    vop2(27,4,191,0);
    words.push_back(0x7c000000U|(193U<<17)|(4U<<9)|160U);
    vop1(5,106);vop1(6,107);store(2,5);store(3,6);
    vop3(291,7,106,128);vop3(292,7,107,263);store(4,7);
    vop2(1,8,256,dynamic?4U:175U);vop1(2,8);store(5,2);
    if(divergent){
        words.push_back(0x7c000000U|(193U<<17)|(4U<<9)|160U);
        words.push_back(0xbe800000U|(10U<<16)|(36U<<8)|106U);
        const auto branch=words.size();words.push_back(0xbf880000U);
        words.push_back(0x7e000400U|(12U<<17)|256U);
        vop1(2,12);store(6,2);
        words.push_back(0x7c000000U|(194U<<17)|(4U<<9)|260U);
        vop1(6,107);store(7,6);
        words[branch]|=u32(words.size()-branch-1);
        words.push_back(0xbe800000U|(126U<<16)|(4U<<8)|10U);
    }
    words.push_back(0xbf810000U);return words;
}
int main(int argc,char** argv){try{
    if(argc!=4)throw std::invalid_argument("Expected converter, DXC and wave fixture mode");
    const std::string mode=argv[3];
    if(mode!="32-uniform"&&mode!="64-uniform"&&mode!="64-dynamic"&&mode!="64-divergent")
        throw std::invalid_argument("Unknown wave fixture mode");
    const u32 wave=mode.starts_with("64-")?64:32;
    const bool dynamic=mode=="64-dynamic",divergent=mode=="64-divergent";
    EmulatorSettings.SetPipelineCacheEnabled(false);EmulatorSettings.SetShaderCollect(false);EmulatorSettings.SetDumpShaders(false);
    _putenv_s("SHADPS4_NATIVE_SHADER_CACHE","off");
    auto device=HardwareDevice();const auto caps=Capabilities::Query(device.Get());
    if(caps.shader_model<D3D_SHADER_MODEL_6_6||!caps.wave_operations||caps.min_wave_lanes>wave||caps.max_wave_lanes<wave)
        throw std::runtime_error("Selected hardware cannot execute the requested wave fixture");
    const auto converter=std::filesystem::path(argv[1]).wstring(),dxc=std::filesystem::path(argv[2]).wstring();
    if(wave==64&&!(Shader::Backend::DX12::ConverterNativeFeatures(converter)&Shader::Backend::DX12::NativeFeatureComputeWave64))
        throw std::runtime_error("Compiler lacks the native compute wave64 export");
    ComPtr<ID3D12InfoQueue> messages;Check(device.As(&messages));
    auto* memory=static_cast<std::byte*>(VirtualAlloc(nullptr,65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!memory)throw std::runtime_error("Wave fixture guest allocation failed");
    struct Allocation{void* value;~Allocation(){VirtualFree(value,0,MEM_RELEASE);}} allocation{memory};
    constexpr u32 Threads=128,Columns=8,GuestBytes=Threads*Columns*sizeof(u32);
    const auto words=Program(dynamic,divergent);if(words.size()>256)throw std::runtime_error("Fixture overlaps binary metadata");
    alignas(256) std::array<u32,512> code{};std::copy(words.begin(),words.end(),code.begin());
    AmdGpu::BinaryInfo binary{};binary.signature=binary.signature_ref;binary.length=u32(words.size()*4);
    binary.shader_hash=0x5741564536340000ULL|wave|(u64(dynamic)<<8)|(u64(divergent)<<9);
    memcpy(code.data()+256,&binary,sizeof(binary));
    auto gpu=std::make_unique<AmdGpu::Liverpool>();auto& program=gpu->GetCsRegs();program={};
    program.address=reinterpret_cast<u64>(code.data())>>8;program.num_thread_x.full=Threads;
    program.num_thread_y.full=program.num_thread_z.full=1;program.dim_x=program.dim_y=program.dim_z=1;
    program.settings.num_user_regs=5;program.settings.num_vgprs=3;program.settings.num_sgprs=3;
    AmdGpu::Buffer buffer{};buffer.base_address=reinterpret_cast<u64>(memory);buffer.stride=Columns*4;
    buffer.num_records=Threads;buffer.data_format=4;buffer.num_format=4;
    buffer.dst_sel_x=4;buffer.dst_sel_y=5;buffer.dst_sel_z=6;buffer.dst_sel_w=7;
    memcpy(program.user_data.data(),&buffer,sizeof(buffer));program.user_data[4]=47;
    Shader::Profile profile{};profile.supported_spirv=0x00010500;profile.subgroup_size=32;
    profile.native_compute_wave_size=wave;profile.needs_lds_barriers=true;profile.max_shared_memory_size=32768;
    profile.support_int64=true;
    GuestShaderFrontend frontend(profile);auto& stage=frontend.BuildCompute(*gpu);
    if(stage.spirv.empty()||stage.info.translation_failed)throw std::runtime_error("Wave GCN translation failed");
    const auto id=stage.id;if(&frontend.BuildCompute(*gpu)!=&stage||stage.id!=id)
        throw std::runtime_error("Wave specialization missed the frontend cache");
    Serialization::Archive serialized;frontend.SerializeStage(serialized,stage);auto saved=serialized.TakeOff();
    GuestShaderFrontend restored(profile);Serialization::Archive input{std::vector<u8>(saved)};auto& reloaded=restored.RestoreStage(input);
    if(reloaded.spirv!=stage.spirv||!input.IsEoS())throw std::runtime_error("Wave stage metadata restore failed");
    Shader::Backend::DX12::SpirvConvertOptions options;options.stage=Shader::Backend::DX12::SpirvStage::Compute;
    options.shader_model_minor=6;options.compute_wave_size=wave;options.native_storage_buffer_uavs=true;
    GuestShaderCache shaders(converter,options);GuestPipelineCache pipelines(device.Get(),dxc,frontend,shaders);
    const auto& pipeline=pipelines.Compute(stage);Rasterizer rasterizer(device.Get());
    rasterizer.MapMemory(reinterpret_cast<u64>(memory),65536);
    std::vector<std::byte> expected(65536,std::byte{0x5a});
    for(u32 local=0;local<Threads;++local){
        const u32 lane=local%64,base=local-lane;
        const u32 values[]{lane,base+63,0,0xfffffffeU,lane>33?lane-33:0,base+47,base+33,0xfffffffeU};
        for(u32 column=0;column<Columns;++column){
            if(column>=6&&(!divergent||lane<=32))continue;
            memcpy(expected.data()+(local*Columns+column)*4,&values[column],4);
        }
    }
    for(unsigned run=0;run<2;++run){
        std::fill_n(memory,65536,std::byte{0x5a});rasterizer.InvalidateMemory(reinterpret_cast<u64>(memory),65536);
        ComputeInvocation invocation{};invocation.shared_bytes=stage.runtime.hw.cs.shared_memory_size;
        ExecuteGuestBufferCompute(device.Get(),rasterizer,pipeline,stage.info,stage.bindings_start,invocation);
        rasterizer.Finish();rasterizer.ReadMemory(reinterpret_cast<u64>(memory),GuestBytes);
        if(memcmp(memory,expected.data(),65536))throw std::runtime_error("Wave GPU contents or guest padding mismatch");
    }
    for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T size{};Check(messages->GetMessage(i,nullptr,&size));
        std::vector<std::byte> storage(size);auto* message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());
        Check(messages->GetMessage(i,message,&size));if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)
            throw std::runtime_error(message->pDescription);
    }
    printf("PASS: GCN compute %u lanes, 128 threads, ballot high bits, masked bit count, lane 63/47, cold/warm dispatch, metadata restore%s\n",wave,divergent?", divergent EXEC":"");
    return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
