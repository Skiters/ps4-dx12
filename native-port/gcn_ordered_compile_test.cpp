// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_shader_frontend.h"
#include "video_core/renderer_d3d12/guest_shader_cache.h"
#include "video_core/renderer_d3d12/guest_point_capture.h"
#include "video_core/amdgpu/regs.h"
#include "common/elf_info.h"
#include "core/emulator_settings.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <set>

// CPU-only: intentionally no device, queue, resource or PSO creation. Validated
// DXIL proves translation/compilation, never GPU pixels, ordering or lifetime.
int main(int argc,char** argv){try{
    if(argc!=4)throw std::invalid_argument("Expected converter, DXC and mode");
    using Capture=NativeDX12::NativePointCapturePass;
    const std::pair<D3D_PRIMITIVE_TOPOLOGY,u32> primitive_cases[]{
        {D3D_PRIMITIVE_TOPOLOGY_POINTLIST,7},{D3D_PRIMITIVE_TOPOLOGY_LINELIST,3},
        {D3D_PRIMITIVE_TOPOLOGY_LINESTRIP,6},{D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,2},
        {D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,5},{D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ,1},
        {D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ,4},{D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ,1},
        {D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ,1}};
    for(const auto [topology,expected]:primitive_cases)
        if(Capture::PrimitiveBound(topology,7)!=expected||Capture::PrimitiveBound(topology,0))
            throw std::runtime_error("Point capture assembly bound mismatch");
    if(Capture::PrimitiveBound(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,2)||
       Capture::PrimitiveBound(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ,5)||
       Capture::PrimitiveBound(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,UINT32_MAX)!=1431655765u||
       Capture::PrimitiveBound(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST,6)!=6||
       Capture::PrimitiveBound(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST,8)!=8)
        throw std::runtime_error("Point capture incomplete/large primitive bound mismatch");
    bool rejected{};try{Capture::PrimitiveBound(D3D_PRIMITIVE_TOPOLOGY_UNDEFINED,3);}
    catch(const std::invalid_argument&){rejected=true;}
    if(!rejected)throw std::runtime_error("Undefined point capture topology was accepted");
    Common::ElfInfo::Instance().InitializeApp("DX12FIXTURE","CPU shader compilation","1.0",
        Common::ElfInfo::FW_900,Common::ElfInfo::FW_900,{});
    EmulatorSettings.SetPipelineCacheEnabled(false);EmulatorSettings.SetShaderCollect(false);
    EmulatorSettings.SetDumpShaders(false);_putenv_s("SHADPS4_NATIVE_SHADER_CACHE","off");
    alignas(256) std::array<u32,128> vertex{},pixel{};
    const auto initialize=[](auto& program,const std::vector<u32>& words,u64 hash){
        if(words.size()>32)throw std::runtime_error("Shader overlaps metadata");
        std::copy(words.begin(),words.end(),program.begin());AmdGpu::BinaryInfo binary{};
        binary.signature=binary.signature_ref;binary.length=u32(words.size()*4);binary.shader_hash=hash;
        memcpy(program.data()+32,&binary,sizeof(binary));
    };
    std::vector<u32> words{0xBEEB03FF,15};
    for(u32 i=0;i<4;++i){words.push_back(0xe0302000U|i*4);words.push_back((128U<<24)|((4+i)<<8));}
    words.insert(words.end(),{0xbf8c007f,0xf80008cf,0x07060504,0xbf810000});
    initialize(vertex,words,0x434f4d504f524456ULL);
    initialize(pixel,{0xBEEB03FF,15,0x7e0002ff,0x3f800000,0x7e020280,0x7e040280,0x7e0602ff,0x3f800000,
        0xf800000f,0x03020100,0x7e000280,0x7e0202ff,0x3f800000,0xf800001f,0x03020100,
        0x7e020280,0x7e0402ff,0x3f800000,0xf800082f,0x03020100,0xbf810000},0x434f4d504f524450ULL);
    Shader::Profile profile{};profile.supported_spirv=0x00010500;profile.needs_buffer_offsets=true;
    profile.native_coherent_images=true;profile.native_buffer_bounds=true;
    profile.max_viewport_width=profile.max_viewport_height=16384;
    profile.support_int8=profile.support_int16=true;profile.native_alpha_blend_factor=false;
    NativeDX12::GuestShaderFrontend frontend(profile);
    Shader::Backend::DX12::SpirvConvertOptions options;options.native_storage_buffer_uavs=true;
    NativeDX12::GuestShaderCache cache(std::filesystem::path(argv[1]).wstring(),options);
    alignas(256) std::array<float,24> positions{};
    AmdGpu::Buffer input{};input.base_address=reinterpret_cast<u64>(positions.data());
    input.stride=32;input.num_records=3;input.data_format=4;input.num_format=4;
    input.dst_sel_x=4;input.dst_sel_y=5;input.dst_sel_z=6;input.dst_sel_w=7;
    unsigned cases{};
    std::set<u64> fragment_variants;
    for(const auto shape:{std::array<u32,2>{10,0},{10,1},{5,0},{5,1},{4,7},{5,7},{9,0},{9,1},{11,7},{14,7},{0,7}})
    for(u32 sample_log:{0u,1u,2u,3u})for(u32 mask:{0xfffu,0xfa1u})for(u32 operation:{0u,1u,2u}){
        if(operation&&(shape[1]==7||mask==0xfff))continue;
        AmdGpu::Regs regs{};regs.stage_enable.raw=AmdGpu::ShaderStageEnable::Vs;
        regs.primitive_type=AmdGpu::PrimitiveType::TriangleList;regs.num_indices=3;
        regs.vs_program.address=reinterpret_cast<u64>(vertex.data())>>8;
        regs.vs_program.settings.num_user_regs=4;regs.vs_program.settings.num_vgprs=1;
        memcpy(regs.vs_program.user_data.data(),&input,sizeof(input));
        regs.ps_program.address=reinterpret_cast<u64>(pixel.data())>>8;
        const u32 format=u32(AmdGpu::ShaderExportFormat::ABGR_32);
        regs.color_export_format.raw=format|(format<<4)|(format<<8);
        regs.color_control.mode=AmdGpu::ColorControl::OperationMode::Normal;
        regs.color_control.rop3=operation==1?AmdGpu::ColorControl::LogicOp::Xor:
            operation==2?AmdGpu::ColorControl::LogicOp::Invert:AmdGpu::ColorControl::LogicOp::Copy;
        regs.color_shader_mask.raw=0xfff;regs.color_target_mask.raw=mask;
        regs.blend_constants={.25f,.5f,.75f,.5f};
        using F=AmdGpu::BlendControl::BlendFactor;
        for(u32 i=0;i<3;++i){auto& cb=regs.color_buffers[i];
            const u32 mixed_formats[]{4,11,14};
            cb.base_address=(0x100000u+i*4096)>>8;cb.info.format=shape[0]?shape[0]:mixed_formats[i];cb.info.number_type=shape[1];
            cb.attrib.tile_mode_index=AmdGpu::TileMode::Thin1DThin;
            cb.attrib.num_fragments_log2=sample_log;cb.pitch.tile_max=cb.slice.tile_max=1;
            auto& blend=regs.blend_control[i];blend.enable=1;blend.separate_alpha_blend=1;
            blend.alpha_src_factor=F::One;blend.alpha_dst_factor=F::Zero;
        }
        regs.blend_control[0].color_src_factor=F::ConstantColor;
        regs.blend_control[0].color_dst_factor=F::ConstantAlpha;
        regs.blend_control[1].color_src_factor=F::OneMinusConstantAlpha;
        regs.blend_control[1].color_dst_factor=F::OneMinusConstantColor;
        regs.blend_control[2].color_src_factor=F::ConstantAlpha;
        regs.blend_control[2].color_dst_factor=F::OneMinusConstantAlpha;
        printf("COMPILE format=%u number=%u samples=%u mask=%03x operation=%u\n",shape[0],shape[1],1u<<sample_log,mask,operation);
        const auto stages=frontend.BuildGraphics(regs);
        if(stages.ordered_color_mask!=7)throw std::runtime_error("Ordered compiler case missed an active target");
        const auto* fragment=stages.stages[u32(Shader::SwStage::Fragment)];
        if(!fragment||!fragment_variants.insert(fragment->id).second)
            throw std::runtime_error("Ordered shader reused a different format/sample/mask/logic specialization");
        for(u32 i=0;i<3;++i){const auto& target=fragment->runtime.hw.fs.ordered_color_targets[i];
            const u32 mixed_formats[]{4,11,14};
            if(!target.enabled||u32(target.data_format)!=(shape[0]?shape[0]:mixed_formats[i])||u32(target.number_format)!=shape[1]||
               target.num_samples!=(1u<<sample_log)||target.logic_op!=regs.color_control.rop3)
                throw std::runtime_error("Ordered shader runtime does not match the requested specialization");
        }
        const auto& dxil=cache.Graphics(stages);
        for(u32 stage=0;stage<Shader::MaxStageTypes;++stage)if(stages.stages[stage]){
            const auto& result=dxil.stages[stage];
            if(!result.success||!result.converted||!result.validated||result.bytecode.size()<32)
                throw std::runtime_error("Ordered shader did not produce validated DXIL: "+result.diagnostics);
        }
        ++cases;
    }
    printf("PASS: %u CPU-only GCN ordered color translations and signed DXIL validations; GPU NOT_RUN\n",cases);
    return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
