// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_compute.h"
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "shader_recompiler/backend/dxil/spirv_converter.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"
#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <algorithm>
#include <cmath>
#include <cstring>
using namespace NativeDX12;
namespace {
std::array<float,4> Decode(u16 word,u32 format){
    switch(format){
    case 16:return {float(word>>11)/31,float((word>>5)&63)/63,float(word&31)/31,1};
    case 17:return {float((word>>10)&31)/31,float((word>>5)&31)/31,float(word&31)/31,float(word>>15)};
    case 19:return {float((word>>4)&15)/15,float((word>>8)&15)/15,float(word>>12)/15,float(word&15)/15};
    default:throw std::invalid_argument("Unexpected packed oracle format");
    }
}
u16 Encode(const std::array<float,4>& color,u32 format){
    const auto q=[&](u32 channel,u32 maximum){return u32(std::nearbyint(std::clamp(color[channel],0.f,1.f)*maximum));};
    switch(format){
    case 16:return u16((q(0,31)<<11)|(q(1,63)<<5)|q(2,31));
    case 17:return u16((q(3,1)<<15)|(q(0,31)<<10)|(q(1,31)<<5)|q(2,31));
    case 19:return u16((q(2,15)<<12)|(q(1,15)<<8)|(q(0,15)<<4)|q(3,15));
    default:throw std::invalid_argument("Unexpected packed oracle format");
    }
}
Shader::ImageResource ImageResource(u32 offset,bool storage,bool msaa,u32 levels){
    Shader::ImageResource image{};image.is_written=storage;image.is_array=true;
    image.native_msaa_storage=storage&&msaa;
    if(storage&&!msaa&&levels>1)image.mip_fallback_mode=Shader::MipStorageFallbackMode::DynamicIndex;
    image.sharp_fetch.summary=Shader::SharpFetch<AmdGpu::Image>::Summary::SingleLoad;
    image.sharp_fetch.offsets[0]=offset;return image;
}
std::vector<u32> Emit(Shader::Info& info,bool read_only,u32 samples,u32 levels){
    using namespace Shader::Backend::SPIRV;
    Shader::Profile profile{};profile.supported_spirv=0x00010500;profile.native_coherent_images=true;
    Shader::RuntimeInfo runtime{};runtime.sw_stage=Shader::SwStage::Compute;
    Shader::Backend::Bindings binding{};EmitContext c(profile,runtime,info,binding);
    c.AddCapability(spv::Capability::StorageImageReadWithoutFormat);
    c.AddCapability(spv::Capability::StorageImageWriteWithoutFormat);
    const auto fn=c.OpFunction(c.void_id,spv::FunctionControlMask::MaskNone,c.TypeFunction(c.void_id));c.AddLabel();
    const auto barrier=[&]{c.OpMemoryBarrier(c.ConstU32(u32(spv::Scope::Device)),
        c.ConstU32(u32(spv::MemorySemanticsMask::AcquireRelease|spv::MemorySemanticsMask::ImageMemory)));};
    for(u32 mip=0;mip<levels;++mip)for(u32 layer=0;layer<3;++layer)for(u32 sample=0;sample<samples;++sample){
        const auto lod=samples>1?Sirit::Id{}:c.ConstU32(mip);
        const auto ms=samples>1?c.ConstU32(sample):Sirit::Id{};
        // Match ResourcePatchingPass: single-view UAVs have no LOD operand;
        // native MSAA storage selects a sample descriptor, never a mip.
        const auto storage_lod=levels>1?lod:Sirit::Id{};
        const auto coord=[&](u32 x){return c.ConstU32(x,0U,layer);};
        if(read_only){
            for(u32 x=0;x<8;++x){
                const auto value=EmitImageRead(c,nullptr,0,coord(x),lod,ms);
                EmitImageWrite(c,nullptr,1,coord(sample*8+x),storage_lod,{},value);
            }
        }else{
            const auto initial=EmitImageRead(c,nullptr,0,coord(0),storage_lod,ms);
            const auto color=c.ConstF32(0.25f,0.5f,0.75f,1.f);
            EmitImageWrite(c,nullptr,0,coord(1),storage_lod,ms,color);barrier();
            const auto written=EmitImageRead(c,nullptr,0,coord(1),storage_lod,ms);
            EmitImageWrite(c,nullptr,0,coord(2),storage_lod,ms,written);
            EmitImageWrite(c,nullptr,0,coord(3),storage_lod,ms,initial);
            for(u32 channel=0;channel<4;++channel){
                const auto component=c.OpCompositeExtract(c.F32[1],initial,channel);
                const auto replicated=c.OpCompositeConstruct(c.F32[4],component,component,component,component);
                EmitImageWrite(c,nullptr,0,coord(4+channel),storage_lod,ms,replicated);
            }
        }
    }
    c.OpReturn();c.OpFunctionEnd();c.AddEntryPoint(spv::ExecutionModel::GLCompute,fn,"main",c.interfaces);
    c.AddExecutionMode(fn,spv::ExecutionMode::LocalSize,1U,1U,1U);return c.Assemble();
}
}
int RunNativePackedImage(const wchar_t* converter,const wchar_t* dxc,std::wstring_view mode){
    const u32 raw_format=u32(std::stoi(std::wstring(mode.substr(7,2))));
    const bool msaa=mode.ends_with(L"-msaa"),linear=mode.ends_with(L"-linear");
    if(raw_format<16||raw_format>19)throw std::invalid_argument("Invalid packed fixture mode");
    ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> adapter;
        if(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter))==DXGI_ERROR_NOT_FOUND)break;
        DXGI_ADAPTER_DESC1 description{};Check(adapter->GetDesc1(&description));if(description.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
        if(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))break;
    }
    if(!device)throw std::runtime_error("No packed fixture hardware device");
    struct Allocation{void* p;~Allocation(){if(p)VirtualFree(p,0,MEM_RELEASE);}} owner{
        VirtualAlloc(reinterpret_cast<void*>(0x1400000000ULL),0x200000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)};
    if(!owner.p)throw std::runtime_error("Cannot allocate packed guest fixture");
    const auto address=reinterpret_cast<VAddr>(owner.p);
    AmdGpu::Image sharp{};sharp.base_address=address>>8;sharp.width=16;sharp.height=8;sharp.pitch=23;
    sharp.depth=2;sharp.type=u64(msaa?AmdGpu::ImageType::Color2DMsaaArray:AmdGpu::ImageType::Color2DArray);
    sharp.last_level=1;sharp.last_array=2;sharp.data_format=raw_format;sharp.num_format=0;sharp.tiling_index=linear?8:13;
    sharp.dst_sel_x=4;sharp.dst_sel_y=5;sharp.dst_sel_z=6;sharp.dst_sel_w=7;
    const auto desc=DescribeGuestTexture(sharp);const auto layout=LayoutGuestTexture(desc);
    auto memory=std::span(static_cast<std::byte*>(owner.p),size_t(layout.guest_bytes));
    std::fill(memory.begin(),memory.end(),std::byte{0xee});std::vector<std::byte> expected(memory.begin(),memory.end());
    for(u32 mip=0;mip<desc.levels;++mip)for(u32 layer=0;layer<desc.layers;++layer)for(u32 sample=0;sample<desc.samples;++sample){
        const u16 initial=u16(0x93b5U+mip*113+layer*431+sample*1023);
        const auto decoded=Decode(initial,u32(sharp.GetDataFmt()));
        const auto store=[&](u32 x,u16 word){memcpy(expected.data()+GuestPixelOffset(desc,layout,mip,layer,x,0,sample),&word,2);};
        memcpy(memory.data()+GuestPixelOffset(desc,layout,mip,layer,0,0,sample),&initial,2);store(0,initial);
        store(1,Encode({0.25f,0.5f,0.75f,1.f},u32(sharp.GetDataFmt())));store(2,Encode({0.25f,0.5f,0.75f,1.f},u32(sharp.GetDataFmt())));
        store(3,initial);
        for(u32 channel=0;channel<4;++channel){const auto value=decoded[channel];store(4+channel,Encode({value,value,value,value},u32(sharp.GetDataFmt())));}
    }
    Rasterizer rasterizer(device.Get());rasterizer.MapMemory(address,0x200000);GuestTextureCache textures(device.Get());
    Shader::Info info;info.hw_stage=Shader::HwStage::Compute;info.sw_stage=Shader::SwStage::Compute;info.flattened_ud_buf.resize(16);
    memcpy(info.flattened_ud_buf.data(),&sharp,sizeof(sharp));info.images.push_back(ImageResource(0,true,msaa,desc.levels));
    Shader::Backend::DX12::SpirvConvertOptions options;options.stage=Shader::Backend::DX12::SpirvStage::Compute;
    auto dxil=Shader::Backend::DX12::ConvertSpirv(converter,Emit(info,false,desc.samples,desc.levels),options);
    if(!dxil.success)throw std::runtime_error(dxil.diagnostics);
    ShaderPipeline storage(device.Get(),dxc,dxil.bytecode);ExecuteGuestBufferCompute(device.Get(),rasterizer,storage,info,{},ComputeInvocation{},&textures);
    if(!std::equal(memory.begin(),memory.end(),expected.begin()))throw std::runtime_error("Packed shader contents/mips/layers/samples/padding mismatch");
    auto output=sharp;output.base_address=(address+0x100000)>>8;output.width=33;output.pitch=39;output.last_level=desc.levels-1;
    output.type=u64(AmdGpu::ImageType::Color2DArray);output.data_format=14;output.num_format=7;
    const auto output_desc=DescribeGuestTexture(output);
    const auto output_layout=LayoutGuestTexture(output_desc);
    auto output_bytes=std::span(static_cast<std::byte*>(owner.p)+0x100000,size_t(output_layout.guest_bytes));
    std::fill(output_bytes.begin(),output_bytes.end(),std::byte{0xcd});std::vector<std::byte> output_expected(output_bytes.begin(),output_bytes.end());
    for(u32 mip=0;mip<desc.levels;++mip)for(u32 layer=0;layer<3;++layer)for(u32 sample=0;sample<desc.samples;++sample)for(u32 x=0;x<8;++x){
        u16 word{};memcpy(&word,expected.data()+GuestPixelOffset(desc,layout,mip,layer,x,0,sample),2);
        const auto value=sharp.DstSelect().Apply(Decode(word,u32(sharp.GetDataFmt())));
        memcpy(output_expected.data()+GuestPixelOffset(output_desc,output_layout,mip,layer,sample*8+x,0),value.data(),16);
    }
    info.images.clear();info.images.push_back(ImageResource(0,false,msaa,desc.levels));
    memcpy(info.flattened_ud_buf.data()+8,&output,sizeof(output));info.images.push_back(ImageResource(8,true,false,output_desc.levels));
    dxil=Shader::Backend::DX12::ConvertSpirv(converter,Emit(info,true,desc.samples,desc.levels),options);
    if(!dxil.success)throw std::runtime_error(dxil.diagnostics);
    ShaderPipeline sampled(device.Get(),dxc,dxil.bytecode);ExecuteGuestBufferCompute(device.Get(),rasterizer,sampled,info,{},ComputeInvocation{},&textures);
    for(u32 mip=0;mip<desc.levels;++mip)for(u32 layer=0;layer<3;++layer)for(u32 sample=0;sample<desc.samples;++sample)for(u32 x=0;x<8;++x){
        const auto offset=GuestPixelOffset(output_desc,output_layout,mip,layer,sample*8+x,0);
        std::array<float,4> observed{},wanted{};memcpy(observed.data(),output_bytes.data()+offset,16);memcpy(wanted.data(),output_expected.data()+offset,16);
        for(u32 channel=0;channel<4;++channel)if(!std::isfinite(observed[channel])||std::abs(observed[channel]-wanted[channel])>2e-7f)
            throw std::runtime_error(fmt::format("Packed sampled mismatch mip={} layer={} sample={} x={} channel={} observed={} expected={}",mip,layer,sample,x,channel,observed[channel],wanted[channel]));
        memcpy(output_bytes.data()+offset,wanted.data(),16);
    }
    if(!std::equal(output_bytes.begin(),output_bytes.end(),output_expected.begin()))throw std::runtime_error("Packed sampled output changed padding");
    if(GetModuleHandleW(L"vulkan-1.dll"))throw std::runtime_error("Unexpected Vulkan packed execution");
    puts("PASS: packed native storage quantization/read components and sampled views preserve guest words, mips, layers, samples and padding");return 0;
}
