// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_shader_frontend.h"
#include "video_core/renderer_d3d12/guest_shader_cache.h"
#include "video_core/amdgpu/regs.h"
#include "video_core/renderer_d3d12/guest_compute.h"
#include "video_core/renderer_d3d12/guest_graphics_state.h"
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "video_core/renderer_d3d12/capabilities.h"
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <cstdio>
#include <cstring>
#include <cmath>
int main(int argc,char** argv){try{
    if(argc!=3&&argc!=4)throw std::invalid_argument("Expected converter and DXC paths");
    const std::string requested_mode=argc==4?argv[3]:"base";
    const bool buffer_atomic=requested_mode.starts_with("buffer-");
    const bool dynamic_operand=buffer_atomic&&requested_mode.ends_with("-dynamic");
    const std::string normalized=dynamic_operand?requested_mode.substr(0,requested_mode.size()-8):requested_mode;
    const std::string mode=buffer_atomic?"atomic-"+normalized.substr(7):normalized;
    const bool aniso_case=mode.find("-aniso-2")!=std::string::npos||mode.find("-aniso-4")!=std::string::npos||
        mode.find("-aniso-8")!=std::string::npos||mode.find("-aniso-16")!=std::string::npos;
    const u32 aniso_limit=mode.find("-aniso-16")!=std::string::npos?16:
        mode.find("-aniso-8")!=std::string::npos?8:mode.find("-aniso-4")!=std::string::npos?4:2;
    const bool aniso_linear=aniso_case&&mode.find("-linear")!=std::string::npos;
    const bool aniso_mip_linear=aniso_case&&mode.ends_with("-mip");
    const bool aniso_cube=aniso_case&&mode.find("-cube")!=std::string::npos;
    const bool aniso_cube_array=aniso_cube&&mode.find("-cube-array")!=std::string::npos;
    const size_t sample_destination_offset=aniso_cube?262144:32768;
    const size_t allocation_bytes=aniso_cube?524288:65536;
    const u32 aniso_sample_layer=aniso_cube_array?11:aniso_cube?5:0;
    std::array<float,2> aniso_dx{1.f,0.f},aniso_dy{0.f,0.125f};
    if(mode.ends_with("-y")){aniso_dx={0.125f,0.f};aniso_dy={0.f,1.f};}
    if(mode.ends_with("-zero")){aniso_dx={0.f,0.f};aniso_dy={0.f,0.f};}
    if(mode.ends_with("-negative"))aniso_dx[0]=-1.f;
    if(mode.ends_with("-degenerate"))aniso_dy={0.f,0.f};
    if(mode.ends_with("-three"))aniso_dx[0]=0.375f;
    if(mode.ends_with("-skew")||aniso_mip_linear)aniso_dx={0.75f,0.75f};
    const float aniso_uv=mode.ends_with("-mirror")?-0.625f:mode.ends_with("-wrap")?1.625f:
        aniso_cube&&mode.ends_with("-edge")?0.f:0.5f;
    const bool atomic64=buffer_atomic&&mode.ends_with("64");
    const bool sample_volume=mode.starts_with("sample-feedback-volume-");
    const bool sample_gather=mode.starts_with("sample-feedback-gather-");
    const bool gather_integer=sample_gather&&mode.find("-integer-")!=std::string::npos;
    const bool gather_integer_white=gather_integer&&mode.find("-border-white")!=std::string::npos;
    const auto gather_word=[](float value,u32 component){
        constexpr u32 seeds[]{0x80000000U,0x7fc00000U,0x01000001U,0xfff00000U};
        return seeds[component]+u32(value)*257U;
    };
    const bool gather_compare=sample_gather&&mode.find("-compare")!=std::string::npos;
    const bool gather_array=sample_gather&&mode.find("-array")!=std::string::npos;
    const bool gather_lod=sample_gather&&mode.find("-lod")!=std::string::npos;
    const bool gather_bias=gather_lod&&mode.find("-bias")!=std::string::npos;
    const bool gather_implicit=gather_lod&&mode.find("-implicit")!=std::string::npos;
    const bool sample_depth=mode.starts_with("sample-feedback-depth-")||sample_volume;
    const bool depth_array=sample_depth&&mode.find("-array")!=std::string::npos;
    const bool depth_1d=sample_depth&&mode.find("-1d")!=std::string::npos;
    const bool depth_shared=sample_depth&&mode.find("-shared")!=std::string::npos;
    const bool sample_readonly=(sample_depth&&!depth_shared)||mode.find("-resource-readonly")!=std::string::npos||
        (aniso_case&&mode.ends_with("-readonly"))||mode=="sample-feedback-grad-clamp-readonly"||mode=="sample-feedback-implicit-clamp-readonly";
    const bool resource_alias=mode=="sample-feedback-lod-resource-alias";
    if(buffer_atomic&&!atomic64&&!mode.starts_with("atomic-f")&&!mode.starts_with("atomic-inc")&&!mode.starts_with("atomic-dec")&&!mode.starts_with("atomic-sub"))
        throw std::invalid_argument("Unsupported buffer fixture");
    constexpr std::string_view modes[]{"base","sample-feedback","sample-feedback-linear",
        "sample-feedback-low","sample-feedback-high","sample-feedback-linear-low",
        "sample-feedback-linear-high","sample-feedback-linear-unnormalized",
        "sample-feedback-linear-wrap","sample-feedback-linear-wrap-negative","sample-feedback-linear-wrap-seam",
        "sample-feedback-linear-mirror","sample-feedback-linear-mirror-negative","sample-feedback-linear-mirror-seam",
        "sample-feedback-linear-mirror-once","sample-feedback-linear-mirror-once-negative",
        "sample-feedback-linear-mirror-once-low","sample-feedback-linear-mirror-once-high",
        "sample-feedback-mirror-once-negative","sample-feedback-mirror-once-high",
        "sample-feedback-border-white-half-linear-resource-readonly",
        "sample-feedback-border-white-half-point-resource-readonly",
        "sample-feedback-border-white-half-mirror-linear-resource-readonly",
        "sample-feedback-border-white-mirror-linear-inside-resource-readonly",
        "sample-feedback-border-custom-half-linear-resource-readonly",
        "sample-feedback-border-custom-half-mirror-linear-resource-readonly",
        "sample-feedback-border-white-half-linear",
        "sample-feedback-border-white-half-point",
        "sample-feedback-border-white-half-high-linear",
        "sample-feedback-border-white-half-mirror-linear",
        "sample-feedback-border-white-half-mirror-point",
        "sample-feedback-border-white-mirror-linear-inside",
        "sample-feedback-border-white-mirror-linear-outside",
        "sample-feedback-border-custom-half-linear",
        "sample-feedback-border-custom-half-point",
        "sample-feedback-border-custom-half-mirror-linear",
        "sample-feedback-border-transparent","sample-feedback-border-opaque","sample-feedback-border-white",
        "sample-feedback-border-custom","sample-feedback-border-custom-edge",
        "sample-feedback-border-transparent-edge","sample-feedback-border-opaque-edge","sample-feedback-border-white-edge",
        "sample-feedback-lod-point","sample-feedback-lod-linear","sample-feedback-lod-linear-dynamic","sample-feedback-lod-low",
        "sample-feedback-lod-high","sample-feedback-lod-min","sample-feedback-lod-max",
        "sample-feedback-lod-round-low","sample-feedback-lod-round-high","sample-feedback-lod-none",
        "sample-feedback-lod-resource","sample-feedback-lod-linear-resource",
        "sample-feedback-lod-resource-limit","sample-feedback-lod-resource-base",
        "sample-feedback-lod-resource-below-base",
        "sample-feedback-lod-resource-readonly",
        "sample-feedback-lod-resource-readonly-base","sample-feedback-lod-linear-resource-readonly",
        "sample-feedback-lz-resource","sample-feedback-lz-resource-readonly",
        "sample-feedback-lz-linear-resource","sample-feedback-lz-linear-resource-readonly",
        "sample-feedback-lod-resource-none","sample-feedback-lod-resource-readonly-none",
        "sample-feedback-lod-resource-alias",
        "sample-feedback-grad","sample-feedback-grad-zero","sample-feedback-grad-negative",
        "sample-feedback-grad-aniso-one","sample-feedback-grad-aniso-one-zero",
        "sample-feedback-grad-aniso-one-linear","sample-feedback-grad-aniso-one-linear-zero",
        "sample-feedback-grad-aniso-2","sample-feedback-grad-aniso-4",
        "sample-feedback-grad-aniso-8","sample-feedback-grad-aniso-16",
        "sample-feedback-grad-aniso-4-y","sample-feedback-grad-aniso-4-linear",
        "sample-feedback-grad-aniso-4-linear-y","sample-feedback-grad-aniso-8-linear-zero",
        "sample-feedback-grad-aniso-16-linear-zero","sample-feedback-grad-aniso-4-linear-negative",
        "sample-feedback-grad-aniso-4-linear-degenerate","sample-feedback-grad-aniso-4-linear-three",
        "sample-feedback-grad-aniso-4-linear-skew","sample-feedback-grad-aniso-4-linear-mip",
        "sample-feedback-grad-aniso-4-linear-min","sample-feedback-grad-aniso-4-linear-max",
        "sample-feedback-grad-aniso-4-linear-resource","sample-feedback-grad-aniso-4-linear-unnormalized",
        "sample-feedback-grad-aniso-4-linear-mirror","sample-feedback-grad-aniso-4-linear-wrap",
        "sample-feedback-grad-aniso-4-linear-reduce-min","sample-feedback-grad-aniso-4-linear-reduce-max",
        "sample-feedback-lod-aniso-4-bias","sample-feedback-lz-aniso-4-bias",
        "sample-feedback-grad-aniso-4-readonly","sample-feedback-grad-aniso-4-linear-readonly",
        "sample-feedback-grad-aniso-4-linear-resource-readonly",
        "sample-feedback-lod-aniso-4-bias-cancel-high","sample-feedback-lod-aniso-4-bias-cancel-low",
        "sample-feedback-grad-aniso-4-cube","sample-feedback-grad-aniso-4-linear-cube",
        "sample-feedback-grad-aniso-4-linear-cube-array","sample-feedback-grad-aniso-4-linear-cube-array-readonly",
        "sample-feedback-grad-aniso-4-linear-cube-readonly","sample-feedback-grad-aniso-4-linear-cube-edge",
        "sample-feedback-grad-aniso-4-linear-cube-mip",
        "sample-feedback-grad-y","sample-feedback-grad-bias","sample-feedback-grad-unnormalized",
        "sample-feedback-grad-skew",
        "sample-feedback-grad-clamp","sample-feedback-grad-clamp-low",
        "sample-feedback-grad-clamp-linear",
        "sample-feedback-grad-clamp-readonly",
        "sample-feedback-grad-clamp-offset",
        "sample-feedback-implicit","sample-feedback-implicit-zero",
        "sample-feedback-implicit-clamp","sample-feedback-implicit-clamp-bias",
        "sample-feedback-implicit-clamp-offset","sample-feedback-implicit-clamp-bias-offset",
        "sample-feedback-implicit-clamp-readonly",
        "sample-feedback-implicit-clamp-bias-gradient",
        "sample-feedback-depth-clamp","sample-feedback-depth-clamp-false",
        "sample-feedback-depth-clamp-offset","sample-feedback-depth-clamp-bias",
        "sample-feedback-depth-clamp-bias-never","sample-feedback-depth-clamp-bias-equal",
        "sample-feedback-depth-clamp-bias-less-equal","sample-feedback-depth-clamp-bias-greater",
        "sample-feedback-depth-clamp-bias-not-equal","sample-feedback-depth-clamp-bias-greater-equal",
        "sample-feedback-depth-clamp-bias-always","sample-feedback-depth-clamp-bias-linear",
        "sample-feedback-depth-clamp-linear",
        "sample-feedback-depth-clamp-grad","sample-feedback-depth-clamp-grad-linear",
        "sample-feedback-depth-clamp-shared","sample-feedback-depth-clamp-shared-linear",
        "sample-feedback-depth-clamp-shared-grad-linear","sample-feedback-depth-clamp-shared-bias-linear",
        "sample-feedback-depth-clamp-shared-after-write",
        "sample-feedback-depth-clamp-shared-crossstage",
        "sample-feedback-depth-clamp-shared-array","sample-feedback-depth-clamp-shared-array-linear",
        "sample-feedback-depth-clamp-shared-array-low","sample-feedback-depth-clamp-shared-array-high",
        "sample-feedback-depth-clamp-shared-array-half","sample-feedback-depth-clamp-shared-array-upper",
        "sample-feedback-depth-clamp-bias-array","sample-feedback-depth-clamp-array",
        "sample-feedback-depth-clamp-shared-array-unnormalized","sample-feedback-depth-clamp-array-unnormalized",
        "sample-feedback-depth-clamp-array-half",
        "sample-feedback-depth-clamp-shared-1d","sample-feedback-depth-clamp-shared-1d-linear",
        "sample-feedback-depth-clamp-shared-1d-array","sample-feedback-depth-clamp-shared-1d-array-linear",
        "sample-feedback-depth-clamp-shared-1d-array-half","sample-feedback-depth-clamp-shared-1d-array-unnormalized",
        "sample-feedback-depth-clamp-bias-1d","sample-feedback-depth-clamp-bias-1d-array",
        "sample-feedback-depth-clamp-1d-linear","sample-feedback-depth-clamp-1d-array-unnormalized",
        "sample-feedback-depth-clamp-shared-1d-grad","sample-feedback-depth-clamp-shared-1d-array-grad",
        "sample-feedback-depth-clamp-shared-1d-grad-linear","sample-feedback-depth-clamp-shared-1d-offset",
        "sample-feedback-volume-shared-point","sample-feedback-volume-shared-point-z",
        "sample-feedback-volume-shared-linear",
        "sample-feedback-volume-shared-linear-xy","sample-feedback-volume-shared-linear-z",
        "sample-feedback-volume-shared-linear-border","sample-feedback-volume-shared-linear-wrap",
        "sample-feedback-volume-shared-linear-mirror",
        "sample-feedback-volume-shared-linear-mip","sample-feedback-volume-shared-linear-grad-z",
        "sample-feedback-volume-shared-linear-grad-dy-z",
        "sample-feedback-gather-integer-uint-lod-implicit",
        "sample-feedback-gather-integer-sint-lod-bias-down",
        "sample-feedback-gather-integer-sint-lod-resource-readonly-implicit",
        "sample-feedback-gather-integer-uint-lod-resource-readonly-bias",
        "sample-feedback-gather-aniso-point",
        "sample-feedback-gather-aniso-linear",
        "sample-feedback-gather-compare-aniso",
        "sample-feedback-gather-integer-uint-aniso",
        "sample-feedback-gather-integer-uint",
        "sample-feedback-gather-integer-sint",
        "sample-feedback-gather-integer-uint-red",
        "sample-feedback-gather-integer-sint-blue",
        "sample-feedback-gather-integer-uint-alpha",
        "sample-feedback-gather-integer-uint-border",
        "sample-feedback-gather-integer-sint-border-white",
        "sample-feedback-gather-integer-uint-array",
        "sample-feedback-gather-integer-sint-array-half",
        "sample-feedback-gather-integer-uint-lod-low",
        "sample-feedback-gather-integer-sint-lod-high",
        "sample-feedback-gather-integer-uint-lod-offset",
        "sample-feedback-gather-integer-uint-lod-resource-readonly-low",
        "sample-feedback-gather-integer-sint-lod-resource-readonly-high",
        "sample-feedback-gather-integer-uint-lod-resource-readonly-offset",
        "sample-feedback-gather-mirror-once-offset",
        "sample-feedback-gather-half-border-offset",
        "sample-feedback-gather-mirror-border-offset",
        "sample-feedback-gather-integer-uint-mirror-once-offset",
        "sample-feedback-gather-half-border",
        "sample-feedback-gather-half-mirror-border",
        "sample-feedback-gather-point","sample-feedback-gather-linear","sample-feedback-gather-border",
        "sample-feedback-gather-red","sample-feedback-gather-blue","sample-feedback-gather-alpha",
        "sample-feedback-gather-center","sample-feedback-gather-offset",
        "sample-feedback-gather-array","sample-feedback-gather-array-half",
        "sample-feedback-gather-compare","sample-feedback-gather-compare-border",
        "sample-feedback-gather-lod","sample-feedback-gather-lod-low",
        "sample-feedback-gather-lod-high","sample-feedback-gather-lod-minimum",
        "sample-feedback-gather-compare-lod",
        "sample-feedback-gather-linear-mip",
        "sample-feedback-gather-lod-resource-readonly",
        "sample-feedback-gather-lod-resource-readonly-low",
        "sample-feedback-gather-lod-resource-readonly-high",
        "sample-feedback-gather-lod-resource-readonly-minimum",
        "sample-feedback-gather-lod-offset","sample-feedback-gather-compare-lod-offset",
        "sample-feedback-gather-lod-resource-readonly-offset",
        "sample-feedback-gather-lod-clamp","sample-feedback-gather-lod-resource-readonly-clamp",
        "sample-feedback-gather-lod-bias","sample-feedback-gather-lod-bias-down",
        "sample-feedback-gather-lod-resource-readonly-bias",
        "sample-feedback-gather-lod-resource-readonly-bias-down",
        "sample-feedback-gather-lod-implicit","sample-feedback-gather-lod-resource-readonly-implicit",
        "sample-feedback-implicit-bias-down","sample-feedback-implicit-bias-deep",
        "sample-feedback-implicit-bias-combined","sample-feedback-implicit-bias-high",
        "sample-feedback-filter-min-linear","sample-feedback-filter-min-point",
        "sample-feedback-filter-mag-linear","sample-feedback-filter-mag-point","sample-feedback-filter-clamp-linear",
        "sample-feedback-reduce-min","sample-feedback-reduce-max",
        "sample-feedback-reduce-min-zero","sample-feedback-reduce-max-zero",
        "sample-feedback-reduce-mip-min","sample-feedback-reduce-mip-max",
        "sample-feedback-reduce-mip-min-zero","sample-feedback-reduce-mip-max-zero",
        "slice","slice-1d","mixed-slice","mixed-slice-read",
        "query-slice","query-slice-1d","query-slice-independent","atomic","atomic-slice",
        "atomic-swap-array","atomic-swap-return","atomic-inc","atomic-inc-wrap",
        "atomic-inc-max","atomic-dec","atomic-dec-zero","atomic-dec-above",
        "atomic-inc-contention","atomic-fmin-neg","atomic-fmin-pos",
        "atomic-fmax-neg","atomic-fmax-pos","atomic-fmin-neg-update","atomic-fmin-pos-update",
        "atomic-fmax-neg-update","atomic-fmax-pos-update","atomic-fmin-neg-float",
        "atomic-fcmp-match","atomic-fcmp-miss","atomic-fcmp-zero","atomic-fcmp-nan",
        "atomic-sub","atomic-sub-wrap","atomic-umin64","atomic-umax64","atomic-smin64","atomic-smax64",
        "atomic-add64","atomic-add-wrap64","atomic-sub64","atomic-sub-wrap64",
        "atomic-and64","atomic-or64","atomic-xor64","atomic-swap64",
        "atomic-cmp-match64","atomic-cmp-high64","atomic-cmp-low64",
        "atomic-inc64","atomic-inc-wrap64","atomic-inc-max64","atomic-dec64",
        "atomic-dec-zero64","atomic-dec-above64","atomic-inc-contention64"};
    if(std::find(std::begin(modes),std::end(modes),mode)==std::end(modes))
        throw std::invalid_argument("Unknown GCN fixture mode");
    printf("GCN fixture mode: %s\n",requested_mode.c_str());
    _putenv_s("SHADPS4_NATIVE_SHADER_CACHE","off");
    alignas(256) std::array<u32,128> code{};
    code[0]=0xBEEB03FF;code[1]=15;code[2]=0xBF810000;
    AmdGpu::BinaryInfo binary{};binary.signature=binary.signature_ref;
    binary.length=12;binary.shader_hash=0x4e41544956455431ULL;
    memcpy(code.data()+32,&binary,sizeof(binary));
    AmdGpu::Regs regs{};regs.stage_enable.raw=AmdGpu::ShaderStageEnable::Vs;
    regs.vs_program.address=reinterpret_cast<u64>(code.data())>>8;
    Shader::Profile profile{};profile.supported_spirv=0x00010500;
    profile.native_coherent_images=true;
    if(atomic64){profile.support_int64=true;profile.supports_buffer_int64_atomics=true;}
    NativeDX12::GuestShaderFrontend frontend(profile);
    const auto first=frontend.BuildGraphics(regs);
    const auto* stage=first.stages[u32(Shader::SwStage::Vertex)];
    if(!stage||stage->spirv.empty())throw std::runtime_error("No translated GCN vertex shader");
    const auto again=frontend.BuildGraphics(regs);
    if(again.stages[u32(Shader::SwStage::Vertex)]!=stage)
        throw std::runtime_error("Unchanged GCN program missed frontend cache");
    alignas(256) std::array<u32,128> reader{},writer{},integer_sampler_program{};
    const auto initialize=[&](auto& program,std::initializer_list<u32> words,u64 hash){
        std::copy(words.begin(),words.end(),program.begin());
        auto header=binary;header.length=u32(words.size()*4);header.shader_hash=hash;
        memcpy(program.data()+32,&header,sizeof(header));
    };
    initialize(reader,{0xBEEB03FF,15,0x7e000280,0x7e020280,0x7e040280,0x7e060280,
        0xf0000f00,0x00000400,0xf80008cf,0x07060504,0xbf810000},0x4e41544956455432ULL);
    initialize(writer,{0xBEEB03FF,15,0x7e000280,0x7e020280,0x7e040280,0x7e060280,
        0xf0200f00,0x00000000,0xbf810000},0x4e41544956455433ULL);
    regs.vs_program.address=reinterpret_cast<u64>(reader.data())>>8;
    regs.ps_program.address=reinterpret_cast<u64>(writer.data())>>8;
    regs.vs_program.settings.num_user_regs=regs.ps_program.settings.num_user_regs=8;
    regs.vs_program.settings.num_vgprs=regs.ps_program.settings.num_vgprs=1;
    AmdGpu::Image image{};image.base_address=0x100000;image.width=image.height=image.pitch=7;
    image.type=u64(AmdGpu::ImageType::Color2D);image.data_format=4;image.num_format=4;
    image.dst_sel_x=4;image.dst_sel_y=5;image.dst_sel_z=6;image.dst_sel_w=7;
    bool mixed_slice=false,array_reader=false,dimension_query=false,independent_query=false,atomic_image=false,buffer_resources=false;
    const auto bind=[&](u64 read_address,u64 write_address){
        image.base_address=read_address;auto read_view=image;
        if(array_reader){read_view.base_array=0;read_view.last_array=2;}
        memcpy(regs.vs_program.user_data.data(),&read_view,sizeof(read_view));
        image.base_address=write_address;auto write_view=image;
        if(resource_alias)write_view.min_lod=0;
        if(mixed_slice&&!array_reader){write_view.base_array=0;write_view.last_array=2;}
        if(dimension_query||(atomic_image&&!mode.ends_with("-float")))write_view.num_format=4;
        memcpy(regs.ps_program.user_data.data(),&write_view,sizeof(write_view));
        if(sample_readonly){
            write_view.base_address+=sample_destination_offset/256;
            memcpy(regs.ps_program.user_data.data()+8,&write_view,sizeof(write_view));
        }
        if(independent_query)memcpy(regs.ps_program.user_data.data()+8,&write_view,sizeof(write_view));
        if(buffer_resources){
            AmdGpu::Buffer buffer{};buffer.base_address=(write_address<<8)+32768;
            buffer.stride=4;buffer.num_records=4;buffer.data_format=4;buffer.num_format=4;
            buffer.dst_sel_x=4;buffer.dst_sel_y=5;buffer.dst_sel_z=6;buffer.dst_sel_w=7;
            memcpy(regs.ps_program.user_data.data(),&buffer,sizeof(buffer));
        }
    };
    bind(0x100000,0x100000);
    const auto shared=frontend.BuildGraphics(regs);
    const auto* shared_vs=shared.stages[u32(Shader::SwStage::Vertex)];
    const auto* shared_ps=shared.stages[u32(Shader::SwStage::Fragment)];
    if(shared_vs->info.images.size()!=1||!shared_vs->info.images[0].is_written||
       shared_ps->info.images.size()!=1||!shared_ps->info.images[0].is_written)
        throw std::runtime_error("GCN cross-stage alias was not unified as storage");
    if(shared_vs->bindings_start.unified!=shared_ps->bindings_end.unified)
        throw std::runtime_error("Cross-stage register allocation overlaps");
    bind(0x100000,0x100100);
    const auto* separate=frontend.BuildGraphics(regs).stages[u32(Shader::SwStage::Vertex)];
    if(separate==shared_vs||separate->info.images[0].is_written)
        throw std::runtime_error("Separating stage resources reused a promoted GCN shader");
    bind(0x100100,0x100100);
    if(frontend.BuildGraphics(regs).stages[u32(Shader::SwStage::Vertex)]!=shared_vs)
        throw std::runtime_error("Relocated cross-stage GCN alias missed shader cache");
    bind(0x100000,0x100100);
    if(frontend.BuildGraphics(regs).stages[u32(Shader::SwStage::Vertex)]!=separate)
        throw std::runtime_error("Restored independent GCN view missed shader cache");
    Shader::Backend::DX12::SpirvConvertOptions options;options.native_storage_buffer_uavs=true;
    if(atomic64)options.shader_model_minor=6;
    NativeDX12::GuestShaderCache cache(std::filesystem::path(argv[1]).wstring(),options);
    const auto& dxil=cache.Graphics(shared);
    if(dxil.stages[u32(Shader::SwStage::Vertex)].bytecode.empty()||
       dxil.stages[u32(Shader::SwStage::Fragment)].bytecode.empty())
        throw std::runtime_error("GCN cross-stage pipeline did not produce DXIL");
    if(&cache.Graphics(shared)!=&dxil)throw std::runtime_error("Cross-stage DXIL cache missed");
    if(mode=="base"){
        const auto signed_number=[](u32 number){return number==1||number==5;};
        auto packed_regs=regs;
        for(const u32 read_number:{0U,1U,4U,5U})for(const u32 write_number:{0U,1U,4U,5U}){
            const Shader::Info* alias_info{};
            for(const bool aliases:{true,false,true}){
                auto read_view=image;read_view.data_format=9;read_view.num_format=read_number;
                read_view.base_address=0x100000;
                auto write_view=read_view;write_view.num_format=write_number;
                write_view.base_address=aliases?read_view.base_address:read_view.base_address+0x100;
                memcpy(packed_regs.vs_program.user_data.data(),&read_view,sizeof(read_view));
                memcpy(packed_regs.ps_program.user_data.data(),&write_view,sizeof(write_view));
                const auto pipeline=frontend.BuildGraphics(packed_regs);
                const auto* vertex=pipeline.stages[u32(Shader::SwStage::Vertex)];
                const auto* fragment=pipeline.stages[u32(Shader::SwStage::Fragment)];
                if(vertex->info.images.size()!=1||fragment->info.images.size()!=1)
                    throw std::runtime_error("Packed GCN stage resources changed unexpectedly");
                const auto& read=vertex->info.images[0];const auto& write=fragment->info.images[0];
                const bool raw=aliases&&(signed_number(read_number)||signed_number(write_number));
                if(read.is_written!=aliases||!write.is_written||read.native_packed10_raw!=raw||
                   write.native_packed10_raw!=(raw||signed_number(write_number)))
                    throw std::runtime_error("Cross-stage packed GCN aliases lost numeric storage coherence");
                if(aliases){
                    if(alias_info&&alias_info!=&vertex->info)
                        throw std::runtime_error("Restored packed GCN alias missed frontend cache");
                    alias_info=&vertex->info;
                }
                const auto& compiled=cache.Graphics(pipeline);
                if(compiled.stages[u32(Shader::SwStage::Vertex)].bytecode.empty()||
                   compiled.stages[u32(Shader::SwStage::Fragment)].bytecode.empty())
                    throw std::runtime_error("Packed GCN pipeline did not produce both DXIL stages");
            }
        }
        const auto cross_format=[&](u32 read_format,u32 read_number,u32 write_format,u32 write_number){
            const NativeDX12::GuestShaderStage* shared_vertex{};
            const NativeDX12::GuestShaderStage* shared_fragment{};
            for(const bool aliases:{true,false,true}){
                auto read_view=image;read_view.data_format=read_format;read_view.num_format=read_number;
                read_view.base_address=0x100000;
                auto write_view=read_view;write_view.data_format=write_format;write_view.num_format=write_number;
                write_view.base_address+=aliases?0:0x100;
                memcpy(packed_regs.vs_program.user_data.data(),&read_view,sizeof(read_view));
                memcpy(packed_regs.ps_program.user_data.data(),&write_view,sizeof(write_view));
                const auto pipeline=frontend.BuildGraphics(packed_regs);
                const auto* vertex=pipeline.stages[u32(Shader::SwStage::Vertex)];
                const auto* fragment=pipeline.stages[u32(Shader::SwStage::Fragment)];
                if(vertex->info.images.size()!=1||fragment->info.images.size()!=1||
                   vertex->info.images[0].is_written!=aliases||!fragment->info.images[0].is_written||
                   vertex->info.images[0].native_raw32!=aliases||fragment->info.images[0].native_raw32!=aliases)
                    throw std::runtime_error("R32/RGBA8 GCN stage resources lost shared/separate representation");
                if(aliases){
                    if(shared_vertex&&(shared_vertex!=vertex||shared_fragment!=fragment))
                        throw std::runtime_error("Restored R32/RGBA8 GCN alias missed stage caches");
                    shared_vertex=vertex;shared_fragment=fragment;
                }
                const auto& compiled=cache.Graphics(pipeline);
                if(compiled.stages[u32(Shader::SwStage::Vertex)].bytecode.empty()||
                   compiled.stages[u32(Shader::SwStage::Fragment)].bytecode.empty())
                    throw std::runtime_error("R32/RGBA8 GCN pipeline did not produce both DXIL stages");
            }
        };
        for(u32 scalar:{4U,5U,7U})for(u32 rgba:{0U,1U,4U,5U,9U}){
            cross_format(4,scalar,10,rgba);cross_format(10,rgba,4,scalar);
        }
        AmdGpu::Sampler nearest{};
        nearest.xy_mag_filter.Assign(AmdGpu::Filter::Point);
        nearest.xy_min_filter.Assign(AmdGpu::Filter::Point);
        nearest.mip_filter.Assign(AmdGpu::MipFilter::None);
        nearest.max_lod.Assign(3840);
        nearest.clamp_x.Assign(AmdGpu::ClampMode::ClampLastTexel);
        nearest.clamp_y.Assign(AmdGpu::ClampMode::ClampLastTexel);
        initialize(integer_sampler_program,{0xBEEB03FF,15,
            0xbe9003ff,u32(nearest.raw0),0xbe9103ff,u32(nearest.raw0>>32),
            0xbe9203ff,u32(nearest.raw1),0xbe9303ff,u32(nearest.raw1>>32),
            0x7e0002ff,std::bit_cast<u32>(0.25f),0x7e0202ff,std::bit_cast<u32>(0.25f),
            0xf09c0f00,0x00800400,0xf80008cf,0x07060504,0xbf810000},
            0x4e41544956455479ULL);
        packed_regs.vs_program.address=reinterpret_cast<u64>(integer_sampler_program.data())>>8;
        for(const u32 format:{4U,9U})for(const u32 number:{4U,5U})for(const bool aliases:{true,false}){
            auto read_view=image;read_view.data_format=format;read_view.num_format=number;
            read_view.base_address=0x100000;
            auto write_view=read_view;write_view.base_address+=aliases?0:0x100;
            memcpy(packed_regs.vs_program.user_data.data(),&read_view,sizeof(read_view));
            memcpy(packed_regs.ps_program.user_data.data(),&write_view,sizeof(write_view));
            const auto pipeline=frontend.BuildGraphics(packed_regs);
            const auto* vertex=pipeline.stages[u32(Shader::SwStage::Vertex)];
            if(vertex->info.images.size()!=1||vertex->info.images[0].is_written!=aliases||
               vertex->info.samplers.size()!=1||!vertex->info.samplers[0].native_manual_sampling)
                throw std::runtime_error("Integer GCN nearest sampling did not use typed loads");
            const auto& compiled=cache.Graphics(pipeline);
            if(compiled.stages[u32(Shader::SwStage::Vertex)].bytecode.empty()||
               compiled.stages[u32(Shader::SwStage::Fragment)].bytecode.empty())
                throw std::runtime_error("Integer GCN nearest pipeline did not produce both DXIL stages");
        }
    }
    using namespace NativeDX12;
    ComPtr<ID3D12Debug> debug;Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));
    ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> adapter;
        const auto hr=factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter));
        if(hr==DXGI_ERROR_NOT_FOUND)break;Check(hr);DXGI_ADAPTER_DESC1 desc{};Check(adapter->GetDesc1(&desc));
        if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
        if(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))break;
    }
    if(!device)throw std::runtime_error("No hardware D3D12 adapter");
    if(atomic64&&!Capabilities::Query(device.Get()).buffer_int64_atomics)
        throw std::runtime_error("Native device lacks descriptor-backed buffer int64 atomics");
    ComPtr<ID3D12InfoQueue> messages;Check(device.As(&messages));
    auto* memory=static_cast<std::byte*>(VirtualAlloc(buffer_atomic?reinterpret_cast<void*>(0x6000000000ULL):nullptr,
        allocation_bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!memory)throw std::runtime_error("Guest fixture allocation failed");
    struct Allocation {void* p;~Allocation(){VirtualFree(p,0,MEM_RELEASE);}} allocation{memory};
    const bool float_compare=mode.starts_with("atomic-fcmp");
    const u32 compare_initial=mode=="atomic-fcmp-zero"?0x80000000U:
        mode=="atomic-fcmp-nan"?0x7fc01234U:mode=="atomic-fcmp-miss"?0x40000000U:0x3f800000U;
    const u32 compare_operand=mode=="atomic-fcmp-zero"?0U:
        mode=="atomic-fcmp-nan"?0x7fc01234U:0x3f800000U;
    const u32 compare_result=mode=="atomic-fcmp-match"||mode=="atomic-fcmp-zero"?0x40400000U:compare_initial;
    const bool float_atomic=float_compare||mode.starts_with("atomic-fmin")||mode.starts_with("atomic-fmax");
    const bool float_max=mode.starts_with("atomic-fmax");
    const bool float_negative=mode.find("-neg")!=std::string_view::npos;
    const bool float_update=mode.ends_with("-update");
    float float_initial=float_negative?(float_max?-1.f:-2.f):(float_max?3.f:2.f);
    float float_operand=float_negative?(float_max?-2.f:-1.f):(float_max?2.f:3.f);
    if(float_update)std::swap(float_initial,float_operand);
    const float float_result=float_max?std::max(float_initial,float_operand):std::min(float_initial,float_operand);
    const bool subtract_atomic=mode.starts_with("atomic-sub");
    const u32 subtract_initial=mode=="atomic-sub-wrap"?0:5;
    const u32 subtract_result=subtract_initial-3U;
    const bool bounded_atomic=mode.starts_with("atomic-inc")||mode.starts_with("atomic-dec");
    const bool atomic_contention=mode=="atomic-inc-contention"||mode=="atomic-inc-contention64";
    const bool decrement=mode.starts_with("atomic-dec");
    const u32 initial_atomic=mode=="atomic-inc-max"?UINT32_MAX:
        (mode=="atomic-inc-wrap"||mode=="atomic-dec"?2:mode=="atomic-dec-above"?3:0);
    const u32 atomic_limit=mode=="atomic-inc-max"||atomic_contention?UINT32_MAX:2;
    const u32 bounded_result=atomic_contention?4096:decrement?
        (initial_atomic==0||initial_atomic>atomic_limit?atomic_limit:initial_atomic-1):
        (initial_atomic>=atomic_limit?0:initial_atomic+1);
    const bool atomic_return=mode=="atomic-swap-return"||bounded_atomic||float_atomic||subtract_atomic;
    const bool atomic_swap_array=mode=="atomic-swap-array"||atomic_return;
    atomic_image=mode=="atomic"||mode=="atomic-slice"||atomic_swap_array;
    image.data_format=atomic_image||sample_depth||gather_compare?4:14;image.num_format=gather_integer?(mode.find("-sint")!=std::string::npos?5:4):7;image.tiling_index=13;
    if(depth_array||gather_array){image.type=u64(AmdGpu::ImageType::Color2DArray);image.depth=2;image.base_array=1;image.last_array=2;}
    if(depth_1d){image.type=u64(depth_array?AmdGpu::ImageType::Color1DArray:AmdGpu::ImageType::Color1D);image.height=0;}
    if(sample_volume){image.type=u64(AmdGpu::ImageType::Color3D);image.depth=7;}
    if(aniso_cube){image.type=u64(AmdGpu::ImageType::Cube);image.depth=1;image.last_array=11;}
    const bool sample_lz=mode.starts_with("sample-feedback-lz-");
    const bool sample_grad=mode.starts_with("sample-feedback-grad");
    const bool sample_implicit=mode.starts_with("sample-feedback-implicit");
    const bool implicit_clamp=mode.starts_with("sample-feedback-implicit-clamp");
    const bool shader_clamp=mode.starts_with("sample-feedback-grad-clamp");
    const bool sample_filter=mode.starts_with("sample-feedback-filter-");
    const bool sample_border=mode.starts_with("sample-feedback-border-");
    const bool border_custom=sample_border&&mode.find("-custom")!=std::string::npos;
    std::array<std::array<float,4>,8> border_table{};
    border_table[7]={0.25f,-0.5f,2.f,0.75f};
    const bool border_half=sample_border&&mode.find("-half")!=std::string::npos;
    const bool border_mirror=sample_border&&mode.find("-mirror")!=std::string::npos;
    const bool border_linear=sample_border&&mode.find("-linear")!=std::string::npos;
    const bool border_inside=sample_border&&mode.find("-inside")!=std::string::npos;
    const bool border_edge=sample_border&&(mode.ends_with("-edge")||border_linear);
    const float border_weight=border_inside?0.4375f:border_half?(border_edge?0.75f:0.f):
        mode.ends_with("-edge")?0.75f:1.f;
    const bool border_white=sample_border&&mode.find("-white")!=std::string::npos;
    const bool border_opaque=sample_border&&mode.find("-opaque")!=std::string::npos;
    const bool sample_reduce=mode.starts_with("sample-feedback-reduce-");
    const bool reduce_max=sample_reduce&&mode.find("-max")!=std::string::npos;
    const bool reduce_mip=sample_reduce&&mode.find("-mip-")!=std::string::npos;
    const bool reduce_zero=sample_reduce&&mode.ends_with("-zero");
    const bool filter_min=sample_filter&&(mode.find("-min-")!=std::string::npos||mode.find("-clamp-")!=std::string::npos);
    const bool filter_linear=sample_filter&&mode.ends_with("-linear");
    const bool sample_bias=sample_implicit&&mode.find("-bias-")!=std::string::npos;
    const u32 implicit_level=implicit_clamp?2:mode.ends_with("-zero")?0:mode.ends_with("-deep")?1:
        mode.ends_with("-down")||mode.ends_with("-combined")?2:3;
    const bool sample_lod=sample_depth||sample_reduce||sample_filter||sample_implicit||sample_grad||sample_lz||mode.starts_with("sample-feedback-lod-");
    const bool sample_lod_linear=mode.starts_with("sample-feedback-lod-linear")||mode.starts_with("sample-feedback-lz-linear")||
        (shader_clamp&&mode.ends_with("-linear"))||aniso_mip_linear;
    const bool sample_lod_dynamic=mode=="sample-feedback-lod-linear-dynamic";
    const bool resource_lod=sample_lod&&mode.find("-resource")!=std::string::npos;
    if(mode.starts_with("sample-feedback")){
        regs.vs_program.settings.num_user_regs=regs.ps_program.settings.num_user_regs=12;
        if(sample_readonly){regs.ps_program.settings.num_user_regs=16;regs.ps_program.settings.num_sgprs=2;}
        AmdGpu::Sampler sampler{};
        sampler.clamp_x.Assign(AmdGpu::ClampMode::ClampLastTexel);
        sampler.clamp_y.Assign(AmdGpu::ClampMode::ClampLastTexel);
        sampler.clamp_z.Assign(AmdGpu::ClampMode::ClampLastTexel);
        const bool wrap=mode.find("-wrap")!=std::string::npos;
        const bool mirror=mode.find("-mirror")!=std::string::npos;
        const bool mirror_once=mode.find("-mirror-once")!=std::string::npos;
        if(wrap||mirror){
            sampler.clamp_x.Assign(wrap?AmdGpu::ClampMode::Wrap:AmdGpu::ClampMode::Mirror);
            sampler.clamp_y.Assign(wrap?AmdGpu::ClampMode::Wrap:AmdGpu::ClampMode::Mirror);
        }
        if(mirror_once){
            sampler.clamp_x.Assign(AmdGpu::ClampMode::MirrorOnceLastTexel);
            sampler.clamp_y.Assign(AmdGpu::ClampMode::MirrorOnceLastTexel);
        }
        const bool linear=mode.find("-linear")!=std::string::npos;
        if(linear){
            sampler.xy_min_filter.Assign(AmdGpu::Filter::Bilinear);
            sampler.xy_mag_filter.Assign(AmdGpu::Filter::Bilinear);
        }
        if(mode.find("-aniso-one")!=std::string::npos){
            sampler.xy_min_filter.Assign(linear?AmdGpu::Filter::AnisoLinear:AmdGpu::Filter::AnisoPoint);
            sampler.xy_mag_filter.Assign(linear?AmdGpu::Filter::AnisoLinear:AmdGpu::Filter::AnisoPoint);
            sampler.max_aniso.Assign(AmdGpu::AnisoRatio::One);
        }
        if(aniso_case){
            sampler.xy_min_filter.Assign(aniso_linear?AmdGpu::Filter::AnisoLinear:AmdGpu::Filter::AnisoPoint);
            sampler.xy_mag_filter.Assign(aniso_linear?AmdGpu::Filter::AnisoLinear:AmdGpu::Filter::AnisoPoint);
            sampler.max_aniso.Assign(aniso_limit==16?AmdGpu::AnisoRatio::Sixteen:
                aniso_limit==8?AmdGpu::AnisoRatio::Eight:aniso_limit==4?AmdGpu::AnisoRatio::Four:AmdGpu::AnisoRatio::Two);
            if(mode.ends_with("-reduce-min"))sampler.filter_mode.Assign(AmdGpu::FilterMode::Min);
            if(mode.ends_with("-reduce-max"))sampler.filter_mode.Assign(AmdGpu::FilterMode::Max);
        }
        if(mode.ends_with("-unnormalized"))sampler.force_unnormalized.Assign(1);
        if(sample_grad&&mode.ends_with("-bias"))sampler.lod_bias.Assign(256);
        if(aniso_case&&mode.ends_with("-bias"))sampler.lod_bias.Assign(256);
        if(aniso_case&&mode.ends_with("-cancel-high"))sampler.lod_bias.Assign(20*256);
        if(aniso_case&&mode.ends_with("-cancel-low"))sampler.lod_bias.Assign(u32(-20*256)&0x3fff);
        if(sample_bias&&mode.ends_with("-combined"))sampler.lod_bias.Assign(256);
        if(sample_lod){
            image.last_level=3;
            if(resource_lod){
                image.min_lod=mode.ends_with("-limit")?3840:mode.ends_with("-below-base")?128:
                    sample_lod_linear?384:512;
                if(mode.ends_with("-base"))image.base_level=1;
            }
            sampler.max_lod.Assign(mode.ends_with("-max")&&mode.find("-reduce-")==std::string::npos?256:3840);
            sampler.min_lod.Assign(mode.ends_with("-min")&&mode.find("-reduce-")==std::string::npos?512:0);
            sampler.mip_filter.Assign(mode.ends_with("-none")?AmdGpu::MipFilter::None:
                sample_lod_linear?AmdGpu::MipFilter::Linear:AmdGpu::MipFilter::Point);
        }
        if(sample_filter){
            sampler.xy_min_filter.Assign((filter_min?filter_linear:!filter_linear)?AmdGpu::Filter::Bilinear:AmdGpu::Filter::Point);
            sampler.xy_mag_filter.Assign((filter_min?!filter_linear:filter_linear)?AmdGpu::Filter::Bilinear:AmdGpu::Filter::Point);
            if(mode.find("-clamp-")!=std::string::npos)image.min_lod=256;
        }
        if(sample_reduce){
            sampler.min_lod.Assign(0);sampler.max_lod.Assign(3840);
            sampler.filter_mode.Assign(reduce_max?AmdGpu::FilterMode::Max:AmdGpu::FilterMode::Min);
            sampler.xy_min_filter.Assign(reduce_mip?AmdGpu::Filter::Point:AmdGpu::Filter::Bilinear);
            sampler.xy_mag_filter.Assign(reduce_mip?AmdGpu::Filter::Point:AmdGpu::Filter::Bilinear);
            sampler.mip_filter.Assign(reduce_mip?AmdGpu::MipFilter::Linear:AmdGpu::MipFilter::Point);
        }
        if(sample_border){
            const auto addressing=border_half?(border_mirror?AmdGpu::ClampMode::MirrorOnceHalfBorder:AmdGpu::ClampMode::ClampHalfBorder):
                border_mirror?AmdGpu::ClampMode::MirrorOnceBorder:AmdGpu::ClampMode::ClampBorder;
            sampler.clamp_x.Assign(addressing);sampler.clamp_y.Assign(addressing);
            sampler.border_color_type.Assign(border_white?AmdGpu::BorderColor::White:
                border_opaque?AmdGpu::BorderColor::OpaqueBlack:border_custom?AmdGpu::BorderColor::Custom:AmdGpu::BorderColor::TransparentBlack);
            if(border_custom)sampler.border_color_ptr.Assign(7);
            sampler.xy_min_filter.Assign(border_edge?AmdGpu::Filter::Bilinear:AmdGpu::Filter::Point);
            sampler.xy_mag_filter.Assign(border_edge?AmdGpu::Filter::Bilinear:AmdGpu::Filter::Point);
        }
        memcpy(regs.vs_program.user_data.data()+8,&sampler,sizeof(sampler));
        memcpy(regs.ps_program.user_data.data()+8,&sampler,sizeof(sampler));
        const float uv=aniso_case?(aniso_uv+(aniso_cube?1.f:0.f))*(mode.ends_with("-unnormalized")?8.f:1.f):sample_border?(border_inside?-0.03125f:mode.find("-high-")!=std::string::npos?4.f:border_linear||border_half?-4.f:border_edge?0.f:-4.f):mirror_once?(mode.ends_with("-low")?-1.e30f:mode.ends_with("-high")?1.e30f:
            (mode.ends_with("-negative")?-1.f:1.f)*(linear?0.125f:0.0625f)):
            sample_reduce?(reduce_mip?0.f:reduce_zero?0.0625f:0.125f):sample_filter?(filter_min?0.25f:0.125f):sample_lod?0.f:mode.ends_with("-seam")?1.f:
            wrap?(mode.ends_with("-negative")?-0.875f:1.125f):
            mirror?(mode.ends_with("-negative")?-0.125f:1.875f):
            mode.ends_with("-low")?-1.e30f:mode.ends_with("-high")?1.e30f:
            mode.ends_with("-unnormalized")?1.f:linear?0.125f:0.f;
        const u32 coordinate=std::bit_cast<u32>(uv);
        const float explicit_level=aniso_case&&mode.ends_with("-cancel-high")?-16.f:aniso_case&&mode.ends_with("-cancel-low")?20.f:
            sample_reduce?(reduce_mip?(reduce_zero?1.f:1.5f):0.f):sample_filter?(filter_min&&mode.find("-clamp-")==std::string::npos?1.f:0.f):sample_implicit?float(implicit_level):resource_lod?0.f:mode.ends_with("-round-low")?1.25f:mode.ends_with("-round-high")?1.75f:
            mode.ends_with("-low")?-8.f:mode.ends_with("-high")?99.f:
            mode.ends_with("-min")?0.f:mode.ends_with("-max")?3.f:
            sample_lod_linear?1.5f:1.f;
        const u32 sample_opcode=sample_lod&&!sample_lz?0xf0900f00U:0xf09c0f00U;
        const u32 lod_instruction=sample_lod_dynamic?0xf0000100U:0x7e0402ffU;
        const u32 lod_operand=sample_lod_dynamic?0x00000200U:std::bit_cast<u32>(explicit_level);
        initialize(reader,{0xBEEB03FF,15,0x7e0002ff,coordinate,0x7e0202ff,coordinate,
            lod_instruction,lod_operand,sample_opcode,0x00400400,
            0xf80008cf,0x07060504,0xbf810000},0x4e41544956455470ULL);
        initialize(writer,{0xBEEB03FF,15,0x7e0002ff,coordinate,0x7e0202ff,coordinate,
            lod_instruction,resource_alias?std::bit_cast<u32>(2.f):lod_operand,sample_opcode,0x00400400,0x7e000282,0x7e020280,
            0xf0200f00,0x00000400,0xbf810000},0x4e41544956455471ULL);
        if(sample_readonly){
            initialize(writer,{0xBEEB03FF,15,
                0xbe9003ff,u32(sampler.raw0),0xbe9103ff,u32(sampler.raw0>>32),
                0xbe9203ff,u32(sampler.raw1),0xbe9303ff,u32(sampler.raw1>>32),
                0x7e0002ff,sample_border?coordinate:0U,0x7e0202ff,sample_border?coordinate:0U,0x7e040280,sample_opcode,0x00800400,
                0x7e000282,0x7e020280,0xf0200f00,0x00020400,0xbf810000},0x4e41544956455472ULL);
        }
        if(sample_grad){
            const bool skew=mode.ends_with("-skew");
            const float gradient=skew?0.3125f:mode.ends_with("-zero")?0.f:mode.ends_with("-negative")?-0.5f:
                mode.ends_with("-unnormalized")?4.f:0.5f;
            const u32 dx=std::bit_cast<u32>(mode.ends_with("-y")?0.f:gradient),dy=std::bit_cast<u32>(skew?0.f:gradient);
            const u32 dx_v=std::bit_cast<u32>(skew?gradient:0.f);
            initialize(reader,{0xBEEB03FF,15,0x7e0002ff,dx,0x7e0202ff,dx_v,0x7e040280,0x7e0602ff,dy,
                0x7e080280,0x7e0a0280,0xf0880f00,0x00400400,0xf80008cf,0x07060504,0xbf810000},0x4e41544956455473ULL);
            initialize(writer,{0xBEEB03FF,15,0x7e0002ff,dx,0x7e0202ff,dx_v,0x7e040280,0x7e0602ff,dy,
                0x7e080280,0x7e0a0280,0xf0880f00,0x00400400,0x7e000282,0x7e020280,
                0xf0200f00,0x00000400,0xbf810000},0x4e41544956455474ULL);
            if(aniso_case){
                const float scale=mode.ends_with("-unnormalized")?8.f:1.f;
                const auto literal=[&](float value){return std::bit_cast<u32>(value*scale);};
                initialize(reader,{0xBEEB03FF,15,0x7e0002ff,literal(aniso_dx[0]),0x7e0202ff,literal(aniso_dx[1]),
                    0x7e0402ff,literal(aniso_dy[0]),0x7e0602ff,literal(aniso_dy[1]),
                    0x7e0802ff,coordinate,0x7e0a02ff,coordinate,0xf0880f00,0x00400400,
                    0xf80008cf,0x07060504,0xbf810000},0x4e415449564554e0ULL);
                initialize(writer,{0xBEEB03FF,15,0x7e0002ff,literal(aniso_dx[0]),0x7e0202ff,literal(aniso_dx[1]),
                    0x7e0402ff,literal(aniso_dy[0]),0x7e0602ff,literal(aniso_dy[1]),
                    0x7e0802ff,coordinate,0x7e0a02ff,coordinate,0xf0880f00,0x00400400,
                    0x7e000282,0x7e020280,0xf0200f00,0x00000400,0xbf810000},0x4e415449564554e1ULL);
                if(sample_readonly)
                    initialize(writer,{0xBEEB03FF,15,
                        0xbe9003ff,u32(sampler.raw0),0xbe9103ff,u32(sampler.raw0>>32),
                        0xbe9203ff,u32(sampler.raw1),0xbe9303ff,u32(sampler.raw1>>32),
                        0x7e0002ff,literal(aniso_dx[0]),0x7e0202ff,literal(aniso_dx[1]),
                        0x7e0402ff,literal(aniso_dy[0]),0x7e0602ff,literal(aniso_dy[1]),
                        0x7e0802ff,coordinate,0x7e0a02ff,coordinate,0xf0880f00,0x00800400,
                        0x7e000282,0x7e020280,0xf0200f00,0x00020400,0xbf810000},0x4e415449564554e2ULL);
                if(aniso_cube){
                    const u32 face=std::bit_cast<u32>(aniso_cube_array?13.f:5.f);
                    initialize(reader,{0xBEEB03FF,15,0x7e0002ff,literal(aniso_dx[0]),0x7e0202ff,literal(aniso_dx[1]),
                        0x7e0402ff,literal(aniso_dy[0]),0x7e0602ff,literal(aniso_dy[1]),
                        0x7e0802ff,coordinate,0x7e0a02ff,coordinate,0x7e0c02ff,face,
                        0xf0880f00,0x00400400,0xf80008cf,0x07060504,0xbf810000},0x4e415449564554e3ULL);
                    if(sample_readonly)
                        initialize(writer,{0xBEEB03FF,15,
                            0xbe9003ff,u32(sampler.raw0),0xbe9103ff,u32(sampler.raw0>>32),
                            0xbe9203ff,u32(sampler.raw1),0xbe9303ff,u32(sampler.raw1>>32),
                            0x7e0002ff,literal(aniso_dx[0]),0x7e0202ff,literal(aniso_dx[1]),
                            0x7e0402ff,literal(aniso_dy[0]),0x7e0602ff,literal(aniso_dy[1]),
                            0x7e0802ff,coordinate,0x7e0a02ff,coordinate,0x7e0c02ff,face,0xf0880f00,0x00800400,
                            0x7e000282,0x7e020280,0x7e040280,0xf0200f00,0x00020400,0xbf810000},0x4e415449564554e4ULL);
                    else
                        initialize(writer,{0xBEEB03FF,15,0x7e0002ff,literal(aniso_dx[0]),0x7e0202ff,literal(aniso_dx[1]),
                            0x7e0402ff,literal(aniso_dy[0]),0x7e0602ff,literal(aniso_dy[1]),
                            0x7e0802ff,coordinate,0x7e0a02ff,coordinate,0x7e0c02ff,face,0xf0880f00,0x00400400,
                            0x7e000282,0x7e020280,0x7e040280,0xf0200f00,0x00000400,0xbf810000},0x4e415449564554e5ULL);
                }
            }
            if(shader_clamp){
                const u32 minimum=std::bit_cast<u32>(mode.ends_with("-low")?-1.f:sample_lod_linear?1.5f:2.f);
                // IMAGE_SAMPLE_D_CL consumes dx.xy, dy.xy, uv.xy, then MinLOD.
                // Zero gradients ensure the instruction's floor selects the mip.
                initialize(reader,{0xBEEB03FF,15,0x7e000280,0x7e020280,0x7e040280,0x7e060280,
                    0x7e080280,0x7e0a0280,0x7e0c02ff,minimum,0xf08c0f00,0x00400400,
                    0xf80008cf,0x07060504,0xbf810000},0x4e41544956455479ULL);
                initialize(writer,{0xBEEB03FF,15,0x7e000280,0x7e020280,0x7e040280,0x7e060280,
                    0x7e080280,0x7e0a0280,0x7e0c02ff,minimum,0xf08c0f00,0x00400400,
                    0x7e000282,0x7e020280,0xf0200f00,0x00000400,0xbf810000},0x4e4154495645547aULL);
                if(sample_readonly)
                    initialize(writer,{0xBEEB03FF,15,
                        0xbe9003ff,u32(sampler.raw0),0xbe9103ff,u32(sampler.raw0>>32),
                        0xbe9203ff,u32(sampler.raw1),0xbe9303ff,u32(sampler.raw1>>32),
                        0x7e000280,0x7e020280,0x7e040280,0x7e060280,
                        0x7e080280,0x7e0a0280,0x7e0c02ff,minimum,0xf08c0f00,0x00800400,
                        0x7e000282,0x7e020280,0xf0200f00,0x00020400,0xbf810000},0x4e4154495645547bULL);
                if(mode.ends_with("-offset")){
                    initialize(reader,{0xBEEB03FF,15,0x7e000281,0x7e020280,0x7e040280,0x7e060280,
                        0x7e080280,0x7e0a0280,0x7e0c0280,0x7e0e02ff,minimum,0xf0cc0f00,0x00400400,
                        0xf80008cf,0x07060504,0xbf810000},0x4e4154495645547cULL);
                    initialize(writer,{0xBEEB03FF,15,0x7e000281,0x7e020280,0x7e040280,0x7e060280,
                        0x7e080280,0x7e0a0280,0x7e0c0280,0x7e0e02ff,minimum,0xf0cc0f00,0x00400400,
                        0x7e000282,0x7e020280,0xf0200f00,0x00000400,0xbf810000},0x4e4154495645547dULL);
                }
            }
        }
        if(sample_implicit){
            regs.ps_input_addr.pos_x_float_ena=regs.ps_input_ena.pos_x_float_ena=1;
            regs.ps_input_addr.pos_y_float_ena=regs.ps_input_ena.pos_y_float_ena=1;
            if(mode.ends_with("-zero"))
                initialize(writer,{0xBEEB03FF,15,0x7e000280,0x7e020280,
                    0xf0800f00,0x00400400,0x7e000282,0x7e020280,0xf0200f00,0x00000400,0xbf810000},0x4e41544956455476ULL);
            else
                initialize(writer,{0xBEEB03FF,15,0xf0800f00,0x00400400,
                    0x7e000282,0x7e020280,0xf0200f00,0x00000400,0xbf810000},0x4e41544956455475ULL);
            if(sample_bias){
                const float bias=mode.ends_with("-high")?1.f:mode.ends_with("-down")?-1.f:-2.f;
                initialize(writer,{0xBEEB03FF,15,0x7e040301,0x7e020300,0x7e0002ff,std::bit_cast<u32>(bias),
                    0xf0940f00,0x00400400,0x7e000282,0x7e020280,0xf0200f00,0x00000400,0xbf810000},0x4e41544956455477ULL);
            }
        }
        if(implicit_clamp){
            const bool with_bias=mode.find("-bias")!=std::string::npos;
            const bool with_offset=mode.ends_with("-offset");
            const u32 op=with_bias?(with_offset?54U:38U):(with_offset?49U:33U);
            std::array<u32,5> arguments{};
            unsigned arg=0;
            if(with_offset)arguments[arg++]=1U;
            if(with_bias)arguments[arg++]=std::bit_cast<u32>(-1.f);
            arg+=2;
            arguments[arg]=std::bit_cast<u32>(2.f);
            initialize(writer,{0xBEEB03FF,15,
                0xbe9003ff,u32(sampler.raw0),0xbe9103ff,u32(sampler.raw0>>32),
                0xbe9203ff,u32(sampler.raw1),0xbe9303ff,u32(sampler.raw1>>32),
                0x7e0002ff,arguments[0],0x7e0202ff,arguments[1],0x7e0402ff,arguments[2],
                0x7e0602ff,arguments[3],0x7e0802ff,arguments[4],
                0xf0000f00U|(op<<18),0x00800400,0x7e000282,0x7e020280,
                0xf0200f00,sample_readonly?0x00020400U:0x00000400U,0xbf810000},0x4e41544956455480ULL+op);
            if(mode.ends_with("-gradient"))
                initialize(writer,{0xBEEB03FF,15,0x7e040301,0x7e020300,
                    0x7e0002ff,std::bit_cast<u32>(-2.f),0x7e0602ff,std::bit_cast<u32>(2.f),
                    0xf0980f00,0x00400400,0x7e000282,0x7e020280,
                    0xf0200f00,0x00000400,0xbf810000},0x4e415449564554c0ULL);
        }
        if(sample_depth){
            const auto comparison=mode.ends_with("-never")?AmdGpu::DepthCompare::Never:
                mode.ends_with("-less-equal")?AmdGpu::DepthCompare::LessEqual:
                mode.ends_with("-greater-equal")?AmdGpu::DepthCompare::GreaterEqual:
                mode.ends_with("-not-equal")?AmdGpu::DepthCompare::NotEqual:
                mode.ends_with("-equal")?AmdGpu::DepthCompare::Equal:
                mode.ends_with("-greater")?AmdGpu::DepthCompare::Greater:
                mode.ends_with("-always")?AmdGpu::DepthCompare::Always:AmdGpu::DepthCompare::Less;
            sampler.depth_compare_func.Assign(comparison);
            const bool with_bias=mode.find("-bias")!=std::string::npos,with_offset=mode.ends_with("-offset");
            const u32 op=with_bias?46U:with_offset?57U:41U;
            std::array<u32,6> arguments{};unsigned arg=0;
            if(with_offset)arguments[arg++]=1U;
            if(with_bias)arguments[arg++]=std::bit_cast<u32>(-1.f);
            arguments[arg++]=std::bit_cast<u32>(0.5f);
            const u32 coordinate=std::bit_cast<u32>(mode.ends_with("-linear")?0.5f:0.f);
            arguments[arg++]=coordinate;if(!depth_1d)arguments[arg++]=coordinate;
            if(depth_array)arguments[arg++]=std::bit_cast<u32>(mode.ends_with("-low")?-1.e30f:
                mode.ends_with("-high")?1.e30f:mode.ends_with("-half")?0.5f:mode.ends_with("-upper")?0.51f:1.f);
            arguments[arg]=std::bit_cast<u32>(2.f);
            initialize(writer,{0xBEEB03FF,15,
                0xbe9003ff,u32(sampler.raw0),0xbe9103ff,u32(sampler.raw0>>32),
                0xbe9203ff,u32(sampler.raw1),0xbe9303ff,u32(sampler.raw1>>32),
                0x7e0002ff,arguments[0],0x7e0202ff,arguments[1],0x7e0402ff,arguments[2],
                0x7e0602ff,arguments[3],0x7e0802ff,arguments[4],
                0x7e0a02ff,arguments[5],
                0xf0000100U|(op<<18)|(depth_array?0x4000U:0U),0x00800400,0x7e000282,0x7e020280,0x7e040280,
                depth_array?0xf0204100U:0xf0200100U,depth_shared?0x00000400U:0x00020400U,0xbf810000},0x4e415449564555c0ULL+op);
            if(mode.find("-grad")!=std::string::npos){
                initialize(writer,{0xBEEB03FF,15,
                    0xbe9003ff,u32(sampler.raw0),0xbe9103ff,u32(sampler.raw0>>32),
                    0xbe9203ff,u32(sampler.raw1),0xbe9303ff,u32(sampler.raw1>>32),
                    0x7e0002ff,std::bit_cast<u32>(0.5f),0x7e020280,0x7e040280,0x7e060280,0x7e080280,
                    0x7e0a02ff,coordinate,0x7e0c02ff,coordinate,0x7e0e02ff,std::bit_cast<u32>(2.f),
                    0xf0ac0100,0x00800400,0x7e000282,0x7e020280,
                    0xf0200100,depth_shared?0x00000400U:0x00020400U,0xbf810000},0x4e415449564556c0ULL);
                if(depth_1d)
                    initialize(writer,{0xBEEB03FF,15,
                        0xbe9003ff,u32(sampler.raw0),0xbe9103ff,u32(sampler.raw0>>32),
                        0xbe9203ff,u32(sampler.raw1),0xbe9303ff,u32(sampler.raw1>>32),
                        0x7e0002ff,std::bit_cast<u32>(0.5f),0x7e0202ff,std::bit_cast<u32>(0.5f),0x7e040280,
                        0x7e0602ff,coordinate,0x7e0802ff,depth_array?std::bit_cast<u32>(1.f):0U,0x7e0a0280,
                        depth_array?0xf0ac4100U:0xf0ac0100U,0x00800400,0x7e000282,0x7e020280,
                        depth_array?0xf0204100U:0xf0200100U,depth_shared?0x00000400U:0x00020400U,0xbf810000},0x4e415449564556c3ULL);
            }
            if(mode.ends_with("-after-write")){
                memcpy(regs.ps_program.user_data.data()+8,&sampler,sizeof(sampler));
                initialize(writer,{0xBEEB03FF,15,
                    0x7e000280,0x7e020280,0x7e040282,0x7e0802ff,std::bit_cast<u32>(0.75f),
                    0xf0240100,0x00000400,0xbf8c0000,
                    0x7e0002ff,std::bit_cast<u32>(0.5f),0x7e020280,0x7e040280,
                    0x7e0602ff,std::bit_cast<u32>(2.f),0xf0a40100,0x00400400,
                    0x7e000282,0x7e020280,0xf0200100,0x00000400,0xbf810000},0x4e415449564556c1ULL);
            }
            if(sample_volume){
                const bool filtered=mode.find("-linear")!=std::string::npos;
                sampler.z_filter.Assign(filtered&&!mode.ends_with("-xy")?2:1);
                if(mode.ends_with("-linear-z")){
                    sampler.xy_min_filter.Assign(AmdGpu::Filter::Point);
                    sampler.xy_mag_filter.Assign(AmdGpu::Filter::Point);
                }
                if(mode.ends_with("-border"))sampler.clamp_z.Assign(AmdGpu::ClampMode::ClampBorder);
                if(mode.ends_with("-wrap"))sampler.clamp_z.Assign(AmdGpu::ClampMode::Wrap);
                if(mode.ends_with("-mirror"))sampler.clamp_z.Assign(AmdGpu::ClampMode::Mirror);
                if(mode.ends_with("-mip"))sampler.mip_filter.Assign(AmdGpu::MipFilter::Linear);
                memcpy(regs.ps_program.user_data.data()+8,&sampler,sizeof(sampler));
                const float uv=filtered?0.5f:0.25f;
                const float z=mode.ends_with("-border")?0.f:
                    mode.ends_with("-wrap")||mode.ends_with("-mirror")?1.f:
                    mode.ends_with("-point-z")?0.75f:uv;
                initialize(writer,{0xBEEB03FF,15,
                    0x7e0002ff,std::bit_cast<u32>(uv),0x7e0202ff,std::bit_cast<u32>(uv),
                    0x7e0402ff,std::bit_cast<u32>(z),
                    0x7e0602ff,std::bit_cast<u32>(mode.ends_with("-mip")?1.5f:2.f),0xf0900100,0x00400400,
                    0x7e000282,0x7e020280,0x7e040280,0xf0200100,0x00000400,0xbf810000},0x4e415449564557c0ULL);
                if(mode.find("-grad-")!=std::string::npos){
                    regs.ps_program.settings.num_vgprs=2;
                    const bool dy=mode.ends_with("-dy-z");
                    // Only the Z derivative supplies the footprint: 8 * 0.5 selects mip 2.
                    initialize(writer,{0xBEEB03FF,15,
                        0x7e000280,0x7e020280,0x7e0402ff,std::bit_cast<u32>(dy?0.f:0.5f),
                        0x7e060280,0x7e080280,0x7e0a02ff,std::bit_cast<u32>(dy?0.5f:0.f),
                        0x7e0c02ff,std::bit_cast<u32>(0.5f),0x7e0e02ff,std::bit_cast<u32>(0.5f),
                        0x7e1002ff,std::bit_cast<u32>(0.5f),0xf0880100,0x00400400,
                        0x7e000282,0x7e020280,0x7e040280,0xf0200100,0x00000400,0xbf810000},0x4e415449564557c1ULL);
                }
            }
        }
        if(sample_gather){
            if(mode.find("-aniso")!=std::string::npos){
                sampler.xy_mag_filter.Assign(AmdGpu::Filter::AnisoPoint);
                sampler.xy_min_filter.Assign(AmdGpu::Filter::AnisoLinear);
                sampler.max_aniso.Assign(AmdGpu::AnisoRatio::Sixteen);
            }
            sampler.depth_compare_func.Assign(AmdGpu::DepthCompare::LessEqual);
            if(mode.ends_with("-linear-mip"))sampler.mip_filter.Assign(AmdGpu::MipFilter::Linear);
            if(gather_lod){
                image.last_level=3;
                sampler.mip_filter.Assign(AmdGpu::MipFilter::Point);
                sampler.max_lod.Assign(3840);
                if(mode.ends_with("-minimum"))image.min_lod=512;
            }
            if(mode.ends_with("-border")||gather_integer_white){
                sampler.clamp_x.Assign(AmdGpu::ClampMode::ClampBorder);
                sampler.clamp_y.Assign(AmdGpu::ClampMode::ClampBorder);
            }
            if(mode.find("-half-border")!=std::string::npos){
                sampler.clamp_x.Assign(AmdGpu::ClampMode::ClampHalfBorder);
                sampler.clamp_y.Assign(AmdGpu::ClampMode::ClampHalfBorder);
            }
            if(mode.find("-half-mirror")!=std::string::npos){
                sampler.clamp_x.Assign(AmdGpu::ClampMode::MirrorOnceHalfBorder);
                sampler.clamp_y.Assign(AmdGpu::ClampMode::MirrorOnceHalfBorder);
            }
            if(mode.find("-mirror-border")!=std::string::npos&&mode.find("-half-")==std::string::npos){
                sampler.clamp_x.Assign(AmdGpu::ClampMode::MirrorOnceBorder);
                sampler.clamp_y.Assign(AmdGpu::ClampMode::MirrorOnceBorder);
            }
            if(mode.find("-mirror-once")!=std::string::npos){
                sampler.clamp_x.Assign(AmdGpu::ClampMode::MirrorOnceLastTexel);
                sampler.clamp_y.Assign(AmdGpu::ClampMode::MirrorOnceLastTexel);
            }
            if(gather_integer_white)sampler.border_color_type.Assign(AmdGpu::BorderColor::White);
            memcpy(regs.ps_program.user_data.data()+8,&sampler,sizeof(sampler));
            const float uv=mode.find("-half-")!=std::string::npos||mode.find("-mirror-")!=std::string::npos?(mode.ends_with("-offset")?-0.125f:-4.f):(mode.ends_with("-border")||gather_integer_white)?0.f:mode.ends_with("-center")?0.0625f:0.125f;
            const u32 component=mode.ends_with("-red")?0:mode.ends_with("-blue")?2:mode.ends_with("-alpha")?3:1;
            initialize(writer,{0xBEEB03FF,15,
                0x7e0002ff,std::bit_cast<u32>(uv),0x7e0202ff,std::bit_cast<u32>(uv),
                0xf11c0000U|(1U<<(8+component)),0x00400400,0x7e000282,0x7e020280,
                0xf0200f00,0x00000400,0xbf810000},0x4e415449564557d0ULL);
            if(mode.ends_with("-offset"))
                initialize(writer,{0xBEEB03FF,15,0x7e000281,
                    0x7e0202ff,std::bit_cast<u32>(uv),0x7e0402ff,std::bit_cast<u32>(uv),
                    0xf15c0200,0x00400400,0x7e000282,0x7e020280,
                    0xf0200f00,0x00000400,0xbf810000},0x4e415449564557d1ULL);
            if(gather_array)
                initialize(writer,{0xBEEB03FF,15,
                    0x7e0002ff,std::bit_cast<u32>(uv),0x7e0202ff,std::bit_cast<u32>(uv),
                    0x7e0402ff,std::bit_cast<u32>(mode.ends_with("-half")?0.5f:1.f),
                    0xf11c4200,0x00400400,0x7e000282,0x7e020280,0x7e040280,
                    0xf0204f00,0x00000400,0xbf810000},0x4e415449564557d2ULL);
            if(gather_compare)
                initialize(writer,{0xBEEB03FF,15,
                    0x7e0002ff,std::bit_cast<u32>(0.5f),0x7e0202ff,std::bit_cast<u32>(uv),
                    0x7e0402ff,std::bit_cast<u32>(uv),0xf13c0100,0x00400400,
                    0x7e000282,0x7e020280,0xf0200100,0x00000400,
                    0x7e000283,0xf0200100,0x00000500,
                    0x7e000284,0xf0200100,0x00000600,
                    0x7e000285,0xf0200100,0x00000700,0xbf810000},0x4e415449564557d3ULL);
            if(gather_lod){
                const float level=mode.ends_with("-low")?1.25f:mode.ends_with("-high")?1.75f:
                    mode.ends_with("-minimum")?0.f:2.f;
                initialize(writer,{0xBEEB03FF,15,
                    0x7e0002ff,std::bit_cast<u32>(0.5f),0x7e0202ff,std::bit_cast<u32>(0.5f),
                    0x7e0402ff,std::bit_cast<u32>(level),mode.ends_with("-clamp")?0xf1040200U:0xf1100200U,0x00400400,
                    0x7e000282,0x7e020280,0xf0200f00,0x00000400,0xbf810000},0x4e415449564557d4ULL);
                if(sample_readonly)
                    initialize(writer,{0xBEEB03FF,15,
                        0xbe9003ff,u32(sampler.raw0),0xbe9103ff,u32(sampler.raw0>>32),
                        0xbe9203ff,u32(sampler.raw1),0xbe9303ff,u32(sampler.raw1>>32),
                        0x7e0002ff,std::bit_cast<u32>(0.5f),0x7e0202ff,std::bit_cast<u32>(0.5f),
                        0x7e0402ff,std::bit_cast<u32>(level),mode.ends_with("-clamp")?0xf1040200U:0xf1100200U,0x00800400,
                        0x7e000282,0x7e020280,0xf0200f00,0x00020400,0xbf810000},0x4e415449564557d6ULL);
                if(gather_compare)
                    initialize(writer,{0xBEEB03FF,15,
                        0x7e0002ff,std::bit_cast<u32>(0.5f),0x7e0202ff,std::bit_cast<u32>(0.5f),
                        0x7e0402ff,std::bit_cast<u32>(0.5f),0x7e0602ff,std::bit_cast<u32>(level),
                        0xf1300100,0x00400400,
                        0x7e000282,0x7e020280,0xf0200100,0x00000400,
                        0x7e000283,0xf0200100,0x00000500,
                        0x7e000284,0xf0200100,0x00000600,
                        0x7e000285,0xf0200100,0x00000700,0xbf810000},0x4e415449564557d5ULL);
                if(mode.ends_with("-offset")){
                    initialize(writer,{0xBEEB03FF,15,0x7e000281,
                        0x7e0202ff,std::bit_cast<u32>(0.5f),0x7e0402ff,std::bit_cast<u32>(0.5f),
                        0x7e0602ff,std::bit_cast<u32>(level),0xf1500200,0x00400400,
                        0x7e000282,0x7e020280,0xf0200f00,0x00000400,0xbf810000},0x4e415449564557d7ULL);
                    if(sample_readonly)
                        initialize(writer,{0xBEEB03FF,15,
                            0xbe9003ff,u32(sampler.raw0),0xbe9103ff,u32(sampler.raw0>>32),
                            0xbe9203ff,u32(sampler.raw1),0xbe9303ff,u32(sampler.raw1>>32),0x7e000281,
                            0x7e0202ff,std::bit_cast<u32>(0.5f),0x7e0402ff,std::bit_cast<u32>(0.5f),
                            0x7e0602ff,std::bit_cast<u32>(level),0xf1500200,0x00800400,
                            0x7e000282,0x7e020280,0xf0200f00,0x00020400,0xbf810000},0x4e415449564557d8ULL);
                    if(gather_compare)
                        initialize(writer,{0xBEEB03FF,15,0x7e000281,
                            0x7e0202ff,std::bit_cast<u32>(0.5f),0x7e0402ff,std::bit_cast<u32>(0.5f),
                            0x7e0602ff,std::bit_cast<u32>(0.5f),0x7e0802ff,std::bit_cast<u32>(level),
                            0xf1700100,0x00400400,
                            0x7e000282,0x7e020280,0xf0200100,0x00000400,
                            0x7e000283,0xf0200100,0x00000500,
                            0x7e000284,0xf0200100,0x00000600,
                            0x7e000285,0xf0200100,0x00000700,0xbf810000},0x4e415449564557d9ULL);
                }
                if(gather_bias||gather_implicit){
                    regs.ps_input_addr.pos_x_float_ena=regs.ps_input_ena.pos_x_float_ena=1;
                    regs.ps_input_addr.pos_y_float_ena=regs.ps_input_ena.pos_y_float_ena=1;
                    const float bias=mode.ends_with("-down")?-2.f:-1.f;
                    initialize(writer,{0xBEEB03FF,15,0x7e040301,0x7e020300,
                        0x7e0002ff,std::bit_cast<u32>(bias),0xf1140200,0x00400400,
                        0x7e000282,0x7e020280,0xf0200f00,0x00000400,0xbf810000},0x4e415449564557daULL);
                    if(sample_readonly)
                        initialize(writer,{0xBEEB03FF,15,
                            0xbe9003ff,u32(sampler.raw0),0xbe9103ff,u32(sampler.raw0>>32),
                            0xbe9203ff,u32(sampler.raw1),0xbe9303ff,u32(sampler.raw1>>32),
                            0x7e040301,0x7e020300,0x7e0002ff,std::bit_cast<u32>(bias),
                            0xf1140200,0x00800400,0x7e000282,0x7e020280,
                            0xf0200f00,0x00020400,0xbf810000},0x4e415449564557dbULL);
                    if(gather_implicit){
                        initialize(writer,{0xBEEB03FF,15,0xf1000200,0x00400400,
                            0x7e000282,0x7e020280,0xf0200f00,0x00000400,0xbf810000},0x4e415449564557dcULL);
                        if(sample_readonly)
                            initialize(writer,{0xBEEB03FF,15,
                                0xbe9003ff,u32(sampler.raw0),0xbe9103ff,u32(sampler.raw0>>32),
                                0xbe9203ff,u32(sampler.raw1),0xbe9303ff,u32(sampler.raw1>>32),
                                0xf1000200,0x00800400,0x7e000282,0x7e020280,
                                0xf0200f00,0x00020400,0xbf810000},0x4e415449564557ddULL);
                    }
                }
            }
        }
        if(sample_border||implicit_clamp||sample_depth||sample_gather)
            initialize(reader,{0xBEEB03FF,15,0x7e000280,0x7e020280,0x7e040280,
                0x7e0602ff,0x3f800000,0xf80008cf,0x03020100,0xbf810000},0x4e41544956455478ULL);
        if(mode.ends_with("-crossstage")){
            memcpy(regs.vs_program.user_data.data()+8,&sampler,sizeof(sampler));
            initialize(reader,{0xBEEB03FF,15,0x7e0002ff,std::bit_cast<u32>(0.5f),0x7e020280,0x7e040280,
                0xf0bc0100,0x00400400,0x7e060304,0x7e000280,0x7e020280,0x7e040280,
                0xf80008cf,0x03020100,0xbf810000},0x4e415449564556c2ULL);
        }
    }
    array_reader=argc==4&&std::string_view(argv[3])=="mixed-slice-read";
    mixed_slice=array_reader||atomic_swap_array||(argc==4&&std::string_view(argv[3])=="mixed-slice");
    dimension_query=argc==4&&std::string_view(argv[3]).starts_with("query-slice");
    const bool one_dimensional=argc==4&&(std::string_view(argv[3])=="slice-1d"||std::string_view(argv[3])=="query-slice-1d");
    const bool slice=mode=="atomic-slice"||dimension_query||one_dimensional||mixed_slice||(argc==4&&std::string_view(argv[3])=="slice");
    if(mixed_slice&&!array_reader){
        initialize(writer,{0xBEEB03FF,15,0x7e000280,0x7e020280,0x7e040281,0x7e060280,
            0x7e080280,0x7e0a0280,0x7e0c0280,0x7e0e0280,
            0xf0204f00,0x00000400,0xbf810000},0x4e41544956455434ULL);
    }
    if(array_reader){
        initialize(reader,{0xBEEB03FF,15,0x7e000280,0x7e020280,0x7e040281,0x7e060280,
            0xf0004f00,0x00000400,0xf80008cf,0x07060504,0xbf810000},0x4e41544956455435ULL);
    }
    independent_query=argc==4&&std::string_view(argv[3])=="query-slice-independent";
    if(dimension_query){
        regs.ps_program.settings.num_user_regs=independent_query?16:8;
        initialize(writer,{0xBEEB03FF,15,0x7e000280,0x7e020280,0x7e040280,0x7e060280,
            0xf0380f00,independent_query?0x00020400U:0x00000400U,0xf0200f00,0x00000400,0xbf810000},independent_query?0x4e41544956455437ULL:0x4e41544956455436ULL);
    }
    if(atomic_image){
        initialize(writer,{0xBEEB03FF,15,0x7e000280,0x7e020280,0x7e040280,0x7e060280,
            0x7e080281,0xf0440100,0x00000400,0xbf810000},0x4e41544956455438ULL);
    }
    if(atomic_swap_array){
        // Array coordinates (0,0,1); exchange 1 into physical layer 1.
        initialize(writer,{0xBEEB03FF,15,0x7e000280,0x7e020280,0x7e040281,0x7e060280,
            0x7e080281,0xf03c4100,0x00000400,0xbf810000},0x4e41544956455439ULL);
    }
    if(atomic_return){
        // GLC returns the old value in v4; store it at x=1 in the same layer.
        initialize(writer,{0xBEEB03FF,15,0x7e000280,0x7e020280,0x7e040281,0x7e060280,
            0x7e080281,0xf03c6100,0x00000400,0x7e000281,
            0xf0204100,0x00000400,0xbf810000},0x4e4154495645543aULL);
    }
    if(bounded_atomic){
        // Operate on x=2, keeping the vertex-position texel at x=0 intact.
        // Store the GLC return at x=1 to independently verify the previous value.
        initialize(writer,{0xBEEB03FF,15,0x7e000282,0x7e020280,0x7e040281,0x7e060280,
            0x7e0802ff,atomic_limit,decrement?0xf0706100U:0xf06c6100U,0x00000400,
            0x7e000281,0xf0204100,0x00000400,0xbf810000},
            0x4e41544956455440ULL+u64(decrement)*4+u64(atomic_limit==UINT32_MAX));
    }
    if(atomic_contention){
        initialize(writer,{0xBEEB03FF,15,0x7e000282,0x7e020280,0x7e040281,0x7e060280,
            0x7e0802ff,atomic_limit,0xf06c4100,0x00000400,0xbf810000},
            0x4e41544956455448ULL);
    }
    if(float_atomic){
        initialize(writer,{0xBEEB03FF,15,0x7e000282,0x7e020280,0x7e040281,0x7e060280,
            0x7e0802ff,std::bit_cast<u32>(float_operand),float_max?0xf07c6100U:0xf0786100U,
            0x00000400,0x7e000281,0xf0204100,0x00000400,0xbf810000},
            0x4e41544956455450ULL+u64(float_max)*2+u64(float_negative)+u64(float_update)*4);
    }
    if(float_compare){
        initialize(writer,{0xBEEB03FF,15,0x7e000282,0x7e020280,0x7e040281,0x7e060280,
            0x7e0802ff,0x40400000U,0x7e0a02ff,compare_operand,0xf0746100,0x00000400,
            0x7e000281,0xf0204100,0x00000400,0xbf810000},
            0x4e41544956455460ULL+compare_operand);
    }
    buffer_resources=buffer_atomic;
    const bool signed64=mode=="atomic-smin64"||mode=="atomic-smax64";
    const bool maximum64=mode=="atomic-umax64"||mode=="atomic-smax64";
    const bool add64=mode=="atomic-add64"||mode=="atomic-add-wrap64";
    const bool sub64=mode=="atomic-sub64"||mode=="atomic-sub-wrap64";
    const bool cmp64=mode.starts_with("atomic-cmp-");
    const bool counter64=atomic64&&(mode.starts_with("atomic-inc")||mode.starts_with("atomic-dec"));
    const bool counter_dec64=counter64&&mode.starts_with("atomic-dec");
    const u64 initial64=counter64?(mode=="atomic-inc-max64"?UINT64_MAX:mode=="atomic-inc-wrap64"?0x200000000ULL:
        mode=="atomic-dec-zero64"?0ULL:mode=="atomic-dec-above64"?0x300000000ULL:
        counter_dec64?0x100000000ULL:0xffffffffULL):add64?(mode=="atomic-add-wrap64"?UINT64_MAX:0xffffffffULL):
        sub64?(mode=="atomic-sub-wrap64"?0ULL:0x100000000ULL):
        signed64?0x8000000187654321ULL:0x1234567887654321ULL;
    const u64 operand64=counter64?(mode=="atomic-inc-max64"?UINT64_MAX:0x200000000ULL):
        add64||sub64?1ULL:0x0000000122222222ULL;
    const u64 compare64=initial64^(mode=="atomic-cmp-high64"?0x100000000ULL:mode=="atomic-cmp-low64"?1ULL:0ULL);
    const u64 result64=counter64&&atomic_contention?initial64+4096:counter64?(counter_dec64?(initial64==0||initial64>operand64?operand64:initial64-1):
        (initial64>=operand64?0:initial64+1)):mode=="atomic-and64"?(initial64&operand64):mode=="atomic-or64"?(initial64|operand64):
        mode=="atomic-xor64"?(initial64^operand64):mode=="atomic-swap64"?operand64:
        cmp64?(compare64==initial64?operand64:initial64):add64?initial64+operand64:sub64?initial64-operand64:
        (signed64?maximum64:!maximum64)?operand64:initial64;
    if(subtract_atomic){
        initialize(writer,{0xBEEB03FF,15,0x7e000282,0x7e020280,0x7e040281,0x7e060280,
            0x7e080283,0xf0486100,0x00000400,0x7e000281,
            0xf0204100,0x00000400,0xbf810000},0x4e415449564556f0ULL);
    }
    if(buffer_atomic){
        const u32 operand=subtract_atomic?3U:bounded_atomic?atomic_limit:float_compare?0x40400000U:std::bit_cast<u32>(float_operand);
        const u32 opcode=subtract_atomic?51U:bounded_atomic?(decrement?61U:60U):float_compare?62U:float_max?64U:63U;
        // MUBUF target at byte 8, previous value stored at byte 4. SGPR offset is inline zero.
        initialize(writer,{0xBEEB03FF,15,0x7e0802ff,operand,0x7e0a02ff,compare_operand,
            0xe0004008U|(opcode<<18),0x80000400,0xe0700004,0x80000400,0xbf810000},
            0x4e41544956455500ULL+u64(opcode)*0x100000000ULL+operand+compare_operand);
        if(dynamic_operand){
            initialize(writer,{0xBEEB03FF,15,0xe0300000,0x80000400,0xe030000c,0x80000500,
                0xe0004008U|(opcode<<18),0x80000400,0xe0700004,0x80000400,0xbf810000},
                0x4e41544956455600ULL+opcode);
        }
        if(atomic_contention){
            initialize(writer,{0xBEEB03FF,15,0x7e0802ff,operand,
                0xe0000008U|(opcode<<18),0x80000400,0xbf810000},0x4e41544956455680ULL+opcode);
        }
        if(atomic64){
            const u32 op64=counter64?(counter_dec64?93U:92U):mode=="atomic-and64"?89U:mode=="atomic-or64"?90U:mode=="atomic-xor64"?91U:
                mode=="atomic-swap64"?80U:cmp64?81U:add64?82U:sub64?83U:maximum64?(signed64?87U:88U):(signed64?85U:86U);
            initialize(writer,{0xBEEB03FF,15,0x7e0802ff,u32(operand64),0x7e0a02ff,u32(operand64>>32),
                0x7e0c02ff,u32(compare64),0x7e0e02ff,u32(compare64>>32),
                0xe0004008U|(op64<<18),0x80000400,0xe0740000,0x80000400,0xbf810000},
                0x4e41544956455700ULL+op64+compare64);
            if(atomic_contention){
                initialize(writer,{0xBEEB03FF,15,0x7e0802ff,u32(operand64),0x7e0a02ff,u32(operand64>>32),
                    0xe0000008U|(op64<<18),0x80000400,0xbf810000},0x4e41544956455800ULL+op64);
            }
        }
    }
    if(slice){image.type=u64(one_dimensional?AmdGpu::ImageType::Color1DArray:AmdGpu::ImageType::Color2DArray);image.depth=2;image.base_array=image.last_array=1;}
    const auto address=reinterpret_cast<u64>(memory);
    bind(address>>8,address>>8);regs.primitive_type=AmdGpu::PrimitiveType::PointList;
    auto gpu_stages=frontend.BuildGraphics(regs);
    if(sample_gather&&sample_readonly){
        const auto* original=gpu_stages.stages[u32(Shader::SwStage::Fragment)];
        if(!std::ranges::any_of(original->info.images,[](const auto& resource){return !resource.is_written;})||
           !std::ranges::any_of(original->info.samplers,[](const auto& resource){return resource.native_manual_sampling;}))
            throw std::runtime_error("Explicit Gather did not retain read-only manual sampling");
        const auto minimum=image.min_lod;
        image.min_lod=768;bind(address>>8,address>>8);
        if(frontend.BuildGraphics(regs).stages[u32(Shader::SwStage::Fragment)]==original)
            throw std::runtime_error("Read-only Gather reused stale resource MinLOD specialization");
        image.min_lod=minimum;bind(address>>8,address>>8);gpu_stages=frontend.BuildGraphics(regs);
        if(gpu_stages.stages[u32(Shader::SwStage::Fragment)]!=original)
            throw std::runtime_error("Restored read-only Gather missed shader cache");
    }
    if(mode.ends_with("-crossstage")&&!gpu_stages.stages[u32(Shader::SwStage::Vertex)]->info.images[0].is_written)
        throw std::runtime_error("Comparison VS did not join the PS writable image alias");
    if(resource_lod&&!sample_readonly&&!mode.ends_with("-limit")){
        const auto* original=gpu_stages.stages[u32(Shader::SwStage::Vertex)];
        const auto minimum=image.min_lod;
        image.min_lod=768;bind(address>>8,address>>8);
        if(frontend.BuildGraphics(regs).stages[u32(Shader::SwStage::Vertex)]==original)
            throw std::runtime_error("Resource minimum LOD reused a stale shader");
        image.min_lod=minimum;bind(address>>8,address>>8);gpu_stages=frontend.BuildGraphics(regs);
        if(gpu_stages.stages[u32(Shader::SwStage::Vertex)]!=original)
            throw std::runtime_error("Restored resource LOD missed shader cache");
    }
    if(sample_gather&&!sample_readonly){
        const auto* sampled=gpu_stages.stages[u32(Shader::SwStage::Fragment)];
        if(sampled->info.images.empty()||!std::ranges::all_of(sampled->info.images,
            [](const auto& resource){return resource.is_written;}))
            throw std::runtime_error("Gather did not join the writable image alias");
    }
    if(aniso_case&&sample_readonly){
        const auto* sampled=gpu_stages.stages[u32(Shader::SwStage::Vertex)];
        if(!sampled->info.samplers[0].native_manual_sampling)
            throw std::runtime_error("Read-only anisotropic point/mip filter was sent to the fixed linear sampler");
        AmdGpu::Sampler original{};memcpy(&original,regs.vs_program.user_data.data()+8,sizeof(original));
        auto changed=original;
        changed.xy_min_filter.Assign(AmdGpu::Filter::AnisoLinear);
        changed.xy_mag_filter.Assign(AmdGpu::Filter::AnisoLinear);
        changed.mip_filter.Assign(AmdGpu::MipFilter::Linear);
        memcpy(regs.vs_program.user_data.data()+8,&changed,sizeof(changed));
        const auto* hardware=frontend.BuildGraphics(regs).stages[u32(Shader::SwStage::Vertex)];
        if(hardware==sampled||hardware->info.samplers[0].native_manual_sampling)
            throw std::runtime_error("Linear read-only anisotropy did not retain the native sampler path");
        changed.xy_min_filter.Assign(AmdGpu::Filter::AnisoPoint);
        changed.xy_mag_filter.Assign(AmdGpu::Filter::AnisoPoint);
        memcpy(regs.vs_program.user_data.data()+8,&changed,sizeof(changed));
        const auto* point=frontend.BuildGraphics(regs).stages[u32(Shader::SwStage::Vertex)];
        if(point==hardware||!point->info.samplers[0].native_manual_sampling)
            throw std::runtime_error("Hardware-to-manual anisotropy reused stale sampler specialization");
        memcpy(regs.vs_program.user_data.data()+8,&original,sizeof(original));
        gpu_stages=frontend.BuildGraphics(regs);
        if(gpu_stages.stages[u32(Shader::SwStage::Vertex)]!=sampled)
            throw std::runtime_error("Restored read-only anisotropic sampler missed frontend cache");
    }
    if(mode.starts_with("sample-feedback")&&!sample_readonly&&!sample_border&&!implicit_clamp&&!sample_depth&&!sample_gather){
        const auto* sampled=gpu_stages.stages[u32(Shader::SwStage::Vertex)];
        if(!sampled->info.images[0].is_written)
            throw std::runtime_error("Sampler alias was not unified as storage");
        AmdGpu::Sampler original{};
        memcpy(&original,regs.vs_program.user_data.data()+8,sizeof(original));
        auto changed=original;
        const auto filter=original.xy_min_filter==AmdGpu::Filter::Point?AmdGpu::Filter::Bilinear:AmdGpu::Filter::Point;
        changed.xy_min_filter.Assign(filter);changed.xy_mag_filter.Assign(filter);
        memcpy(regs.vs_program.user_data.data()+8,&changed,sizeof(changed));
        if(frontend.BuildGraphics(regs).stages[u32(Shader::SwStage::Vertex)]==sampled)
            throw std::runtime_error("Manual sampling reused stale filter specialization");
        memcpy(regs.vs_program.user_data.data()+8,&original,sizeof(original));
        gpu_stages=frontend.BuildGraphics(regs);
        if(gpu_stages.stages[u32(Shader::SwStage::Vertex)]!=sampled)
            throw std::runtime_error("Restored manual sampler missed frontend cache");
        if(aniso_case){
            changed=original;
            changed.max_aniso.Assign(original.max_aniso==AmdGpu::AnisoRatio::Two?
                AmdGpu::AnisoRatio::Four:AmdGpu::AnisoRatio::Two);
            memcpy(regs.vs_program.user_data.data()+8,&changed,sizeof(changed));
            if(frontend.BuildGraphics(regs).stages[u32(Shader::SwStage::Vertex)]==sampled)
                throw std::runtime_error("Anisotropic tap limit reused stale shader specialization");
            memcpy(regs.vs_program.user_data.data()+8,&original,sizeof(original));
            gpu_stages=frontend.BuildGraphics(regs);
            if(gpu_stages.stages[u32(Shader::SwStage::Vertex)]!=sampled)
                throw std::runtime_error("Restored anisotropic limit missed frontend cache");
        }
    }
    if(slice&&!mixed_slice){
        const auto* layered=gpu_stages.stages[u32(Shader::SwStage::Vertex)];
        image.depth=0;image.base_array=image.last_array=0;bind(address>>8,address>>8);
        const auto one_layer=frontend.BuildGraphics(regs);
        if(one_layer.stages[u32(Shader::SwStage::Vertex)]==layered)
            throw std::runtime_error("Layered DXIL resource dimension reused a non-array variant");
        image.depth=2;image.base_array=image.last_array=1;bind(address>>8,address>>8);
        gpu_stages=frontend.BuildGraphics(regs);
        if(gpu_stages.stages[u32(Shader::SwStage::Vertex)]!=layered)
            throw std::runtime_error("Restored layered view missed frontend cache");
    }
    const auto& gpu_dxil=cache.Graphics(gpu_stages);
    const auto desc=DescribeGuestTexture(image);const auto layout=LayoutGuestTexture(desc);
    if(layout.guest_bytes>(sample_readonly?sample_destination_offset:allocation_bytes)||
        (sample_readonly&&layout.guest_bytes>allocation_bytes-sample_destination_offset))
        throw std::runtime_error("GCN image fixture exceeds its guest allocation");
    std::fill_n(memory,layout.guest_bytes,std::byte{0x5a});
    const float position[]{0.f,0.f,0.f,1.f};
    const auto offset=GuestPixelOffset(desc,layout,0,slice?1:0,0,0);
    const auto texel_bytes=atomic_image||sample_depth?sizeof(u32):sizeof(position);
    memcpy(memory+offset,position,texel_bytes);
    if(mode.starts_with("sample-feedback-linear")&&!mode.ends_with("-low")&&!mode.ends_with("-high")&&mode!="sample-feedback-linear-mirror-seam"){
        // Each individual tap is outside the clip volume. Only their bilinear
        // average produces the center point, which triggers the PS writeback.
        const float taps[4][4]{{-8,-8,0,1},{8,-8,0,1},{-8,8,0,1},{8,8,0,1}};
        for(u32 tap=0;tap<4;++tap){
            const bool seam=mode=="sample-feedback-linear-wrap-seam";
            memcpy(memory+GuestPixelOffset(desc,layout,0,0,seam?(tap%2?0:7):tap%2,
                seam?(tap/2?0:7):tap/2),taps[tap],sizeof(position));
        }
    }
    if(mode.starts_with("sample-feedback")&&!sample_depth&&(mode.ends_with("-high")||mode=="sample-feedback-linear-mirror-seam"||
        (mode.find("-mirror-once")!=std::string::npos&&mode.ends_with("-low"))))
        memcpy(memory+GuestPixelOffset(desc,layout,0,0,7,7),position,sizeof(position));
    if(sample_border&&(border_mirror&&!border_inside||mode.find("-high-")!=std::string::npos))
        memcpy(memory+GuestPixelOffset(desc,layout,0,0,7,7),position,sizeof(position));
    if(sample_lod){
        const u32 selected=shader_clamp?(mode.ends_with("-low")?0:2):sample_implicit?implicit_level:
            sample_grad?(mode.ends_with("-zero")?0:mode.ends_with("-bias")?3:2):
            resource_lod?(mode.ends_with("-limit")?3:mode.ends_with("-below-base")?1:2):
            mode.ends_with("-round-low")?1:mode.ends_with("-round-high")?2:
            mode.ends_with("-low")||mode.ends_with("-none")?0:
            mode.ends_with("-high")?3:mode.ends_with("-min")||mode.ends_with("-round-high")?2:1;
        for(u32 mip=0;mip<4;++mip){
            const float bad[]{16.f,16.f,0.f,1.f};
            memcpy(memory+GuestPixelOffset(desc,layout,mip,0,0,0),bad,sizeof(bad));
        }
        if(sample_lod_linear){
            const float lower[]{-8.f,-8.f,0.f,1.f},upper[]{8.f,8.f,0.f,1.f};
            memcpy(memory+GuestPixelOffset(desc,layout,1,0,0,0),lower,sizeof(lower));
            memcpy(memory+GuestPixelOffset(desc,layout,2,0,0,0),upper,sizeof(upper));
        }else memcpy(memory+GuestPixelOffset(desc,layout,selected,0,(shader_clamp||implicit_clamp)&&mode.ends_with("-offset")?1:0,0),position,sizeof(position));
        if(sample_bias)
            for(u32 y=0;y<std::max(1U,8U>>selected);++y)for(u32 x=0;x<std::max(1U,8U>>selected);++x)
                memcpy(memory+GuestPixelOffset(desc,layout,selected,0,x,y),position,sizeof(position));
        if(sample_lod_dynamic){const float dynamic_lod=1.5f;memcpy(memory+offset,&dynamic_lod,sizeof(dynamic_lod));}
    }
    if(sample_filter){
        const u32 mip=filter_min?1:0;
        const float taps[4][4]{{-8,-8,0,1},{8,-8,0,1},{-8,8,0,1},{8,8,0,1}};
        for(u32 tap=0;tap<4;++tap)
            memcpy(memory+GuestPixelOffset(desc,layout,mip,0,tap%2,tap/2),
                !filter_linear&&tap==3?position:taps[tap],sizeof(position));
    }
    if(sample_reduce){
        const float edge=reduce_max?-8.f:8.f;
        const float taps[4][4]{{0,edge,0,1},{edge,0,0,1},{edge,edge,0,1},{edge,edge,0,1}};
        const float poison[]{-edge,-edge,0,1};
        for(u32 tap=0;tap<(reduce_mip?2U:4U);++tap){
            const auto target=GuestPixelOffset(desc,layout,reduce_mip?1+tap:0,0,reduce_mip?0:tap%2,reduce_mip?0:tap/2);
            memcpy(memory+target,reduce_zero?(tap==0?position:poison):taps[tap],sizeof(position));
        }
    }
    if(sample_depth){
        for(u32 mip=0;mip<4;++mip){
            const u32 size=std::max(1U,8U>>mip);
            for(u32 layer=0;layer<(depth_array?3U:1U);++layer)for(u32 y=0;y<(depth_1d?1U:size);++y)for(u32 x=0;x<size;++x){
                const float depth=(!depth_array||layer==2)&&(mip==2||(mip==0&&mode.ends_with("-crossstage")))&&!mode.ends_with("-false")&&!mode.ends_with("-after-write")?
                    (depth_1d&&mode.ends_with("-offset")?(x==1?0.75f:0.25f):mode.ends_with("-linear")?(x==0&&y==0?0.75f:0.25f):
                     mode.ends_with("-equal")?0.5f:0.75f):0.25f;
                memcpy(memory+GuestPixelOffset(desc,layout,mip,layer,x,y),&depth,sizeof(depth));
            }
        }
    }
    if(sample_volume){
        for(u32 mip=0;mip<4;++mip){
            const u32 size=std::max(1U,8U>>mip);
            for(u32 z=0;z<size;++z)for(u32 y=0;y<size;++y)for(u32 x=0;x<size;++x){
                const float value=float((mip==2?1:100)+x+2*y+4*z);
                memcpy(memory+GuestPixelOffset(desc,layout,mip,z,x,y),&value,sizeof(value));
            }
        }
    }
    if(sample_gather){
        for(u32 mip=0;mip<(gather_lod?4U:1U);++mip){
        for(u32 layer=0;layer<(gather_array?3U:1U);++layer)for(u32 y=0;y<(8U>>mip);++y)for(u32 x=0;x<(8U>>mip);++x){
            const float base=float(10+x+10*y+100*layer+1000*mip);
            const float values[]{gather_compare?(x==y?0.75f:0.25f):base,base+1,base+2,base+3};
            if(gather_integer){
                u32 raw[4];for(u32 c=0;c<4;++c)raw[c]=gather_word(values[c],c);
                memcpy(memory+GuestPixelOffset(desc,layout,mip,layer,x,y),raw,sizeof(raw));
            }else memcpy(memory+GuestPixelOffset(desc,layout,mip,layer,x,y),values,gather_compare?sizeof(float):sizeof(values));
        }
        }
    }
    std::array<float,4> anisotropic_reference{};
    if(aniso_case){
        for(u32 mip=0;mip<4;++mip){
            const u32 size=8U>>mip;
            for(u32 layer=0;layer<(aniso_cube?image.NumLayers():1U);++layer)
            for(u32 y=0;y<size;++y)for(u32 x=0;x<size;++x){
                const float value[]{float(int((x*3+y*5+mip*7+layer*2)%9)-4)/16.f,
                    float(int((x+y*2+mip+layer)%5)-2)/16.f,0.f,1.f};
                memcpy(memory+GuestPixelOffset(desc,layout,mip,layer,x,y),value,sizeof(value));
            }
        }
        const double rho_x=8.0*std::hypot(aniso_dx[0],aniso_dx[1]);
        const double rho_y=8.0*std::hypot(aniso_dy[0],aniso_dy[1]);
        const double major=std::max(rho_x,rho_y),minor=std::min(rho_x,rho_y);
        const double eta=!sample_grad||major==0?1.0:
            minor==0?double(aniso_limit):std::clamp(major/minor,1.0,double(aniso_limit));
        const u32 count=u32(std::ceil(eta));
        double level=sample_grad?(major==0?-100.0:std::log2(major/eta)):sample_lz?0.0:
            mode.ends_with("-cancel-high")?-16.0:mode.ends_with("-cancel-low")?20.0:1.0;
        if(mode.ends_with("-bias"))level+=1.0;
        if(mode.ends_with("-cancel-high"))level+=15.99;
        if(mode.ends_with("-cancel-low"))level-=16.0;
        level=std::clamp(level,mode.ends_with("-min")&&mode.find("-reduce-")==std::string::npos?2.0:0.0,
            mode.ends_with("-max")&&mode.find("-reduce-")==std::string::npos?1.0:15.0);
        if(resource_lod)level=std::max(level,2.0);
        level=std::clamp(level,0.0,3.0);
        const bool minimum=mode.ends_with("-reduce-min"),maximum=mode.ends_with("-reduce-max");
        const auto merge=[&](double a,double b,double weight){
            if(weight==0.0)return a;
            if(weight==1.0)return b;
            return minimum?std::min(a,b):maximum?std::max(a,b):a+(b-a)*weight;
        };
        const auto address_texel=[&](int coordinate,int size){
            if(mode.ends_with("-wrap"))return (coordinate%size+size)%size;
            if(mode.ends_with("-mirror")){
                const int folded=(coordinate%(2*size)+2*size)%(2*size);
                return folded<size?folded:2*size-1-folded;
            }
            return std::clamp(coordinate,0,size-1);
        };
        const auto sample_mip=[&](u32 mip,double u,double v){
            const int size=8>>mip;
            const double px=u*size-(aniso_linear?0.5:0.0),py=v*size-(aniso_linear?0.5:0.0);
            const int bx=int(std::floor(px)),by=int(std::floor(py));
            const auto texel=[&](int x,int y){
                std::array<float,4> value{};
                memcpy(value.data(),memory+GuestPixelOffset(desc,layout,mip,aniso_sample_layer,
                    u32(address_texel(x,size)),u32(address_texel(y,size))),sizeof(value));
                return value;
            };
            const auto a=texel(bx,by),b=texel(bx+1,by),c=texel(bx,by+1),d=texel(bx+1,by+1);
            std::array<double,4> result{};
            for(u32 component=0;component<4;++component)
                result[component]=aniso_linear?merge(merge(a[component],b[component],px-bx),
                    merge(c[component],d[component],px-bx),py-by):double(a[component]);
            return result;
        };
        const auto& direction=rho_x>rho_y?aniso_dx:aniso_dy;
        std::array<double,4> accumulated{};
        for(u32 tap=0;tap<count;++tap){
            const double shift=sample_grad?-0.5+double(tap+1)/double(count+1):0.0;
            const double u=aniso_uv+direction[0]*shift,v=aniso_uv+direction[1]*shift;
            const u32 mip=u32(std::floor(aniso_mip_linear?level:level+0.5));
            auto value=sample_mip(mip,u,v);
            if(aniso_mip_linear){
                const auto next=sample_mip(std::min(mip+1,3U),u,v);
                for(u32 component=0;component<4;++component)
                    value[component]=merge(value[component],next[component],level-std::floor(level));
            }
            for(u32 component=0;component<4;++component)
                accumulated[component]=tap?merge(accumulated[component],value[component],1.0/double(tap+1)):value[component];
        }
        for(u32 component=0;component<4;++component)anisotropic_reference[component]=float(accumulated[component]);
    }
    std::vector<std::byte> expected(memory,memory+layout.guest_bytes);
    std::fill_n(expected.data()+offset,texel_bytes,std::byte{});
    if(mode.starts_with("sample-feedback")){
        expected.assign(memory,memory+layout.guest_bytes);
        if(!sample_readonly)memcpy(expected.data()+GuestPixelOffset(desc,layout,desc.first_mip,0,2,0),position,sizeof(position));
    }
    if(aniso_case&&!sample_readonly)memcpy(expected.data()+GuestPixelOffset(desc,layout,0,0,2,0),
        anisotropic_reference.data(),sizeof(anisotropic_reference));
    if(sample_border){
        const float weight=border_weight;
        const float rgb=border_white?weight:0.f;
        const float alpha=border_white||border_opaque?1.f:1.f-weight;
        float border_result[]{rgb,rgb,rgb,alpha};
        if(border_custom)for(unsigned c=0;c<4;++c)
            border_result[c]=border_table[7][c]*weight+(c==3?1.f-weight:0.f);
        memcpy(expected.data()+GuestPixelOffset(desc,layout,0,0,2,0),border_result,sizeof(border_result));
    }
    if(depth_shared){
        expected.assign(memory,memory+layout.guest_bytes);
        const float result=mode.ends_with("-linear")?(depth_1d?0.5f:0.25f):depth_array&&(mode.ends_with("-low")||mode.ends_with("-half"))?0.f:1.f;
        memcpy(expected.data()+GuestPixelOffset(desc,layout,0,depth_array?1:0,2,0),&result,sizeof(result));
        if(mode.ends_with("-after-write")){
            const float written_depth=0.75f;
            memcpy(expected.data()+GuestPixelOffset(desc,layout,2,0,0,0),&written_depth,sizeof(written_depth));
        }
    }
    if(sample_volume){
        const float value=mode.ends_with("-mip")?57.5f:mode.ends_with("-border")?1.25f:
            mode.ends_with("-xy")||mode.ends_with("-mirror")?6.5f:
            mode.ends_with("-linear-z")?6.f:
            mode.find("-linear")!=std::string::npos?4.5f:mode.ends_with("-point-z")?5.f:1.f;
        memcpy(expected.data()+GuestPixelOffset(desc,layout,0,0,2,0),&value,sizeof(value));
    }
    if(sample_gather){
        expected.assign(memory,memory+layout.guest_bytes);
        const bool border=mode.ends_with("-border")||gather_integer_white;
        const float component=mode.ends_with("-red")?0.f:mode.ends_with("-blue")?2.f:mode.ends_with("-alpha")?3.f:1.f;
        const float shift=gather_array?(mode.ends_with("-half")?100.f:200.f):mode.ends_with("-offset")?1.f:0.f;
        const float values[]{border?0.f:20.f+component+shift,border?11.f:21.f+component+shift,
            border?0.f:11.f+component+shift,border?0.f:10.f+component+shift};
        if(!gather_compare)
            memcpy(expected.data()+GuestPixelOffset(desc,layout,0,gather_array?1:0,2,0),values,sizeof(values));
        if(gather_compare){
            const bool shifted=gather_lod&&mode.ends_with("-offset");
            const float results[]{shifted?1.f:0.f,1.f,0.f,border||shifted?0.f:1.f};
            for(u32 c=0;c<4;++c)
                memcpy(expected.data()+GuestPixelOffset(desc,layout,0,0,2+c,0),&results[c],sizeof(float));
        }
        if(gather_lod&&!gather_compare){
            const bool low=mode.ends_with("-low");
            const float base=low?1022.f:2011.f;
            const bool shifted=mode.ends_with("-offset");
            const float gathered[]{base+(shifted?11:10),base+11,base+1,base+(shifted?1:0)};
            memcpy(expected.data()+GuestPixelOffset(desc,layout,0,0,2,0),gathered,sizeof(gathered));
            if(gather_bias||gather_implicit){
                const float value=gather_implicit?3011.f:mode.ends_with("-down")?1044.f:2022.f;
                const float biased[]{value,value,value,value};
                memcpy(expected.data()+GuestPixelOffset(desc,layout,0,0,2,0),biased,sizeof(biased));
            }
        }
        if(mode.find("-half-")!=std::string::npos||mode.find("-mirror-")!=std::string::npos){
            const float mirrored[]{21.f,21.f,11.f,11.f};
            const float half[]{0.f,11.f,0.f,0.f};
            const float mirror_border[]{0.f,21.f,11.f,0.f};
            const float half_mirror[]{0.f,0.f,0.f,88.f};
            const float* taps=mode.find("-mirror-once")!=std::string::npos?mirrored:
                mode.find("-mirror-border")!=std::string::npos&&mode.find("-half-")==std::string::npos?mirror_border:
                mode.find("-half-mirror")!=std::string::npos?half_mirror:half;
            memcpy(expected.data()+GuestPixelOffset(desc,layout,0,0,2,0),taps,4*sizeof(float));
        }
        if(gather_integer){
            const auto at=GuestPixelOffset(desc,layout,0,gather_array?1:0,2,0);
            float taps[4];memcpy(taps,expected.data()+at,sizeof(taps));
            u32 raw[4];for(u32 c=0;c<4;++c)
                raw[c]=border&&c!=1?(gather_integer_white?1U:0U):gather_word(taps[c],u32(component));
            memcpy(expected.data()+at,raw,sizeof(raw));
        }
    }
    std::vector<std::byte> sampled_destination;
    if(sample_readonly){
        if(layout.guest_bytes>sample_destination_offset)throw std::runtime_error("Sample source overlaps separate output");
        std::fill_n(memory+sample_destination_offset,layout.guest_bytes,std::byte{0x5a});
        sampled_destination.assign(memory+sample_destination_offset,memory+sample_destination_offset+layout.guest_bytes);
        const float compare_result=mode.ends_with("-linear")?(depth_1d?0.5f:0.25f):
            mode.ends_with("-false")||mode.ends_with("-never")||mode.ends_with("-greater")||mode.ends_with("-not-equal")||
            (depth_array&&(mode.ends_with("-half")||mode.ends_with("-low")))?0.f:1.f;
        memcpy(sampled_destination.data()+GuestPixelOffset(desc,layout,desc.first_mip,depth_array?1:0,2,0),
            sample_depth?&compare_result:position,sample_depth?sizeof(float):sizeof(position));
        if(aniso_case)memcpy(sampled_destination.data()+GuestPixelOffset(desc,layout,0,0,2,0),
            anisotropic_reference.data(),sizeof(anisotropic_reference));
        if(sample_border){
            const auto output=GuestPixelOffset(desc,layout,0,0,2,0);
            memcpy(sampled_destination.data()+output,expected.data()+output,4*sizeof(float));
            expected.assign(memory,memory+layout.guest_bytes);
        }
        if(sample_gather){
            const auto output=GuestPixelOffset(desc,layout,0,0,2,0);
            memcpy(sampled_destination.data()+output,expected.data()+output,4*sizeof(float));
            expected.assign(memory,memory+layout.guest_bytes);
        }
    }
    if(atomic_image){const u32 incremented=1;memcpy(expected.data()+offset,&incremented,sizeof(incremented));}
    if(atomic_return){const u32 previous=0;
        const auto result_offset=GuestPixelOffset(desc,layout,0,1,1,0);
        memcpy(expected.data()+result_offset,&previous,sizeof(previous));}
    if(bounded_atomic){
        memcpy(expected.data()+offset,position,sizeof(u32));
        const auto target=GuestPixelOffset(desc,layout,0,1,2,0);
        memcpy(memory+target,&initial_atomic,sizeof(initial_atomic));
        memcpy(expected.data()+target,&bounded_result,sizeof(bounded_result));
        const auto result=GuestPixelOffset(desc,layout,0,1,1,0);
        if(atomic_contention)memcpy(expected.data()+result,memory+result,sizeof(u32));
        else memcpy(expected.data()+result,&initial_atomic,sizeof(initial_atomic));
    }
    if(float_atomic){
        memcpy(expected.data()+offset,position,sizeof(u32));
        const auto target=GuestPixelOffset(desc,layout,0,1,2,0);
        memcpy(memory+target,&float_initial,sizeof(float));
        memcpy(expected.data()+target,&float_result,sizeof(float));
        const auto result=GuestPixelOffset(desc,layout,0,1,1,0);
        memcpy(expected.data()+result,&float_initial,sizeof(float));
    }
    if(float_compare){
        const auto target=GuestPixelOffset(desc,layout,0,1,2,0);
        const auto result=GuestPixelOffset(desc,layout,0,1,1,0);
        memcpy(memory+target,&compare_initial,sizeof(u32));
        memcpy(expected.data()+target,&compare_result,sizeof(u32));
        memcpy(expected.data()+result,&compare_initial,sizeof(u32));
    }
    if(subtract_atomic){
        memcpy(expected.data()+offset,position,sizeof(u32));
        const auto target=GuestPixelOffset(desc,layout,0,1,2,0);
        const auto result=GuestPixelOffset(desc,layout,0,1,1,0);
        memcpy(memory+target,&subtract_initial,sizeof(u32));
        memcpy(expected.data()+target,&subtract_result,sizeof(u32));
        memcpy(expected.data()+result,&subtract_initial,sizeof(u32));
    }
    std::array<u32,4> buffer_expected{};
    if(buffer_atomic){
        if(layout.guest_bytes>32768)throw std::runtime_error("Buffer fixture overlaps texture");
        // The image is only read by VS; PS writes exclusively to the separate buffer.
        expected.assign(memory,memory+layout.guest_bytes);
        std::array<u32,4> initial{0x5a5a5a5a,0x5a5a5a5a,
            subtract_atomic?subtract_initial:bounded_atomic?initial_atomic:float_compare?compare_initial:std::bit_cast<u32>(float_initial),0x5a5a5a5a};
        if(dynamic_operand){initial[0]=subtract_atomic?3U:bounded_atomic?atomic_limit:float_compare?0x40400000U:std::bit_cast<u32>(float_operand);
            initial[3]=compare_operand;}
        memcpy(memory+32768,initial.data(),sizeof(initial));
        buffer_expected=initial;if(!atomic_contention)buffer_expected[1]=initial[2];
        buffer_expected[2]=subtract_atomic?subtract_result:bounded_atomic?bounded_result:float_compare?compare_result:std::bit_cast<u32>(float_result);
        if(atomic64){
            memcpy(memory+32768+8,&initial64,sizeof(initial64));
            if(!atomic_contention)memcpy(buffer_expected.data(),&initial64,sizeof(initial64));
            memcpy(buffer_expected.data()+2,&result64,sizeof(result64));
        }
    }
    if(dimension_query){const u32 dimensions[]{8,one_dimensional?0U:8U,0,1};
        memcpy(expected.data()+offset,dimensions,sizeof(dimensions));}
    Rasterizer rasterizer(device.Get());rasterizer.MapMemory(address,allocation_bytes);
    GuestTextureCache textures(device.Get());
    const auto state=BuildGuestGraphicsState(regs,{});
    ShaderPipeline pipeline(device.Get(),std::filesystem::path(argv[2]).wstring(),
        {gpu_dxil.stages[u32(Shader::SwStage::Vertex)].bytecode,
         gpu_dxil.stages[u32(Shader::SwStage::Fragment)].bytecode},state.pipeline);
    const auto* vs=gpu_stages.stages[u32(Shader::SwStage::Vertex)];
    const auto* ps=gpu_stages.stages[u32(Shader::SwStage::Fragment)];
    const GuestResourceStage resources[]{{&ps->info,ps->bindings_start},{&vs->info,vs->bindings_start}};
    Shader::PushData push{};push.xscale=push.yscale=1;
    GraphicsRuntime runtime{};runtime.viewport_width=runtime.viewport_height=8;
    const D3D12_VIEWPORT viewport{0,0,8,8,0,1};const D3D12_RECT scissor{0,0,8,8};const float blend[]{1,1,1,1};
    if(border_custom)memcpy(memory+49152,border_table.data(),sizeof(border_table));
    ExecuteGuestGraphicsResources(device.Get(),rasterizer,pipeline,resources,push,runtime,
        [&](ResourceBindings& bindings){
            rasterizer.ExecuteGraphics(pipeline,bindings,state,{},nullptr,std::span(&viewport,1),
                std::span(&scissor,1),blend,{atomic_contention?4096U:1U},{},nullptr,false);
        },textures,border_custom?address+49152:0);
    rasterizer.Finish();
    if(buffer_atomic){
        rasterizer.ReadMemory(address+32768,sizeof(buffer_expected));
        if(memcmp(memory+32768,buffer_expected.data(),sizeof(buffer_expected)))
            throw std::runtime_error("GCN buffer atomic destination/previous value mismatch");
    }
    if(aniso_case){
        const auto output=GuestPixelOffset(desc,layout,0,0,2,0);
        std::array<float,4> actual{};memcpy(actual.data(),memory+(sample_readonly?sample_destination_offset:0)+output,sizeof(actual));
        for(u32 component=0;component<4;++component)
            if(!std::isfinite(actual[component])||std::abs(actual[component]-anisotropic_reference[component])>1.e-5f)
                throw std::runtime_error("Anisotropic GCN sample disagrees with independent footprint/mip/tap oracle");
        memcpy((sample_readonly?sampled_destination:expected).data()+output,actual.data(),sizeof(actual));
    }
    if(sample_readonly&&memcmp(memory+sample_destination_offset,sampled_destination.data(),sampled_destination.size()))
        throw std::runtime_error("Read-only sample destination or unrelated bytes changed");
    const bool constant_depth=sample_depth&&(mode.ends_with("-never")||mode.ends_with("-always"));
    if(memcmp(memory,expected.data(),expected.size())||textures.Entries().size()!=(sample_readonly&&!constant_depth?2:1))
        throw std::runtime_error("GCN graphics draw lost shared texture writeback or changed unrelated bytes");
    if(border_custom){
        border_table[7]={0.5f,0.25f,-1.f,0.125f};
        memcpy(memory+49152,border_table.data(),sizeof(border_table));
        rasterizer.InvalidateMemory(address+49152,sizeof(border_table));
        const float weight=border_weight;
        std::array<float,4> changed{};
        for(unsigned c=0;c<4;++c)changed[c]=border_table[7][c]*weight+(c==3?1.f-weight:0.f);
        memcpy((sample_readonly?sampled_destination:expected).data()+GuestPixelOffset(desc,layout,0,0,2,0),changed.data(),sizeof(changed));
        ExecuteGuestGraphicsResources(device.Get(),rasterizer,pipeline,resources,push,runtime,
            [&](ResourceBindings& bindings){
                rasterizer.ExecuteGraphics(pipeline,bindings,state,{},nullptr,std::span(&viewport,1),
                    std::span(&scissor,1),blend,{1U},{},nullptr,false);
            },textures,address+49152);
        rasterizer.Finish();
        if(memcmp(memory,expected.data(),expected.size())||(sample_readonly&&memcmp(memory+32768,sampled_destination.data(),sampled_destination.size())))
            throw std::runtime_error("Custom border update did not reach the unchanged pipeline");
    }
    for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T size{};Check(messages->GetMessage(i,nullptr,&size));
        std::vector<std::byte> data(size);auto* message=reinterpret_cast<D3D12_MESSAGE*>(data.data());
        Check(messages->GetMessage(i,message,&size));
        if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);
    }
    puts("PASS: selected GCN resource operations -> BuildGraphics -> native bindings -> linked DXIL -> hardware draw/writeback; alias split/relocation cache verified; no game");
    return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}

