// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_compute.h"
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "shader_recompiler/backend/dxil/spirv_converter.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"
#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include <d3d12sdklayers.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
using namespace NativeDX12;
namespace {
bool Integer(u32 number){return number==4||number==5;}
std::array<u32,4> Color(u32 number,u32 pattern){
    std::array<u32,4> result{};
    const float values[]{-2.f,-1.f,-.5f,-.25f,0.f,.25f,.5f,1.f,2.f};
    for(u32 c=0;c<4;++c){const auto value=values[(pattern+c*2)%std::size(values)];
        if(Integer(number)){
            const s32 maximum=c==3?(number==5?1:3):(number==5?511:1023);
            const s32 minimum=number==5?(c==3?-2:-512):0;
            const s32 candidates[]{minimum,minimum+1,0,maximum/2,maximum};
            result[c]=u32(candidates[(pattern+c)%std::size(candidates)]);
        }else result[c]=std::bit_cast<u32>(value);
    }
    return result;
}
u32 Encode(const std::array<u32,4>& value,u32 number,u32 format=9){
    if(format==4)return value[0];
    u32 word{};for(u32 c=0;c<4;++c){const u32 bits=format==10?8:c==3?2:10,mask=(1u<<bits)-1;u32 encoded;
        if(number==4)encoded=std::min(value[c],mask);
        else if(number==5)encoded=u32(std::clamp(std::bit_cast<s32>(value[c]),-s32(1u<<(bits-1)),s32((1u<<(bits-1))-1)));
        else{const bool signed_value=number==1;
            const float scaled=std::clamp(std::bit_cast<float>(value[c]),signed_value?-1.f:0.f,1.f)*
                float((1u<<(bits-u32(signed_value)))-1);
            const s32 lower=s32(std::floor(scaled));const float fraction=scaled-float(lower);
            encoded=u32(lower+s32(fraction>.5f||(fraction==.5f&&(lower&1))));
        }
        word|=(encoded&mask)<<(c*(format==10?8:10));
    }return word;
}
std::array<u32,4> Decode(u32 word,u32 number,u32 format=9){
    if(format==4)return {word,0U,0U,Integer(number)?1U:std::bit_cast<u32>(1.f)};
    std::array<u32,4> result{};
    for(u32 c=0;c<4;++c){const u32 bits=format==10?8:c==3?2:10,mask=(1u<<bits)-1;
        const u32 raw=(word>>(c*(format==10?8:10)))&mask;const bool signed_value=number==1||number==5;
        const s32 value=signed_value&&raw&(1u<<(bits-1))?s32(raw)-s32(1u<<bits):s32(raw);
        if(Integer(number))result[c]=u32(value);
        else result[c]=std::bit_cast<u32>(std::max(float(value)/float((1u<<(bits-u32(signed_value)))-1),signed_value?-1.f:0.f));
    }return result;
}
Shader::ImageResource ImageResource(u32 offset,u32 levels,bool msaa,bool array){
    Shader::ImageResource image{};image.is_written=true;image.is_array=array;image.native_msaa_storage=msaa;
    if(!msaa&&levels>1)image.mip_fallback_mode=Shader::MipStorageFallbackMode::DynamicIndex;
    image.sharp_fetch.summary=Shader::SharpFetch<AmdGpu::Image>::Summary::SingleLoad;
    image.sharp_fetch.offsets[0]=offset;return image;
}
std::vector<u32> Emit(Shader::Info& info,const GuestTextureDesc& d,u32 number,u32 phase){
    using namespace Shader::Backend::SPIRV;
    Shader::Profile profile{};profile.supported_spirv=0x00010500;profile.native_coherent_images=true;
    Shader::RuntimeInfo runtime{};runtime.sw_stage=Shader::SwStage::Compute;
    Shader::Backend::Bindings bindings{};EmitContext c(profile,runtime,info,bindings);
    c.AddCapability(spv::Capability::StorageImageReadWithoutFormat);c.AddCapability(spv::Capability::StorageImageWriteWithoutFormat);
    const auto fn=c.OpFunction(c.void_id,spv::FunctionControlMask::MaskNone,c.TypeFunction(c.void_id));c.AddLabel();
    for(u32 mip=0;mip<d.levels;++mip){const auto slices=d.volume_depth?std::max(d.volume_depth>>mip,1u):d.layers;
        const auto count=std::min(9u,(std::max(d.width>>mip,1u)-1)/2);
        for(u32 slice=0;slice<slices;++slice)for(u32 sample=0;sample<d.samples;++sample){
            const auto lod=c.ConstU32(mip),ms=d.samples>1?c.ConstU32(sample):Sirit::Id{};
            const auto coord=[&](u32 x){return d.one_dimensional?c.ConstU32(x,slice):c.ConstU32(x,0u,slice);};
            EmitImageWrite(c,nullptr,1,coord(0),lod,ms,EmitImageRead(c,nullptr,0,coord(0),lod,ms));
            for(u32 i=0;i<count;++i){const auto bits=Color(number,i+phase);const auto value=c.OpBitcast(c.F32[4],c.ConstU32(bits[0],bits[1],bits[2],bits[3]));
                EmitImageWrite(c,nullptr,0,coord(1+i*2),lod,ms,value);
                c.OpMemoryBarrier(c.ConstU32(u32(spv::Scope::Device)),c.ConstU32(u32(spv::MemorySemanticsMask::AcquireRelease|spv::MemorySemanticsMask::ImageMemory)));
                const auto loaded=EmitImageRead(c,nullptr,0,coord(1+i*2),lod,ms);
                EmitImageWrite(c,nullptr,0,coord(2+i*2),lod,ms,loaded);
                EmitImageWrite(c,nullptr,1,coord(i+1),lod,ms,loaded);
            }
        }
    }
    c.OpReturn();c.OpFunctionEnd();c.AddEntryPoint(spv::ExecutionModel::GLCompute,fn,"main",c.interfaces);
    c.AddExecutionMode(fn,spv::ExecutionMode::LocalSize,1u,1u,1u);return c.Assemble();
}
int RunDisjointPacked10(const wchar_t* converter,const wchar_t* dxc,u32 number,bool mip_alias,bool coherent_alias=false,u32 read_format=9,u32 mixed_writer_format=0){
    ComPtr<ID3D12Debug> debug;Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    ComPtr<ID3D12Device> device;Check(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)));
    struct Allocation{void* p;~Allocation(){if(p)VirtualFree(p,0,MEM_RELEASE);}} allocation{
        VirtualAlloc(reinterpret_cast<void*>(0x1500000000ull),0x200000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)};
    if(!allocation.p)throw std::runtime_error("Cannot allocate packed10 disjoint fixture memory");
    const auto address=reinterpret_cast<VAddr>(allocation.p);
    AmdGpu::Image sharp{};sharp.base_address=address>>8;sharp.width=16;sharp.height=8;sharp.pitch=23;
    sharp.depth=sharp.last_array=2;sharp.last_level=2;sharp.type=u64(AmdGpu::ImageType::Color2DArray);
    sharp.data_format=read_format;sharp.num_format=5;sharp.tiling_index=13;
    sharp.dst_sel_x=4;sharp.dst_sel_y=5;sharp.dst_sel_z=6;sharp.dst_sel_w=7;
    auto desc=DescribeGuestTexture(sharp);desc.writable=true;
    desc.native_raw32=mixed_writer_format&&(read_format==4||read_format==10);
    const auto layout=LayoutGuestTexture(desc);
    auto output=sharp;output.base_address=(address+0x100000)>>8;output.data_format=14;
    output.num_format=Integer(number)?4:7;output.last_level=0;
    auto output_desc=DescribeGuestTexture(output);output_desc.writable=true;const auto output_layout=LayoutGuestTexture(output_desc);
    auto memory=std::span(static_cast<std::byte*>(allocation.p),size_t(layout.guest_bytes));
    auto output_bytes=std::span(static_cast<std::byte*>(allocation.p)+0x100000,size_t(output_layout.guest_bytes));
    std::fill(memory.begin(),memory.end(),std::byte{0xee});std::fill(output_bytes.begin(),output_bytes.end(),std::byte{0xcd});
    std::vector<std::byte> expected(memory.begin(),memory.end()),expected_output(output_bytes.begin(),output_bytes.end());
    for(u32 mip=0;mip<desc.levels;++mip)for(u32 layer=0;layer<desc.layers;++layer)
    for(u32 y=0;y<std::max(desc.height>>mip,1u);++y)for(u32 x=0;x<std::max(desc.width>>mip,1u);++x){
        const u32 word=0xa0080200u+mip*137+layer*211+x*31+y*67;
        const auto at=GuestPixelOffset(desc,layout,mip,layer,x,y);memcpy(memory.data()+at,&word,4);memcpy(expected.data()+at,&word,4);
    }
    Rasterizer raster(device.Get());raster.MapMemory(address,0x200000);GuestTextureCache textures(device.Get());
    Shader::Backend::DX12::SpirvConvertOptions options;options.stage=Shader::Backend::DX12::SpirvStage::Compute;
    for(u32 pass=0;pass<3;++pass){
        auto read=sharp,write=sharp;read.num_format=number;write.num_format=coherent_alias?
            ((number==1||number==5)?(pass%2?4:0):(pass%2?1:5)):(pass%2?1:5);
        if(read_format!=9){
            write.data_format=read_format==4?10:4;
            write.num_format=read_format==4?(pass==0?0:pass==1?1:4):(pass==0?4:pass==1?5:7);
        }
        if(mixed_writer_format){write.data_format=mixed_writer_format;write.num_format=mixed_writer_format==4?4:
            mixed_writer_format==10?(pass==0?0:pass==1?1:4):pass%2?1:5;}
        read.base_array=read.last_array=mip_alias?2:pass%2?2:1;
        write.base_array=write.last_array=mip_alias?2:pass%2?1:2;
        read.base_level=mip_alias?1:0;
        if(coherent_alias){write.base_array=write.last_array=read.base_array;read.base_level=write.base_level=pass%desc.levels;}
        Shader::Info info;info.hw_stage=Shader::HwStage::Compute;info.sw_stage=Shader::SwStage::Compute;info.flattened_ud_buf.resize(24);
        memcpy(info.flattened_ud_buf.data(),&write,sizeof(write));memcpy(info.flattened_ud_buf.data()+8,&read,sizeof(read));
        memcpy(info.flattened_ud_buf.data()+16,&output,sizeof(output));
        auto writer=ImageResource(0,desc.levels,false,true);writer.mip_fallback_mode=Shader::MipStorageFallbackMode::None;
        auto sampled=ImageResource(8,desc.levels,false,true);sampled.is_written=false;
        sampled.mip_fallback_mode=Shader::MipStorageFallbackMode::None;
        if(coherent_alias){
            if(mixed_writer_format){writer.native_raw32=mixed_writer_format==4||mixed_writer_format==10;writer.native_packed10_raw=mixed_writer_format==9;
                sampled.native_raw32=read_format==4||read_format==10;sampled.native_packed10_raw=read_format==9;}
            else if(read_format==9)writer.native_packed10_raw=sampled.native_packed10_raw=true;
            else writer.native_raw32=sampled.native_raw32=true;
            sampled.is_written=true;
        }
        const u32 writer_slot=coherent_alias&&pass%2?1:0,read_slot=1-writer_slot;
        info.images.push_back(writer_slot?sampled:writer);info.images.push_back(writer_slot?writer:sampled);
        info.images.push_back(ImageResource(16,1,false,true));
        using namespace Shader::Backend::SPIRV;
        Shader::Profile profile{};profile.supported_spirv=0x00010500;profile.native_coherent_images=true;
        Shader::RuntimeInfo runtime{};runtime.sw_stage=Shader::SwStage::Compute;Shader::Backend::Bindings bindings{};
        EmitContext c(profile,runtime,info,bindings);
        c.AddCapability(spv::Capability::StorageImageReadWithoutFormat);c.AddCapability(spv::Capability::StorageImageWriteWithoutFormat);
        const auto fn=c.OpFunction(c.void_id,spv::FunctionControlMask::MaskNone,c.TypeFunction(c.void_id));c.AddLabel();
        const auto coord=c.ConstU32(0u,0u,0u);
        const auto lod=coherent_alias?Sirit::Id{}:c.ConstU32(0u);
        const auto loaded=EmitImageRead(c,nullptr,read_slot,coord,lod,{});EmitImageWrite(c,nullptr,2,coord,{}, {},loaded);
        auto color=Color(u32(write.num_format),pass+2);
        if(mixed_writer_format==4)color[0]=0xc00805ffu+pass*0x00100401u;
        if(u32(write.data_format)==4&&u32(write.num_format)==7)color[0]=0x7fa12345U;
        EmitImageWrite(c,nullptr,writer_slot,coord,{}, {},c.OpBitcast(c.F32[4],c.ConstU32(color[0],color[1],color[2],color[3])));
        if(coherent_alias){
            c.OpMemoryBarrier(c.ConstU32(u32(spv::Scope::Device)),c.ConstU32(u32(spv::MemorySemanticsMask::ImageMemory|spv::MemorySemanticsMask::AcquireRelease)));
            EmitImageWrite(c,nullptr,2,c.ConstU32(1u,0u,0u),{}, {},EmitImageRead(c,nullptr,read_slot,coord,lod,{}));
        }
        c.OpReturn();c.OpFunctionEnd();c.AddEntryPoint(spv::ExecutionModel::GLCompute,fn,"main",c.interfaces);
        c.AddExecutionMode(fn,spv::ExecutionMode::LocalSize,1u,1u,1u);
        const auto dxil=Shader::Backend::DX12::ConvertSpirv(converter,c.Assemble(),options);
        if(!dxil.success)throw std::runtime_error(dxil.diagnostics);
        u32 source_word{};memcpy(&source_word,expected.data()+GuestPixelOffset(desc,layout,u32(read.base_level),u32(read.base_array),0,0),4);
        const auto value=Decode(source_word,number,read_format);
        const auto out_at=GuestPixelOffset(output_desc,output_layout,0,0,0,0);memcpy(expected_output.data()+out_at,value.data(),16);
        const auto written=Encode(color,u32(write.num_format),u32(write.data_format));
        if(mixed_writer_format&&written==source_word)throw std::runtime_error("Mixed word fixture must change the stored word");
        memcpy(expected.data()+GuestPixelOffset(desc,layout,u32(write.base_level),u32(write.base_array),0,0),&written,4);
        const auto after=Decode(written,number,read_format);const auto out_after=GuestPixelOffset(output_desc,output_layout,0,0,1,0);
        if(coherent_alias)memcpy(expected_output.data()+out_after,after.data(),16);
        ShaderPipeline pipeline(device.Get(),dxc,dxil.bytecode);ExecuteGuestBufferCompute(device.Get(),raster,pipeline,info,{},ComputeInvocation{},&textures);
        if(!std::equal(memory.begin(),memory.end(),expected.begin()))throw std::runtime_error("Packed10 alias guest words or padding changed");
        if(!Integer(number)&&read_format!=4)for(u32 position=0;position<(coherent_alias?2u:1u);++position)for(u32 channel=0;channel<4;++channel){float actual{},wanted{};
            const auto at=position?out_after:out_at;const auto& decoded=position?after:value;
            memcpy(&actual,output_bytes.data()+at+channel*4,4);memcpy(&wanted,decoded.data()+channel,4);
            if(!std::isfinite(actual)||std::abs(actual-wanted)>2e-7f)throw std::runtime_error("Packed10 disjoint sampled normalized value mismatch");
            memcpy(output_bytes.data()+at+channel*4,&wanted,4);
        }
        if(!std::equal(output_bytes.begin(),output_bytes.end(),expected_output.begin()))throw std::runtime_error("Packed10 disjoint sampled value or output padding mismatch");
        raster.Finish();raster.ValidateDebugMessages();
    }
    if(GetModuleHandleW(L"vulkan-1.dll"))throw std::runtime_error("Unexpected Vulkan disjoint packed10 execution");
    puts(mixed_writer_format?"PASS: coherent packed10/word shader views share changed writes, barriers and full guest padding":read_format!=9?"PASS: coherent R32/RGBA8 shader views share writes, raw bits and guest padding":coherent_alias?"PASS: coherent packed10 numeric views share writes and retain original words and guest padding":
        "PASS: disjoint packed10 sampled/UAV views retain numeric interpretation, writable backing and guest padding");return 0;
}
}
int RunNativePacked10(const wchar_t* converter,const wchar_t* dxc,std::wstring_view mode){
    if(mode==L"packed10-word-domain"){
        GuestTextureDesc packed{};packed.address=0x10000;packed.data_format=9;packed.number_format=1;
        packed.width=32;packed.height=16;packed.pitch=32;packed.writable=true;packed.native_packed10_raw=true;
        auto scalar=packed;scalar.data_format=4;scalar.number_format=4;scalar.native_packed10_raw=false;scalar.native_raw32=true;
        if(!CompatibleGuestWritableStorage(packed,scalar)||!CompatibleGuestWritableStorage(scalar,packed))
            throw std::runtime_error("Exact raw word storage was not compatible in both directions");
        for(u32 negative=0;negative<8;++negative){auto bad=scalar;
            switch(negative){case 0:bad.width++;break;case 1:bad.pitch+=32;break;
            case 2:bad.samples=4;break;case 3:bad.render_target=true;break;case 4:bad.depth_target=true;break;
            case 5:bad.writable=false;break;case 6:bad.native_raw32=false;break;case 7:bad.data_format=16;break;}
            if(CompatibleGuestWritableStorage(packed,bad)||CompatibleGuestWritableStorage(bad,packed))
                throw std::runtime_error("Writable raw word admitted an incompatible geometry/access/codec");
        }
        auto expanded=packed;expanded.number_format=0;expanded.native_packed10_raw=false;
        if(CompatibleGuestWritableStorage(expanded,scalar)||CompatibleGuestWritableStorage(scalar,expanded))
            throw std::runtime_error("Typed packed10 storage was mistaken for a raw R32 allocation");
        puts("PASS: common writable word domain preserves geometry, permissions and representation negatives");return 0;
    }
    if(mode==L"packed10-0-word-writer"||mode==L"packed10-1-word-writer"||
       mode==L"packed10-4-word-writer"||mode==L"packed10-5-word-writer")
        return RunDisjointPacked10(converter,dxc,u32(mode[9]-L'0'),false,true,9,4);
    if(mode==L"raw32-4-4-packed-writer"||mode==L"raw32-4-5-packed-writer"||mode==L"raw32-4-7-packed-writer")
        return RunDisjointPacked10(converter,dxc,u32(mode[8]-L'0'),false,true,4,9);
    if(mode==L"packed10-0-byte-writer"||mode==L"packed10-1-byte-writer"||
       mode==L"packed10-4-byte-writer"||mode==L"packed10-5-byte-writer")
        return RunDisjointPacked10(converter,dxc,u32(mode[9]-L'0'),false,true,9,10);
    if(mode==L"raw32-10-0-packed-writer"||mode==L"raw32-10-1-packed-writer"||
       mode==L"raw32-10-4-packed-writer"||mode==L"raw32-10-5-packed-writer")
        return RunDisjointPacked10(converter,dxc,u32(mode[9]-L'0'),false,true,10,9);
    if(mode.starts_with(L"raw32-4-")&&mode.size()==9){
        const u32 number=u32(mode[8]-L'0');
        if(number!=4&&number!=5&&number!=7)throw std::invalid_argument("Invalid R32 alias number format");
        return RunDisjointPacked10(converter,dxc,number,false,true,4);
    }
    if(mode.starts_with(L"raw32-10-")&&mode.size()==10){
        const u32 number=u32(mode[9]-L'0');
        if(number!=0&&number!=1&&number!=4&&number!=5)throw std::invalid_argument("Invalid RGBA8 alias number format");
        return RunDisjointPacked10(converter,dxc,number,false,true,10);
    }
    if(mode.size()<11)throw std::invalid_argument("Invalid packed10 fixture mode");
    const u32 number=u32(mode[9]-L'0');const bool msaa=mode.ends_with(L"-msaa"),linear=mode.ends_with(L"-linear");
    const bool one_dimensional=mode.ends_with(L"-1d"),volume=mode.ends_with(L"-3d");
    if(number!=0&&number!=1&&number!=4&&number!=5)throw std::invalid_argument("Invalid packed10 number format");
    if(mode.ends_with(L"-alias-coherent"))return RunDisjointPacked10(converter,dxc,number,false,true);
    if(mode.ends_with(L"-alias-layer")||mode.ends_with(L"-alias-mip"))
        return RunDisjointPacked10(converter,dxc,number,mode.ends_with(L"-alias-mip"));
    ComPtr<ID3D12Debug> debug;Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    ComPtr<ID3D12Device> device;Check(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)));
    struct Allocation{void* p;~Allocation(){if(p)VirtualFree(p,0,MEM_RELEASE);}} allocation{
        VirtualAlloc(reinterpret_cast<void*>(0x1500000000ull),0x200000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)};
    if(!allocation.p)throw std::runtime_error("Cannot allocate packed10 fixture guest memory");
    const auto address=reinterpret_cast<VAddr>(allocation.p);
    AmdGpu::Image sharp{};sharp.base_address=address>>8;sharp.width=64;sharp.height=one_dimensional?0:8;sharp.pitch=72;
    sharp.depth=volume?3:2;sharp.type=u64(msaa?AmdGpu::ImageType::Color2DMsaaArray:volume?AmdGpu::ImageType::Color3D:
        one_dimensional?AmdGpu::ImageType::Color1DArray:AmdGpu::ImageType::Color2DArray);
    sharp.last_level=2;sharp.last_array=volume?0:2;sharp.data_format=9;sharp.num_format=number;
    sharp.tiling_index=linear?8:13;sharp.dst_sel_x=4;sharp.dst_sel_y=5;sharp.dst_sel_z=6;sharp.dst_sel_w=7;
    auto desc=DescribeGuestTexture(sharp);desc.writable=true;const auto layout=LayoutGuestTexture(desc);
    auto output=sharp;output.base_address=(address+0x100000)>>8;output.data_format=14;output.num_format=Integer(number)?4:7;
    auto output_desc=DescribeGuestTexture(output);output_desc.writable=true;const auto output_layout=LayoutGuestTexture(output_desc);
    auto memory=std::span(static_cast<std::byte*>(allocation.p),size_t(layout.guest_bytes));
    auto output_bytes=std::span(static_cast<std::byte*>(allocation.p)+0x100000,size_t(output_layout.guest_bytes));
    std::fill(memory.begin(),memory.end(),std::byte{0xee});std::fill(output_bytes.begin(),output_bytes.end(),std::byte{0xcd});
    std::vector<std::byte> expected(memory.begin(),memory.end()),expected_output(output_bytes.begin(),output_bytes.end());
    const auto each=[&](auto&& fn){for(u32 mip=0;mip<desc.levels;++mip){const auto slices=volume?std::max(desc.volume_depth>>mip,1u):desc.layers;
        for(u32 slice=0;slice<slices;++slice)for(u32 sample=0;sample<desc.samples;++sample)fn(mip,slice,sample);}};
    each([&](u32 mip,u32 slice,u32 sample){const u32 word=0x80080600u+mip*111+slice*197+sample*331;
        const auto at=GuestPixelOffset(desc,layout,mip,slice,0,0,sample);memcpy(memory.data()+at,&word,4);memcpy(expected.data()+at,&word,4);});
    Rasterizer raster(device.Get());raster.MapMemory(address,0x200000);GuestTextureCache textures(device.Get());
    Shader::Info info;info.hw_stage=Shader::HwStage::Compute;info.sw_stage=Shader::SwStage::Compute;info.flattened_ud_buf.resize(16);
    memcpy(info.flattened_ud_buf.data(),&sharp,sizeof(sharp));memcpy(info.flattened_ud_buf.data()+8,&output,sizeof(output));
    info.images.push_back(ImageResource(0,desc.levels,msaa,!volume));info.images.push_back(ImageResource(8,output_desc.levels,msaa,!volume));
    Shader::Backend::DX12::SpirvConvertOptions options;options.stage=Shader::Backend::DX12::SpirvStage::Compute;
    for(u32 phase=0;phase<2;++phase){
        each([&](u32 mip,u32 slice,u32 sample){u32 initial;memcpy(&initial,expected.data()+GuestPixelOffset(desc,layout,mip,slice,0,0,sample),4);
            const auto store_output=[&](u32 x,const std::array<u32,4>& value){memcpy(expected_output.data()+GuestPixelOffset(output_desc,output_layout,mip,slice,x,0,sample),value.data(),16);};
            store_output(0,Decode(initial,number));const auto count=std::min(9u,(std::max(desc.width>>mip,1u)-1)/2);
            for(u32 i=0;i<count;++i){const u32 word=Encode(Color(number,i+phase),number),copied=Encode(Decode(word,number),number);
                memcpy(expected.data()+GuestPixelOffset(desc,layout,mip,slice,1+i*2,0,sample),&word,4);
                memcpy(expected.data()+GuestPixelOffset(desc,layout,mip,slice,2+i*2,0,sample),&copied,4);store_output(i+1,Decode(word,number));}
        });
        const auto dxil=Shader::Backend::DX12::ConvertSpirv(converter,Emit(info,desc,number,phase),options);
        if(!dxil.success)throw std::runtime_error(dxil.diagnostics);
        ShaderPipeline pipeline(device.Get(),dxc,dxil.bytecode);ExecuteGuestBufferCompute(device.Get(),raster,pipeline,info,{},ComputeInvocation{},&textures);
        if(!std::equal(memory.begin(),memory.end(),expected.begin()))throw std::runtime_error("Packed10 storage word/mip/layer/sample/padding mismatch");
        if(Integer(number)){
            if(!std::equal(output_bytes.begin(),output_bytes.end(),expected_output.begin()))throw std::runtime_error("Packed10 sign extension/integer bit pattern mismatch");
        }else{
            each([&](u32 mip,u32 slice,u32 sample){const auto count=std::min(9u,(std::max(desc.width>>mip,1u)-1)/2);
                for(u32 x=0;x<=count;++x){const auto at=GuestPixelOffset(output_desc,output_layout,mip,slice,x,0,sample);
                    for(u32 c=0;c<4;++c){float actual,wanted;memcpy(&actual,output_bytes.data()+at+c*4,4);memcpy(&wanted,expected_output.data()+at+c*4,4);
                        if(!std::isfinite(actual)||std::abs(actual-wanted)>2e-7f)throw std::runtime_error("Packed10 normalized typed load mismatch");}
                    memcpy(output_bytes.data()+at,expected_output.data()+at,16);}});
            if(!std::equal(output_bytes.begin(),output_bytes.end(),expected_output.begin()))throw std::runtime_error("Packed10 normalized output altered guest padding");
        }
        raster.Finish();raster.ValidateDebugMessages();
    }
    if(GetModuleHandleW(L"vulkan-1.dll"))throw std::runtime_error("Unexpected Vulkan packed10 execution");
    puts("PASS: packed10 native typed storage, signed/unsigned components, quantization, dimensions, samples, warm reuse and padding");return 0;
}
