// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_pipeline_cache.h"
#include "video_core/renderer_d3d12/guest_compute.h"
#include "video_core/renderer_d3d12/capabilities.h"
#include "video_core/amdgpu/liverpool.h"
#include "common/serdes.h"
#include "core/emulator_settings.h"
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <bit>
#include <cmath>
#include <cstdio>

using namespace NativeDX12;
constexpr u32 Columns=32;
constexpr std::array<u32,12> Comparisons{1,2,3,4,5,6,8,9,10,11,12,13};
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
static std::vector<u32> Program(){
    std::vector<u32> words{0xBEEB03FF,127};
    const auto move=[&](u32 dst,u32 src){words.push_back(0x7e000200U|(dst<<17)|src);};
    const auto binary=[&](u32 op,u32 dst,u32 src0,u32 src1){
        words.push_back((op<<25)|(dst<<17)|(src1<<9)|src0);
    };
    const auto ternary=[&](u32 op,u32 dst,u32 src0,u32 src1,u32 src2,bool clamp=false){
        words.push_back(0xd0000000U|(op<<17)|(u32(clamp)<<11)|dst);
        words.push_back(src0|(src1<<9)|(src2<<18));
    };
    const auto access=[&](u32 op,u32 column,u32 value){
        words.push_back(0xe0002000U|(op<<18)|(column*4));
        words.push_back((128U<<24)|(value<<8));
    };
    for(u32 i=0;i<3;++i)access(12,i,1+i);
    words.push_back(0xbf8c007fU);
    binary(7,4,257,2);access(28,3,4);
    move(4,259);binary(6,4,257,2);access(28,4,4);
    ternary(320,4,257,258,259);access(28,5,4);
    ternary(337,4,257,258,259);access(28,6,4);
    ternary(340,4,257,258,259);access(28,7,4);
    ternary(343,4,257,258,259);access(28,8,4);
    ternary(259,4,257,128,128,true);access(28,9,4);
    for(u32 i=0;i<Comparisons.size();++i){
        words.push_back(0x7c000000U|(Comparisons[i]<<17)|(2U<<9)|257U);
        move(4,106);access(28,10+i,4);
    }
    binary(26,5,130,0);
    words.push_back(0xd8000000U|(13U<<17));words.push_back(5U|(1U<<8));
    binary(37,6,129,0);binary(27,6,159,6);binary(26,6,130,6);
    words.push_back(0xd8000000U|(54U<<17));words.push_back(6U|(7U<<24));
    words.push_back(0xbf8c007fU);access(28,22,7);
    words.push_back(0xbf810000U);return words;
}
static std::array<float,3> Inputs(u32 lane,u32 run){
    constexpr std::array<std::array<u32,3>,16> bits{{
        {0,0x7f800000,0x3f800000},{0x7f800000,0,0x40000000},
        {0x80000000,0xff800000,0xc0000000},{0x7fc01234,0,0x3f000000},
        {0,0x7fc05678,0x40400000},{0x7fc01234,0x40000000,0x40400000},
        {0x40000000,0x7fc05678,0x40400000},{0x40000000,0x40400000,0x7fc01234},
        {0x7fc01234,0x7fc05678,0x7fc09abc},{0xc0000000,0x40400000,0xbf800000},
        {0x3f000000,0x40000000,0x3e800000},{0x7f800000,0xff800000,0x3f800000},
        {0xff800000,0x7f800000,0xbf800000},{0x3f800000,0x3f800000,0x3f800000},
        {0x80000000,0,0x80000000},{0xbf800000,0xbf800000,0x40000000}
    }};
    const auto& b=bits[(lane+run*5)%bits.size()];
    return {std::bit_cast<float>(b[0]),std::bit_cast<float>(b[1]),std::bit_cast<float>(b[2])};
}
static bool Compare(u32 opcode,float a,float b){
    const bool nan=std::isnan(a)||std::isnan(b);
    switch(opcode){
    case 1:return !nan&&a<b;case 2:return !nan&&a==b;case 3:return !nan&&a<=b;
    case 4:return !nan&&a>b;case 5:return !nan&&a!=b;case 6:return !nan&&a>=b;
    case 8:return nan;case 9:return nan||a<b;case 10:return nan||a<=b;
    case 11:return nan||a>b;case 12:return nan||a!=b;case 13:return nan||a>=b;
    default:throw std::runtime_error("Unknown comparison oracle");
    }
}
static bool SameFloat(float actual,float expected,bool preserve_zero_sign){
    if(!preserve_zero_sign&&actual==0.f&&expected==0.f)return true;
    return std::isnan(expected)?std::isnan(actual):std::bit_cast<u32>(actual)==std::bit_cast<u32>(expected);
}
int main(int argc,char** argv){try{
    if(argc!=4)throw std::invalid_argument("Expected compiler, DXC and 32/64 arithmetic mode");
    const std::string mode=argv[3];if(mode!="32"&&mode!="64")throw std::invalid_argument("Unknown arithmetic mode");
    const u32 wave=mode=="64"?64:32,threads=wave;
    EmulatorSettings.SetPipelineCacheEnabled(false);EmulatorSettings.SetShaderCollect(false);EmulatorSettings.SetDumpShaders(false);
    _putenv_s("SHADPS4_NATIVE_SHADER_CACHE","off");
    auto device=HardwareDevice();const auto caps=Capabilities::Query(device.Get());
    if(caps.shader_model<D3D_SHADER_MODEL_6_6||!caps.wave_operations||caps.min_wave_lanes>wave||caps.max_wave_lanes<wave)
        throw std::runtime_error("Selected hardware cannot execute the requested compute wave");
    const auto converter=std::filesystem::path(argv[1]).wstring(),dxc=std::filesystem::path(argv[2]).wstring();
    if(wave==64&&!(Shader::Backend::DX12::ConverterNativeFeatures(converter)&Shader::Backend::DX12::NativeFeatureComputeWave64))
        throw std::runtime_error("Native compute wave64 export is absent");
    ComPtr<ID3D12InfoQueue> messages;Check(device.As(&messages));
    auto* memory=static_cast<std::byte*>(VirtualAlloc(nullptr,65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!memory)throw std::runtime_error("Arithmetic guest allocation failed");
    struct Allocation{void* value;~Allocation(){VirtualFree(value,0,MEM_RELEASE);}} allocation{memory};
    const auto words=Program();if(words.size()>256)throw std::runtime_error("GCN fixture overlaps binary metadata");
    alignas(256) std::array<u32,512> code{};std::copy(words.begin(),words.end(),code.begin());
    AmdGpu::BinaryInfo binary{};binary.signature=binary.signature_ref;binary.length=u32(words.size()*4);
    binary.shader_hash=0x41524954484d0000ULL|wave;memcpy(code.data()+256,&binary,sizeof(binary));
    auto gpu=std::make_unique<AmdGpu::Liverpool>();auto& program=gpu->GetCsRegs();program={};
    program.address=reinterpret_cast<u64>(code.data())>>8;program.num_thread_x.full=threads;
    program.num_thread_y.full=program.num_thread_z.full=1;program.dim_x=program.dim_y=program.dim_z=1;
    program.settings.num_user_regs=4;program.settings.num_vgprs=3;program.settings.num_sgprs=3;program.settings.lds_dwords=1;
    AmdGpu::Buffer buffer{};buffer.base_address=reinterpret_cast<u64>(memory);buffer.stride=Columns*4;
    buffer.num_records=threads;buffer.data_format=4;buffer.num_format=4;
    buffer.dst_sel_x=4;buffer.dst_sel_y=5;buffer.dst_sel_z=6;buffer.dst_sel_w=7;
    memcpy(program.user_data.data(),&buffer,sizeof(buffer));
    Shader::Profile profile{};profile.supported_spirv=0x00010500;profile.subgroup_size=32;
    profile.native_compute_wave_size=wave;profile.needs_lds_barriers=true;profile.max_shared_memory_size=32768;
    profile.support_int64=true;GuestShaderFrontend frontend(profile);auto& stage=frontend.BuildCompute(*gpu);
    if(stage.spirv.empty()||stage.info.translation_failed)throw std::runtime_error("Arithmetic GCN translation failed");
    Serialization::Archive serialized;frontend.SerializeStage(serialized,stage);auto saved=serialized.TakeOff();
    GuestShaderFrontend restored(profile);Serialization::Archive input{std::vector<u8>(saved)};auto& reloaded=restored.RestoreStage(input);
    if(reloaded.spirv!=stage.spirv||!input.IsEoS())throw std::runtime_error("Arithmetic metadata restore failed");
    Shader::Backend::DX12::SpirvConvertOptions options;options.stage=Shader::Backend::DX12::SpirvStage::Compute;
    options.shader_model_minor=6;options.compute_wave_size=wave;options.native_storage_buffer_uavs=true;
    GuestShaderCache shaders(converter,options);GuestPipelineCache pipelines(device.Get(),dxc,frontend,shaders);
    const auto& pipeline=pipelines.Compute(stage);Rasterizer rasterizer(device.Get());
    rasterizer.MapMemory(reinterpret_cast<u64>(memory),65536);
    for(u32 run=0;run<3;++run){
        std::vector<std::byte> original(65536,std::byte{0x5a});
        for(u32 lane=0;lane<threads;++lane){const auto values=Inputs(lane,run);
            memcpy(original.data()+lane*Columns*4,values.data(),values.size()*4);}
        memcpy(memory,original.data(),original.size());rasterizer.InvalidateMemory(reinterpret_cast<u64>(memory),65536);
        ComputeInvocation invocation{};invocation.shared_bytes=stage.runtime.hw.cs.shared_memory_size;
        ExecuteGuestBufferCompute(device.Get(),rasterizer,pipeline,stage.info,stage.bindings_start,invocation);
        rasterizer.Finish();rasterizer.ReadMemory(reinterpret_cast<u64>(memory),threads*Columns*4);
        for(u32 lane=0;lane<threads;++lane){
            const auto in=Inputs(lane,run);const float a=in[0],b=in[1],c=in[2];
            const float product=a==0||b==0?0.f:a*b;
            const float minimum=std::fmin(a,std::fmin(b,c)),maximum=std::fmax(a,std::fmax(b,c));
            const float median=std::isnan(a)||std::isnan(b)||std::isnan(c)?minimum:
                std::fmax(std::fmin(a,b),std::fmin(std::fmax(a,b),c));
            const std::array<float,7> expected{product,product+c,product+c,minimum,maximum,median,
                std::fmin(std::fmax(a+0.f,0.f),1.f)};
            for(u32 column=0;column<expected.size();++column){float actual{};
                memcpy(&actual,memory+(lane*Columns+3+column)*4,4);
                if(!SameFloat(actual,expected[column],column<3))throw std::runtime_error("GCN arithmetic GPU value mismatch at lane "+std::to_string(lane)+", column "+std::to_string(column));
            }
            for(u32 i=0;i<Comparisons.size();++i){u32 mask{};
                for(u32 bit=0;bit<32;++bit){const auto v=Inputs(bit,run);if(Compare(Comparisons[i],v[0],v[1]))mask|=1U<<bit;}
                u32 actual{};memcpy(&actual,memory+(lane*Columns+10+i)*4,4);
                if(actual!=mask)throw std::runtime_error("Ordered/unordered GCN ballot mismatch");
            }
            const auto neighbor=Inputs((lane+1)%32,run);float actual{};memcpy(&actual,memory+(lane*Columns+22)*4,4);
            if(std::bit_cast<u32>(actual)!=std::bit_cast<u32>(neighbor[0]))throw std::runtime_error("GCN LDS neighbor visibility mismatch");
            memcpy(original.data()+lane*Columns*4+3*4,memory+lane*Columns*4+3*4,20*4);
        }
        if(memcmp(memory,original.data(),original.size()))throw std::runtime_error("Arithmetic fixture corrupted guest input or padding");
    }
    for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T size{};Check(messages->GetMessage(i,nullptr,&size));
        std::vector<std::byte> storage(size);auto* message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());
        Check(messages->GetMessage(i,message,&size));if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);}
    printf("PASS: actual GCN legacy MUL/MAC/MAD, NaN MIN3/MAX3/MED3/saturate, ordered/unordered ballots, LDS neighbors, cold/warm/CPU changes and metadata, wave %u\n",wave);
    return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
