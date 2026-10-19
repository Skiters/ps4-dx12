// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_shader_frontend.h"
#include "video_core/renderer_d3d12/guest_shader_cache.h"
#include "video_core/renderer_d3d12/guest_point_capture.h"
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "video_core/renderer_d3d12/guest_graphics_state.h"
#include "ufloat_attachment_oracle.h"
#include "normalized_blend_oracle.h"
#include "srgb_blend_oracle.h"
#include "integer_logic_oracle.h"
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
    NativeDX12::Tests::CheckUfloatBoundaryOracle();
    NativeDX12::Tests::CheckNormalizedBlendOracle();
    NativeDX12::Tests::CheckSrgbBlendOracle();
    NativeDX12::Tests::CheckIntegerLogicOracle();
    // Vulkan logicOpEnable disables blending even for formats where the bit
    // operation itself is not applied. Check native state without a device.
    AmdGpu::Regs logic_regs{};
    logic_regs.color_control.mode=AmdGpu::ColorControl::OperationMode::Normal;
    logic_regs.color_control.rop3=AmdGpu::ColorControl::LogicOp::Xor;
    logic_regs.color_target_mask.raw=logic_regs.color_shader_mask.raw=0xfff;
    for(u32 i=0;i<3;++i){
        auto& color=logic_regs.color_buffers[i];color.base_address=1+i;
        color.info.format=i==2?4:10;color.info.number_type=i==1?6:i==2?7:0;
        logic_regs.blend_control[i].enable=1;
    }
    const std::array logical_formats{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,DXGI_FORMAT_R32_FLOAT};
    const auto logical_state=NativeDX12::BuildGuestGraphicsState(logic_regs,logical_formats);
    if(logical_state.ordered_color_mask!=1||logical_state.pipeline.RTVFormats[0]!=DXGI_FORMAT_UNKNOWN||
       logical_state.pipeline.RTVFormats[1]!=logical_formats[1]||logical_state.pipeline.RTVFormats[2]!=logical_formats[2])
        throw std::runtime_error("Mixed normalized/sRGB/FLOAT logic path lost native pass-through or ordered integer operation");
    for(u32 i=0;i<3;++i)
        if(logical_state.pipeline.BlendState.RenderTarget[i].BlendEnable||logical_state.pipeline.BlendState.RenderTarget[i].LogicOpEnable)
            throw std::runtime_error("Mixed logic path enabled fixed-function blend/logic on a pass-through attachment");
    for(const auto shape:{std::array<u32,3>{10,6,DXGI_FORMAT_R8G8B8A8_UNORM_SRGB},{4,7,DXGI_FORMAT_R32_FLOAT}}){
        logic_regs.color_buffers[0].info.format=shape[0];logic_regs.color_buffers[0].info.number_type=shape[1];
        const std::array format{DXGI_FORMAT(shape[2])};
        const auto state=NativeDX12::BuildGuestGraphicsState(logic_regs,format);
        if(state.ordered_color_mask||state.pipeline.BlendState.RenderTarget[0].BlendEnable||
           state.pipeline.BlendState.RenderTarget[0].LogicOpEnable||state.pipeline.RTVFormats[0]!=format[0])
            throw std::runtime_error("Single sRGB/FLOAT logic pass-through changed Vulkan semantics");
    }
    logic_regs.color_target_mask.raw=0x1f;
    for(u32 i=0;i<2;++i){logic_regs.color_buffers[i].info.format=12;logic_regs.color_buffers[i].info.number_type=0;}
    const std::array normalized_logic{DXGI_FORMAT_R16G16B16A16_UNORM,DXGI_FORMAT_R16G16B16A16_UNORM};
    const auto normalized_state=NativeDX12::BuildGuestGraphicsState(logic_regs,normalized_logic);
    if(normalized_state.ordered_color_mask!=3||normalized_state.pipeline.NumRenderTargets)
        throw std::runtime_error("RGBA16 independent logical masks no longer use both ordered targets");
    for(u32 i=0;i<2;++i){logic_regs.color_buffers[i].info.format=4;logic_regs.color_buffers[i].info.number_type=4;}
    const std::array integer_logic{DXGI_FORMAT_R32_UINT,DXGI_FORMAT_R32_UINT};
    const auto integer_state=NativeDX12::BuildGuestGraphicsState(logic_regs,integer_logic);
    if(integer_state.ordered_color_mask!=3||integer_state.pipeline.NumRenderTargets)
        throw std::runtime_error("UINT independent logical masks lost an ordered target");
    logic_regs.color_control.rop3=AmdGpu::ColorControl::LogicOp::Copy;
    logic_regs.blend_control[0].color_src_factor=AmdGpu::BlendControl::BlendFactor::ConstantColor;
    logic_regs.blend_control[1].color_src_factor=AmdGpu::BlendControl::BlendFactor::ConstantAlpha;
    if(NativeDX12::GuestOrderedColorMask(logic_regs,3,false))
        throw std::runtime_error("Integer color incorrectly selected floating-point ordered blending");
    using Cache=NativeDX12::GuestTextureCache;
    const std::set<DXGI_FORMAT> rg16_family{DXGI_FORMAT_R16G16_UNORM,DXGI_FORMAT_R16G16_SNORM,
        DXGI_FORMAT_R16G16_UINT,DXGI_FORMAT_R16G16_SINT,DXGI_FORMAT_R16G16_FLOAT,DXGI_FORMAT_R32_UINT};
    const std::set<DXGI_FORMAT> rgba8_family{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R8G8B8A8_SNORM,
        DXGI_FORMAT_R8G8B8A8_UINT,DXGI_FORMAT_R8G8B8A8_SINT,DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,DXGI_FORMAT_R32_UINT};
    const std::set<DXGI_FORMAT> packed_family{DXGI_FORMAT_R10G10B10A2_UNORM,DXGI_FORMAT_R10G10B10A2_UINT,DXGI_FORMAT_R32_UINT};
    const std::set<DXGI_FORMAT> rg8_family{DXGI_FORMAT_R8G8_UNORM,DXGI_FORMAT_R8G8_SNORM,
        DXGI_FORMAT_R8G8_UINT,DXGI_FORMAT_R8G8_SINT,DXGI_FORMAT_R16_UINT};
    const std::set<DXGI_FORMAT> rgba16_family{DXGI_FORMAT_R16G16B16A16_UNORM,DXGI_FORMAT_R16G16B16A16_SNORM,
        DXGI_FORMAT_R16G16B16A16_UINT,DXGI_FORMAT_R16G16B16A16_SINT,DXGI_FORMAT_R16G16B16A16_FLOAT,DXGI_FORMAT_R32G32_UINT};
    for(const auto shape:{std::array<u32,2>{5,0},{5,1},{5,7},{10,0},{10,1},{10,9},{9,0},{9,1},{16,0},{17,0},{18,0},{19,0},
        {3,4},{3,5},{5,4},{5,5},{9,4},{9,5},{10,4},{10,5},{12,4},{12,5}}){
        NativeDX12::GuestTextureDesc desc;desc.render_target=true;desc.data_format=shape[0];desc.number_format=shape[1];
        const auto formats=Cache::OrderedColorFormats(desc,true);
        const std::set<DXGI_FORMAT> actual(formats.begin(),formats.end());
        const std::set<DXGI_FORMAT> packed16_family{shape[0]==16?DXGI_FORMAT_B5G6R5_UNORM:
            shape[0]==19?DXGI_FORMAT_B4G4R4A4_UNORM:DXGI_FORMAT_B5G5R5A1_UNORM,DXGI_FORMAT_R16_UINT};
        const auto& expected=shape[0]>=16?packed16_family:shape[0]==3?rg8_family:shape[0]==5?rg16_family:
            shape[0]==10?rgba8_family:shape[0]==12?rgba16_family:packed_family;
        if(actual!=expected||actual.size()!=formats.size()||!Cache::OrderedColorFormats(desc,false).empty())
            throw std::runtime_error("Ordered allocation narrowed its original typed family or ignored capabilities");
        auto invalid=desc;invalid.writable=true;
        if(!Cache::OrderedColorFormats(invalid,true).empty())throw std::runtime_error("Writer admitted as ordered attachment");
        invalid=desc;invalid.render_target=false;
        if(!Cache::OrderedColorFormats(invalid,true).empty())throw std::runtime_error("Nonattachment admitted as ordered allocation");
        invalid=desc;invalid.depth_target=true;
        if(!Cache::OrderedColorFormats(invalid,true).empty())throw std::runtime_error("Depth admitted as ordered color");
    }
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
    for(unsigned boundary=0;boundary<=NativeDX12::Tests::NormalizedBlendBoundaries.size();++boundary)
    for(const auto shape:{std::array<u32,2>{10,0},{10,1},{10,9},{5,0},{5,1},{4,7},{5,7},{9,0},{9,1},{11,7},{14,7},{0,7},{12,0},{12,1},{12,7},{0,0},{6,7},{1,0},{1,1},{2,0},{2,1},{2,7},{0,1},{3,0},{3,1},{16,0},{17,0},{18,0},{19,0},
        {1,4},{1,5},{2,4},{2,5},{3,4},{3,5},{4,4},{4,5},{5,4},{5,5},{9,4},{9,5},{10,4},{10,5},{11,4},{11,5},{12,4},{12,5},{14,4},{14,5}})
    for(u32 sample_log:{0u,1u,2u,3u})for(u32 mask:{0xfffu,0xfa1u})for(u32 operation:{0u,1u,2u}){
        if(boundary&&(shape[0]!=10||(shape[1]>1&&shape[1]!=9)||operation))continue;
        const bool integer=shape[1]==4||shape[1]==5;
        if(integer&&(!operation||mask==0xfff))continue;
        if(operation&&(shape[1]==7||shape[1]==9||mask==0xfff))continue;
        if(!boundary){
            if(integer){
                const bool sint=shape[1]==5;
                initialize(pixel,{0xBEEB03FF,15,0x7e0002ff,sint?u32(-5):5,
                    0x7e0202ff,sint?u32(-10):10,0x7e0402ff,3,0x7e0602ff,sint?u32(-1):1,
                    0xf800000f,0x03020100,0xf800001f,0x03020100,0xf800082f,0x03020100,0xbf810000},
                    0x494e544c4f470000ULL+shape[1]);
            }else initialize(pixel,{0xBEEB03FF,15,0x7e0002ff,0x3f800000,0x7e020280,0x7e040280,0x7e0602ff,0x3f800000,
                0xf800000f,0x03020100,0x7e000280,0x7e0202ff,0x3f800000,0xf800001f,0x03020100,
                0x7e020280,0x7e0402ff,0x3f800000,0xf800082f,0x03020100,0xbf810000},0x434f4d504f524450ULL);
        }
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
            const u32 mixed_formats[]{shape[1]==7?4u:shape[1]==1?1u:10u,
                shape[1]==7?11u:shape[1]==1?2u:5u,shape[1]==7?14u:shape[1]==1?10u:12u};
            cb.base_address=(0x100000u+i*4096)>>8;cb.info.format=shape[0]?shape[0]:mixed_formats[i];cb.info.number_type=shape[1]==9?6:shape[1];
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
        if(boundary){
            const auto& point=NativeDX12::Tests::NormalizedBlendBoundaries[boundary-1];
            std::copy(point.constants.begin(),point.constants.end(),regs.blend_constants.begin());
            regs.blend_control[1].color_src_factor=point.color_factors?F::OneMinusSrcColor:F::OneMinusConstantAlpha;
            regs.blend_control[1].color_dst_factor=point.color_factors?F::ConstantAlpha:F::OneMinusConstantColor;
            regs.blend_control[2].color_src_factor=point.color_factors?F::SrcAlpha:F::ConstantAlpha;
            regs.blend_control[2].color_dst_factor=point.color_factors?F::OneMinusSrcAlpha:F::OneMinusConstantAlpha;
            initialize(pixel,{0xBEEB03FF,15,0x7e0002ff,std::bit_cast<u32>(point.source[0]),
                0x7e0202ff,std::bit_cast<u32>(point.source[1]),0x7e0402ff,std::bit_cast<u32>(point.source[2]),
                0x7e0602ff,std::bit_cast<u32>(point.source[3]),0xf800000f,0x03020100,
                0xf800001f,0x03020100,0xf800082f,0x03020100,0xbf810000},0x4e4f524d45440000ULL+boundary-1);
        }
        printf("COMPILE format=%u number=%u samples=%u mask=%03x operation=%u boundary=%u\n",shape[0],shape[1],1u<<sample_log,mask,operation,boundary);
        const auto stages=frontend.BuildGraphics(regs);
        if(stages.ordered_color_mask!=7)throw std::runtime_error("Ordered compiler case missed an active target");
        const auto* fragment=stages.stages[u32(Shader::SwStage::Fragment)];
        if(!fragment||!fragment_variants.insert(fragment->id).second)
            throw std::runtime_error("Ordered shader reused a different format/sample/mask/logic specialization");
        for(u32 i=0;i<3;++i){const auto& target=fragment->runtime.hw.fs.ordered_color_targets[i];
            const u32 mixed_formats[]{shape[1]==7?4u:shape[1]==1?1u:10u,
                shape[1]==7?11u:shape[1]==1?2u:5u,shape[1]==7?14u:shape[1]==1?10u:12u};
            const u32 expected_format=shape[0]?shape[0]:mixed_formats[i];
            if(!target.enabled||u32(target.data_format)!=(expected_format==18?17:expected_format)||u32(target.number_format)!=shape[1]||
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
