// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_rasterizer.h"
#include "video_core/amdgpu/liverpool.h"
#include "video_core/amdgpu/cb_db_extent.h"
#include "core/emulator_settings.h"
#include "common/elf_info.h"
#include "gcn_ordered_formatted_fixture.h"
#include "gcn_readonly_numeric_fixture.h"
#include "gcn_writable_word_fixture.h"
#include "gcn_cached_depth_storage_fixture.h"
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <cstdio>
#include <cmath>
#include <bit>

using namespace NativeDX12;
static ComPtr<ID3D12Device> HardwareDevice(DXGI_FORMAT required_format=DXGI_FORMAT_UNKNOWN){
    ComPtr<ID3D12Debug> debug;Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> adapter;
        const auto hr=factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter));
        if(hr==DXGI_ERROR_NOT_FOUND)break;Check(hr);DXGI_ADAPTER_DESC1 desc{};Check(adapter->GetDesc1(&desc));
        if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;ComPtr<ID3D12Device> device;
        if(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)))){
            if(required_format!=DXGI_FORMAT_UNKNOWN){D3D12_FEATURE_DATA_FORMAT_SUPPORT support{required_format};
                const auto flags=D3D12_FORMAT_SUPPORT1_RENDER_TARGET|D3D12_FORMAT_SUPPORT1_BLENDABLE;
                if(FAILED(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support)))||(support.Support1&flags)!=flags)continue;
                printf("FIXTURE adapter=%u vendor=%04x device=%04x required_rt=%u name=%ls\n",i,desc.VendorId,desc.DeviceId,unsigned(required_format),desc.Description);
            }
            return device;
        }
    }
    throw std::runtime_error("No hardware D3D12 device");
}
int main(int argc,char** argv){try{
    if(argc!=4)throw std::invalid_argument("Expected converter, DXC and attachment mode");
    const bool signed_color=std::string_view(argv[3])=="signed-color";
    const auto mode=std::string_view(argv[3]);
    const bool owned=mode.ends_with("-owned");
    const bool writable_word=mode=="writable-packed10-word-owned";
    const bool linear_download=mode=="linear-download-owned";
    const bool linear_attachment=mode=="linear-attachment-owned";
    const bool separate_versions=mode=="separate-buffer-image-owned"||linear_download;
    const bool formatted_cmask=mode=="formatted-cmask-owned";
    const bool cached_depth_cmask6=mode=="cached-depth-cmask6-owned";
    const u32 cached_depth_cmask_packed=mode=="cached-depth-cmask16-owned"?16:mode=="cached-depth-cmask17-owned"?17:
        mode=="cached-depth-cmask18-owned"?18:mode=="cached-depth-cmask19-owned"?19:0;
    const bool cached_depth_cmask=cached_depth_cmask_packed||cached_depth_cmask6||mode=="cached-depth-cmask-owned";
    const bool cached_depth_storage_ordered=mode=="cached-depth-storage-ordered-owned";
    const u32 cached_depth_packed_storage=mode=="cached-depth-packed16-storage-owned"?16:mode=="cached-depth-packed17-storage-owned"?17:
        mode=="cached-depth-packed18-storage-owned"?18:mode=="cached-depth-packed19-storage-owned"?19:0;
    const bool cached_depth_storage=cached_depth_packed_storage||cached_depth_storage_ordered||cached_depth_cmask||mode=="cached-depth-storage-owned";
    const bool ordered_formatted=mode=="ordered-formatted-owned";
    const bool readonly_numeric_exclusive=mode=="readonly-numeric-exclusive-owned";
    const bool readonly_packed_ms=mode=="readonly-snormword-msaa-owned"||mode=="readonly-sintword-msaa-owned";
    const bool readonly_packed_sample=mode=="readonly-snormword-sample-owned";
    const u32 readonly_signed_word=(readonly_packed_sample||mode=="readonly-snormword-msaa-owned"||mode=="readonly-snormword-owned")?1:(mode=="readonly-sintword-owned"||mode=="readonly-sintword-msaa-owned")?5:0;
    const bool readonly_mutable32=readonly_signed_word||mode=="readonly-mutable32-owned";
    const bool readonly_numeric=readonly_mutable32||readonly_numeric_exclusive||mode=="readonly-numeric-owned";
    const bool image_writer=mode=="readonly-depth-image"||mode=="readonly-depth-image-owned"||
        mode=="mixed-depth-image"||mode=="mixed-depth-image-owned";
    const bool mixed_planes=mode=="mixed-depth-buffer"||mode=="mixed-depth-buffer-owned"||
        mode=="mixed-depth-image"||mode=="mixed-depth-image-owned";
    const bool planned_depth=mode=="planned-depth-recreation-owned";
    const bool readonly_depth=planned_depth||mixed_planes||image_writer||mode=="readonly-depth-buffer"||mode=="readonly-depth-buffer-owned";
    if(!writable_word&&!readonly_numeric&&!cached_depth_storage&&!signed_color&&!readonly_depth&&!separate_versions&&!formatted_cmask&&!ordered_formatted&&!linear_attachment&&mode!="base")throw std::invalid_argument("Invalid attachment mode");
    Common::ElfInfo::Instance().InitializeApp("DX12FIXTURE", "Headless renderer fixture", "1.0",
        Common::ElfInfo::FW_900, Common::ElfInfo::FW_900, {});
    EmulatorSettings.SetPipelineCacheEnabled(false);EmulatorSettings.SetShaderCollect(false);EmulatorSettings.SetDumpShaders(false);
    if(linear_download)EmulatorSettings.SetReadbackLinearImagesEnabled(false);
    if(linear_attachment)EmulatorSettings.SetReadbackLinearImagesEnabled(true);
    _putenv_s("SHADPS4_NATIVE_SHADER_CACHE","off");
    auto device=HardwareDevice(cached_depth_cmask_packed==19?DXGI_FORMAT_B4G4R4A4_UNORM:DXGI_FORMAT_UNKNOWN);
    ComPtr<ID3D12InfoQueue> messages;Check(device.As(&messages));
    if(readonly_numeric&&GetEnvironmentVariableW(L"SHADPS4_FIXTURE_BREAK_ON_GPU_ERROR",nullptr,0))
        Check(messages->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR,TRUE));
    struct Allocation{HANDLE section{};std::byte* guest{};std::byte* backing{};
        ~Allocation(){if(guest)UnmapViewOfFile(guest);if(backing)UnmapViewOfFile(backing);if(section)CloseHandle(section);}} allocation;
    allocation.section=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,65536,nullptr);
    if(!allocation.section)throw std::runtime_error("Attachment physical backing allocation failed");
    allocation.guest=static_cast<std::byte*>(MapViewOfFileEx(allocation.section,FILE_MAP_ALL_ACCESS,0,0,65536,
        reinterpret_cast<void*>(0x1000000000ULL)));
    allocation.backing=static_cast<std::byte*>(MapViewOfFile(allocation.section,FILE_MAP_ALL_ACCESS,0,0,65536));
    if(!allocation.guest||!allocation.backing)throw std::runtime_error("Attachment guest/backing alias mapping failed");
    auto* memory=allocation.guest;auto* backing=allocation.backing;
    const auto base=reinterpret_cast<u64>(memory);
    alignas(256) std::array<u32,128> vertex{},pixel{},plain_vertex{},metadata_pixel{};
    const auto initialize=[](auto& program,const std::vector<u32>& words,u64 hash){
        if(words.size()>32)throw std::runtime_error("GCN program overlaps binary metadata");
        std::copy(words.begin(),words.end(),program.begin());AmdGpu::BinaryInfo binary{};
        binary.signature=binary.signature_ref;binary.length=u32(words.size()*4);binary.shader_hash=hash;
        memcpy(program.data()+32,&binary,sizeof(binary));
    };
    std::vector<u32> vertex_words{0xBEEB03FF,15};
    for(u32 i=0;i<4;++i){vertex_words.push_back(0xe0302000U|i*4);vertex_words.push_back((128U<<24)|((4+i)<<8));}
    vertex_words.insert(vertex_words.end(),{0xbf8c007f,0xf80008cf,0x07060504,0xbf810000});
    initialize(vertex,vertex_words,0x4154544143485601ULL);
    initialize(plain_vertex,vertex_words,0x4154544143485602ULL);
    initialize(metadata_pixel,{0xBEEB03FF,15,0xe0000000,0x80000400,0xbf8c0000,
        0x7e0a0280,0x7e0c0280,0x7e0e02ff,0x3f800000,0xf800000f,0x07060504,0xbf810000},
        0x4154544d45545001ULL);
    initialize(pixel,{0xBEEB03FF,15,0x7e0002ff,0x3f800000,0x7e020280,0x7e040280,0x7e0602ff,0x3f800000,
        0xf800000f,0x03020100,0x7e000280,0x7e0202ff,0x3f800000,0xf800001f,0x03020100,
        0x7e020280,0x7e0402ff,0x3f800000,0xf800082f,0x03020100,0xbf810000},0x4154544143485001ULL);
    if(signed_color)initialize(pixel,{0xBEEB03FF,15,0x7e0002ff,u32(-512),0x7e0202ff,u32(-1),0x7e040280,0x7e060281,
        0xf800000f,0x03020100,0x7e0002ff,511,0x7e0402ff,u32(-512),0xf800001f,0x03020100,
        0x7e020280,0x7e0602ff,u32(-2),0xf800082f,0x03020100,0xbf810000},0x41545453494E5401ULL);
    AmdGpu::Liverpool gpu;
    Shader::Profile profile{};profile.supported_spirv=0x00010500;profile.needs_buffer_offsets=true;
    profile.native_coherent_images=true;profile.max_viewport_width=profile.max_viewport_height=16384;
    profile.native_buffer_bounds=true;
    profile.support_int8=profile.support_int16=true;
    ReadonlyNumericProbeRasterizer rasterizer(device.Get(),gpu,profile,std::filesystem::path(argv[1]).wstring(),
        std::filesystem::path(argv[2]).wstring(),readonly_numeric);
    rasterizer.SetBackingAccess([base,owned](VAddr address,u64 bytes){
        return owned&&address>=base&&address-base<=65536&&bytes<=65536-(address-base);
    },
        [base,backing](VAddr address,std::span<std::byte> data){
            if(address<base||address-base>65536||data.size()>65536-(address-base))return false;
            memcpy(data.data(),backing+(address-base),data.size());return true;
        },[base,backing](VAddr address,std::span<const std::byte> data){
            if(address<base||address-base>65536||data.size()>65536-(address-base))return false;
            memcpy(backing+(address-base),data.data(),data.size());return true;
        });
    rasterizer.MapMemory(base,65536);rasterizer.NotifyHostMapping(base,65536,true,true);
    if(writable_word){
        RunWritableWordGcnFixture(rasterizer,gpu,memory,backing,base);
    }else if(readonly_numeric){
        RunReadonlyNumericGcnFixture(rasterizer,gpu,memory,backing,base,readonly_numeric_exclusive,readonly_mutable32,readonly_signed_word,readonly_packed_sample,readonly_packed_ms);
    }else if(cached_depth_storage){
        RunCachedDepthStorageGcnFixture(rasterizer,gpu,plain_vertex,vertex_words,cached_depth_storage_ordered,memory,backing,base,initialize,cached_depth_cmask,cached_depth_cmask_packed?cached_depth_cmask_packed:cached_depth_cmask6?6:10,cached_depth_packed_storage);
    }else if(ordered_formatted){
        RunOrderedFormattedGcnFixture(rasterizer,gpu,profile,memory,backing,base);
    }else if(formatted_cmask){
        initialize(pixel,{0xBEEB03FF,15,0x7e0002ff,0x3f800000,0x7e020280,0x7e040280,
            0x7e0602ff,0x3f800000,0xf800000f,0x03020100,0xbf810000},0x415454434d415301ULL);
        for(u32 log2_samples:{0u,1u,2u,3u})for(u32 warm=0;warm<3;++warm){
            rasterizer.ReadMemory(base,65536);rasterizer.InvalidateMemory(base,65536);
            memset(memory,int(0xa1+warm),65536);memset(memory+32768,0xff,32768);
            const float xy[3][2]{{-1,-1},{3,-1},{-1,3}};
            for(unsigned i=0;i<3;++i){const float position[]{xy[i][0],xy[i][1],0.5f,1.f};memcpy(memory+i*32,position,sizeof(position));}
            auto& regs=gpu.regs;regs={};regs.stage_enable.raw=AmdGpu::ShaderStageEnable::Vs;
            regs.primitive_type=AmdGpu::PrimitiveType::TriangleList;regs.num_indices=3;
            regs.vs_program.address=reinterpret_cast<u64>(plain_vertex.data())>>8;
            regs.vs_program.settings.num_user_regs=4;regs.vs_program.settings.num_vgprs=1;
            regs.ps_program.address=reinterpret_cast<u64>(pixel.data())>>8;
            regs.color_export_format.raw=u32(AmdGpu::ShaderExportFormat::ABGR_32);
            regs.color_control.mode=AmdGpu::ColorControl::OperationMode::Normal;
            regs.color_control.rop3=AmdGpu::ColorControl::LogicOp::Copy;
            regs.color_target_mask.raw=regs.color_shader_mask.raw=15;
            regs.clipper_control.clip_space=AmdGpu::ClipSpace::ZeroToW;
            auto& vp=regs.viewport_control;
            vp.xscale_enable=vp.yscale_enable=vp.xoffset_enable=vp.yoffset_enable=vp.zscale_enable=1;
            regs.viewports[0].xscale=4;regs.viewports[0].yscale=-4;
            regs.viewports[0].xoffset=regs.viewports[0].yoffset=4;regs.viewports[0].zscale=1;
            regs.screen_scissor.bottom_right_x=regs.window_scissor.bottom_right_x=regs.generic_scissor.bottom_right_x=8;
            regs.screen_scissor.bottom_right_y=regs.window_scissor.bottom_right_y=regs.generic_scissor.bottom_right_y=8;
            AmdGpu::Buffer input{};input.base_address=base;input.stride=32;input.num_records=3;
            input.data_format=4;input.num_format=4;input.dst_sel_x=4;input.dst_sel_y=5;input.dst_sel_z=6;input.dst_sel_w=7;
            memcpy(regs.vs_program.user_data.data(),&input,sizeof(input));
            auto& cb=regs.color_buffers[0];cb.base_address=u32((base+4096)>>8);
            cb.info.format=10;cb.info.number_type=0;cb.info.fast_clear=1;
            cb.cmask_base_address=u32((base+32768)>>8);cb.attrib.tile_mode_index=AmdGpu::TileMode::Thin1DThin;
            cb.attrib.num_fragments_log2=log2_samples;cb.pitch.tile_max=cb.slice.tile_max=1;
            gpu.last_cb_extent[0]={8,8};
            const auto color=DescribeGuestColorBuffer(cb,gpu.last_cb_extent[0]);const auto layout=LayoutGuestTexture(color);
            if(layout.guest_bytes>4096||GuestCmaskBytes(cb)>32768)
                throw std::runtime_error("CMASK formatted fixture geometry overlaps");
            std::vector<std::byte> expected(backing,backing+65536);
            const auto check_frame=[&](u8 red){
                const u32 rgba=u32(red)|0xff000000;
                for(u32 sample=0;sample<color.samples;++sample)for(u32 y=0;y<8;++y)for(u32 x=0;x<8;++x)
                    memcpy(expected.data()+4096+GuestPixelOffset(color,layout,0,0,x,y,sample),&rgba,4);
                rasterizer.ReadMemory(color.address,layout.guest_bytes);rasterizer.Finish();
                if(memcmp(backing,expected.data(),65536))
                    throw std::runtime_error("Actual GCN CMASK sample pattern, CPU-stream distinction or full physical padding mismatch");
            };
            // The real fast-clear-enabled color draw registers the actual
            // metadata owner/sample count; expanded physical bytes stay intact.
            rasterizer.Draw(false);check_frame(255);
            regs.ps_program.address=reinterpret_cast<u64>(metadata_pixel.data())>>8;
            regs.ps_program.settings.num_user_regs=4;regs.ps_program.settings.num_vgprs=1;
            AmdGpu::Buffer formatted{};formatted.base_address=base+32768;formatted.stride=4;
            formatted.data_format=10;formatted.num_format=0;
            formatted.dst_sel_x=4;formatted.dst_sel_y=5;formatted.dst_sel_z=6;formatted.dst_sel_w=7;
            const auto draw_formatted=[&](u32 records,u8 red){
                formatted.num_records=records;memcpy(regs.ps_program.user_data.data(),&formatted,sizeof(formatted));
                rasterizer.Draw(false);check_frame(red);
            };
            const u8 expanded=log2_samples==1?221:log2_samples==2?238:255;
            draw_formatted(8192,expanded); // large clean arena: synthesized metadata
            draw_formatted(1024,255);     // small clean stream: physical 0xff bytes
            draw_formatted(8192,expanded);
        }
    }else if(separate_versions){
        // The original keeps the post-write buffer and image versions separate.
        // CPU reads consume only the buffer, while image loads keep seeing the
        // image's own result until a CPU write or exact-base buffer write.
        alignas(256) std::array<u32,128> both_vertex{},buffer_vertex{},byte_vertex{},half_vertex{},image_vertex{},red_pixel{},load_pixel{},next_pixel{},first_pixel{},formatted_pixel{},raw_byte_pixel{},raw_float_pixel{},zero_clear_compute{};
        std::vector<u32> zero_clear_words{0x7e000280};
        for(u32 component=0;component<4;++component){
            zero_clear_words.push_back(0xe0300000U|component*4);
            zero_clear_words.push_back(0x80000000U|((4+component)<<8));
        }
        zero_clear_words.insert(zero_clear_words.end(),{0xbf8c0000,0xe0101000,0x80010400,0xbf810000});
        initialize(zero_clear_compute,zero_clear_words,0x8b355b5a);
        auto both_words=vertex_words;both_words.resize(both_words.size()-3);
        both_words.insert(both_words.end(),{0x7e020280,0xe0700000,0x80010100,
            0x7e040280,0x7e1002ff,0x3e800000,0xf0200100,0x00020801,
            0xf80008cf,0x07060504,0xbf810000});
        initialize(both_vertex,both_words,0x4154544455415601ULL);
        auto buffer_words=vertex_words;buffer_words.resize(buffer_words.size()-3);
        buffer_words.insert(buffer_words.end(),{0x7e020280,0xe0700000,0x80010100,
            0xf80008cf,0x07060504,0xbf810000});
        initialize(buffer_vertex,buffer_words,0x4154544455415602ULL);
        auto image_words=vertex_words;image_words.resize(image_words.size()-3);
        image_words.insert(image_words.end(),{0x7e020280,0x7e040280,0x7e1002ff,0x3f000000,
            0xf0200100,0x00010801,0xf80008cf,0x07060504,0xbf810000});
        initialize(image_vertex,image_words,0x4154544455415720ULL);
        initialize(red_pixel,{0xBEEB03FF,15,0x7e0002ff,0x3f800000,0x7e020280,0x7e040280,
            0x7e0602ff,0x3f800000,0xf800000f,0x03020100,0xbf810000},0x4154544455415001ULL);
        const auto convert_integer=[](std::vector<u32>& words,bool enabled,bool signed_value,float scale){
            if(enabled)words.insert(words.end(),{signed_value?0x7e080b04U:0x7e080d04U,
                0x100808ff,std::bit_cast<u32>(1.f/scale)});
        };
        const auto image_load=[&](u32 x,u32 array_flag,u32 layer,bool integer=false,bool signed_value=false,float scale=1.f){
            std::vector<u32> words{0xBEEB03FF,15,0x7e000280U|x,0x7e020280,0x7e040280U|layer,
                0xf0000f00U|array_flag,0x00000400,0xbf8c0000};
            convert_integer(words,integer,signed_value,scale);
            // The color oracle observes R; all guest channels remain covered
            // independently by the complete physical allocation comparison.
            words.insert(words.end(),{0x7e0a0280,0x7e0c0280,0x7e0e02ff,0x3f800000,
                0xf800000f,0x07060504,0xbf810000});return words;
        };
        initialize(load_pixel,image_load(0,0,0),0x4154544455415002ULL);
        initialize(next_pixel,image_load(1,0,0),0x4154544455415003ULL);
        initialize(formatted_pixel,{0xBEEB03FF,15,0xe0000000,0x80000400,0xbf8c0000,
            0x7e0a0280,0x7e0c0280,0x7e0e02ff,0x3f800000,0xf800000f,0x07060504,0xbf810000},
            0x4154544455415004ULL);
        auto& regs=gpu.regs;regs={};regs.stage_enable.raw=AmdGpu::ShaderStageEnable::Vs;
        regs.primitive_type=AmdGpu::PrimitiveType::TriangleList;regs.num_indices=3;
        regs.color_export_format.raw=u32(AmdGpu::ShaderExportFormat::ABGR_32);
        regs.color_control.mode=AmdGpu::ColorControl::OperationMode::Normal;
        regs.color_control.rop3=AmdGpu::ColorControl::LogicOp::Copy;
        regs.color_target_mask.raw=regs.color_shader_mask.raw=15;
        regs.clipper_control.clip_space=AmdGpu::ClipSpace::ZeroToW;
        auto& vp=regs.viewport_control;
        vp.xscale_enable=vp.yscale_enable=vp.xoffset_enable=vp.yoffset_enable=vp.zscale_enable=1;
        regs.viewports[0].xscale=4;regs.viewports[0].yscale=-4;
        regs.viewports[0].xoffset=regs.viewports[0].yoffset=4;regs.viewports[0].zscale=1;
        regs.screen_scissor.bottom_right_x=regs.window_scissor.bottom_right_x=regs.generic_scissor.bottom_right_x=8;
        regs.screen_scissor.bottom_right_y=regs.window_scissor.bottom_right_y=regs.generic_scissor.bottom_right_y=8;
        auto& cb=regs.color_buffers[0];cb.base_address=u32((base+61440)>>8);
        cb.info.format=10;cb.info.number_type=0;cb.attrib.tile_mode_index=AmdGpu::TileMode::Thin1DThin;
        cb.pitch.tile_max=cb.slice.tile_max=1;gpu.last_cb_extent[0]={8,8};
        const auto color=DescribeGuestColorBuffer(cb,gpu.last_cb_extent[0]);const auto cl=LayoutGuestTexture(color);
        AmdGpu::Buffer input{};input.base_address=base;input.stride=32;input.num_records=3;
        input.data_format=4;input.num_format=4;input.dst_sel_x=4;input.dst_sel_y=5;input.dst_sel_z=6;input.dst_sel_w=7;
        AmdGpu::Buffer target{};target.base_address=base+16384;target.stride=4;target.num_records=1024;
        target.data_format=4;target.num_format=4;
        target.dst_sel_x=4;target.dst_sel_y=5;target.dst_sel_z=6;target.dst_sel_w=7;
        AmdGpu::Image image{};image.base_address=(base+16384)>>8;
        image.type=u64(AmdGpu::ImageType::Color2D);
        image.data_format=4;image.num_format=7;image.tiling_index=5;
        image.dst_sel_x=4;image.dst_sel_y=5;image.dst_sel_z=6;image.dst_sel_w=7;
        for(u32 data_format:{1u,2u,3u,4u,5u,6u,7u,9u,10u,11u,12u,13u,14u,16u,17u,18u,19u})for(u32 layers:{1u,2u,4u,6u})
        for(u32 selected_mip:{0u,1u})for(u32 number_format:{0u,1u,4u,5u,7u,9u}){
        if(linear_download&&(data_format!=4||number_format!=7||layers>2))continue;
        const bool half_components=data_format==2||data_format==5||data_format==12;
        const bool byte_components=data_format==1||data_format==3||data_format==10;
        const bool packed10=data_format==9;
        const bool packed16=data_format>=16&&data_format<=19;
        const bool packed_float=data_format==6||data_format==7;
        const bool srgb=data_format==10&&number_format==9;
        const bool normalized=(byte_components||half_components||packed10||packed16)&&(number_format==0||number_format==1||srgb);
        const bool small_integer=(byte_components||half_components||packed10)&&(number_format==4||number_format==5);
        const bool signed_components=(normalized&&number_format==1)||(small_integer&&number_format==5);
        if(packed_float?number_format!=7:packed16?number_format!=0:packed10?(number_format!=0&&number_format!=1&&number_format!=4&&number_format!=5):
           byte_components?(number_format!=0&&number_format!=1&&number_format!=4&&number_format!=5&&!srgb):
           half_components?(number_format!=0&&number_format!=1&&number_format!=4&&number_format!=5&&number_format!=7):
           (number_format!=4&&number_format!=5&&number_format!=7))continue;
        const bool cube=layers==6,volume=layers==4;
        if(cube&&selected_mip)continue; // Six base faces fit the complete physical allocation.
        if(data_format==13&&(cube||selected_mip))continue; // Page-sized RGB32 base chains fit one/two slices.
        const u32 selected_layer=volume?1:layers-1,array_flag=!volume&&layers>1?0x4000:0;
        const u64 shape=data_format*1024+layers*32+selected_mip*16+number_format;
        image.type=u64(cube?AmdGpu::ImageType::Cube:volume?AmdGpu::ImageType::Color3D:
            layers>1?AmdGpu::ImageType::Color2DArray:AmdGpu::ImageType::Color2D);
        image.depth=volume?(selected_mip?3:1):cube?0:layers-1;
        image.base_array=0;image.last_array=volume?0:layers-1;
        image.data_format=data_format;
        const u32 pixel_bytes=AmdGpu::NumBitsPerElement(image.GetDataFmt())/8;
        const u32 base_width=pixel_bytes==1?64:pixel_bytes<=4?32:16;
        const u32 base_height=base_width*((pixel_bytes==2||pixel_bytes==8)?2:1);
        image.width=image.pitch=(base_width<<selected_mip)-1;
        image.height=((cube?base_width:base_height)<<selected_mip)-1;
        if(volume){
            image.width=image.pitch=base_width-1;
            image.height=(base_height<<selected_mip)-1;
        }
        if(data_format==13)image.width=image.height=image.pitch=31;
        image.base_level=image.last_level=selected_mip;
        if(linear_download)image.tiling_index=8;
        image.num_format=number_format;
        const u32 packed16_shift=data_format==18?10:data_format==19?8:0;
        const float numeric_scale=packed16?(data_format==19?15.f:31.f):packed10?(signed_components?511.f:1023.f):half_components?(signed_components?32767.f:65535.f):signed_components?127.f:255.f;
        const u8 buffer_quarter_red=packed16?(data_format==19?68:66):64;
        const u32 quarter_word=packed_float?((13u<<(data_format==7?5:6))<<(data_format==7?22:0)):
            packed16?((data_format==19?4u:8u)<<packed16_shift):packed10?(signed_components?128:256):half_components?(number_format==7?0x3400:signed_components?8192:16384):
            byte_components?(signed_components?32:64):0x3e800000;
        const u32 half_word=packed_float?((14u<<(data_format==7?5:6))<<(data_format==7?22:0)):
            packed16?((data_format==19?8u:16u)<<packed16_shift):packed10?(signed_components?256:512):half_components?(number_format==7?0x3800:signed_components?16384:32768):
            byte_components?(signed_components?64:128):0x3f000000;
        const u32 one_word=packed_float?((15u<<(data_format==7?5:6))<<(data_format==7?22:0)):
            packed16?(u32(numeric_scale)<<packed16_shift):packed10?(signed_components?511:1023):half_components?(number_format==7?0x3c00:signed_components?32767:65535):
            byte_components?(signed_components?127:255):0x3f800000;
        const u8 image_quarter_red=srgb?13:buffer_quarter_red;
        const u8 image_half_red=packed16?(data_format==19?136:132):srgb?55:signed_components&&!half_components&&!packed10?129:128;
        const auto desc=DescribeGuestTexture(image);const auto layout=LayoutGuestTexture(desc);
        const auto store_offset=GuestPixelOffset(desc,layout,selected_mip,selected_layer,0,0);
        const u64 extended_bytes=std::max<u64>(layout.guest_bytes+4096,20480);
        const u64 short_bytes=store_offset+layout.bytes_per_pixel;
        if(!layout.guest_bytes||layout.guest_bytes%4096||layout.guest_bytes>45056||cl.guest_bytes>4096||
           store_offset>UINT32_MAX||store_offset+8>layout.guest_bytes||extended_bytes>45056||short_bytes>=layout.guest_bytes)
            throw std::runtime_error("Independent-version fixture geometry changed");
        target.num_records=u32(layout.guest_bytes/4);
        both_words=vertex_words;both_words.resize(both_words.size()-3);
        both_words.insert(both_words.end(),{0x7e0002ff,u32(store_offset),0x7e020280,0xe0701000,0x80010100,
            0x7e040280,0x7e060280U|selected_layer,0x7e1002ff,small_integer?quarter_word:0x3e800000,0xf0200100U|array_flag,0x00020801,
            0xf80008cf,0x07060504,0xbf810000});
        initialize(both_vertex,both_words,0x4154544455420000ULL+shape);
        buffer_words=vertex_words;buffer_words.resize(buffer_words.size()-3);
        buffer_words.insert(buffer_words.end(),{0x7e0002ff,u32(store_offset),0x7e020280,0xe0701000,0x80010100,
            0xf80008cf,0x07060504,0xbf810000});
        initialize(buffer_vertex,buffer_words,0x4154544455430000ULL+shape);
        auto byte_words=vertex_words;byte_words.resize(byte_words.size()-3);
        byte_words.insert(byte_words.end(),{0x7e000280,0x7e0202ff,0x35,0xe0601000,0x80010100,
            0xf80008cf,0x07060504,0xbf810000});
        initialize(byte_vertex,byte_words,0x4154544455490000ULL+shape);
        auto half_words=vertex_words;half_words.resize(half_words.size()-3);
        half_words.insert(half_words.end(),{0x7e000280,0x7e0202ff,0x7a35,0xe0681000,0x80010100,
            0xf80008cf,0x07060504,0xbf810000});
        initialize(half_vertex,half_words,0x41545444554a0000ULL+shape);
        image_words=vertex_words;image_words.resize(image_words.size()-3);
        image_words.insert(image_words.end(),{0x7e020280,0x7e040280,0x7e060280,0x7e1002ff,small_integer?half_word:0x3f000000,
            0xf0200100U|array_flag,0x00010801,0xf80008cf,0x07060504,0xbf810000});
        initialize(image_vertex,image_words,0x4154544455440000ULL+shape);
        initialize(load_pixel,image_load(0,array_flag,selected_layer,small_integer,signed_components,numeric_scale),0x4154544455450000ULL+shape);
        initialize(next_pixel,image_load(1,array_flag,selected_layer,small_integer,signed_components,numeric_scale),0x4154544455460000ULL+shape);
        initialize(first_pixel,image_load(0,array_flag,0,small_integer,signed_components,numeric_scale),0x4154544455470000ULL+shape);
        std::vector<u32> formatted_words{0xBEEB03FF,15,0x7e0002ff,u32(store_offset),0xe0001000,0x80000400,0xbf8c0000};
        if(packed16){
            // Original formatted buffers do not support these packed data formats.
            // Read their actual R16_UINT bytes, then extract the guest red field.
            formatted_words.insert(formatted_words.end(),{0x2c0808ff,packed16_shift,0x360808ff,u32(numeric_scale)});
        }
        convert_integer(formatted_words,small_integer||packed16,signed_components,numeric_scale);
        formatted_words.insert(formatted_words.end(),{0x7e0a0280,0x7e0c0280,0x7e0e02ff,0x3f800000,
            0xf800000f,0x07060504,0xbf810000});
        initialize(formatted_pixel,formatted_words,0x4154544455480000ULL+shape);
        std::vector<u32> raw_byte_words{0xBEEB03FF,15,0x7e0002ff,u32(store_offset),
            0xe0301000,0x80000400,0xbf8c0000,0x360808ff,255};
        convert_integer(raw_byte_words,true,false,255.f);
        raw_byte_words.insert(raw_byte_words.end(),{0x7e0a0280,0x7e0c0280,0x7e0e02ff,0x3f800000,
            0xf800000f,0x07060504,0xbf810000});
        initialize(raw_byte_pixel,raw_byte_words,0x41545444554b0000ULL+shape);
        initialize(raw_float_pixel,{0xBEEB03FF,15,0x7e0002ff,u32(store_offset),
            0xe0301000,0x80000400,0xbf8c0000,0x7e0a0280,0x7e0c0280,0x7e0e02ff,0x3f800000,
            0xf800000f,0x07060504,0xbf810000},0x41545444554c0000ULL+shape);
        u64 writer_bytes=layout.guest_bytes;
        const auto bind_vertex=[&](const auto& program,u32 users){
            regs.vs_program.address=reinterpret_cast<u64>(program.data())>>8;
            regs.vs_program.settings.num_user_regs=users;regs.vs_program.settings.num_vgprs=2;
            memcpy(regs.vs_program.user_data.data(),&input,sizeof(input));
            auto vertex_target=target;
            if(program.data()==both_vertex.data())vertex_target.num_records=u32(writer_bytes/4);
            memcpy(regs.vs_program.user_data.data()+4,&vertex_target,sizeof(vertex_target));
            memcpy(regs.vs_program.user_data.data()+8,&image,sizeof(image));
        };
        const auto bind_pixel=[&](const auto& program){
            regs.ps_program.address=reinterpret_cast<u64>(program.data())>>8;
            regs.ps_program.settings.num_user_regs=8;regs.ps_program.settings.num_vgprs=1;
            memcpy(regs.ps_program.user_data.data(),&image,sizeof(image));
        };
        for(const u64 writer_extent:{layout.guest_bytes,store_offset+4,extended_bytes})
        for(const float initial:{0.f,1.f,0.5f,-0.5f,-1.f}){
            if(linear_download&&(writer_extent!=layout.guest_bytes||(initial!=0.f&&initial!=0.5f)))continue;
            if(initial==-1.f&&!(packed10&&number_format==1))continue;
            writer_bytes=writer_extent;
            u32 initial_bits{};memcpy(&initial_bits,&initial,4);
            float initial_value=initial;
            if(packed_float){
                const u32 exponent=initial<=0.f?0:initial==1.f?15:14;
                const u32 rg=exponent<<6,blue=exponent<<5;
                initial_bits=rg|(rg<<11)|(blue<<22);initial_value=std::max(initial,0.f);
            }else if(half_components&&number_format==7){
                const u32 component=initial==0.f?0:initial==1.f?0x3c00:initial==0.5f?0x3800:0xb800;
                initial_bits=component|(component<<16);
            }else if(normalized||small_integer){
                const float scale=numeric_scale;
                const int component=packed10&&number_format==1&&initial==-1.f?-512:
                    int(std::round(std::clamp(initial,signed_components?-1.f:0.f,1.f)*scale));
                if(packed16){
                    const u32 r=u32(component),alpha=u32(std::round(std::clamp(initial,0.f,1.f)*(data_format==19?15.f:1.f)));
                    if(data_format==16){const u32 green=u32(std::round(std::clamp(initial,0.f,1.f)*63.f));
                        initial_bits=(r<<11)|(green<<5)|r;
                    }else if(data_format==19)initial_bits=(r<<4)|(r<<8)|(r<<12)|alpha;
                    else initial_bits=(r<<10)|(r<<5)|r|(alpha<<15);
                    initial_bits|=initial_bits<<16;
                }else if(packed10){
                    const u32 rgb=u32(component)&1023;
                    const int alpha_value=number_format==1&&initial==-1.f?-2:
                        int(std::round(std::clamp(initial,signed_components?-1.f:0.f,1.f)*(signed_components?1.f:3.f)));
                    const u32 alpha=u32(alpha_value)&3;
                    initial_bits=rgb|(rgb<<10)|(rgb<<20)|(alpha<<30);
                }else initial_bits=half_components?u32(u16(component))*0x00010001U:u32(u8(component))*0x01010101U;
                initial_value=std::max(float(component)/scale,-1.f);
            }
            rasterizer.ReadMemory(base,65536);rasterizer.InvalidateMemory(base,65536);memset(memory,0xee,65536);
            const float xy[3][2]{{-1,-1},{3,-1},{-1,3}};
            for(unsigned i=0;i<3;++i){const float position[]{xy[i][0],xy[i][1],0.5f,1.f};memcpy(memory+i*32,position,sizeof(position));}
            for(u32 mip=0;mip<desc.levels;++mip)
            for(u32 layer=0;layer<(desc.volume_depth?std::max(desc.volume_depth>>mip,1u):desc.layers);++layer)
            for(u32 y=0;y<std::max(desc.height>>mip,1u);++y)
            for(u32 x=0;x<std::max(desc.width>>mip,1u);++x)
            for(u32 component=0;component<layout.bytes_per_pixel;component+=4)
                memcpy(memory+16384+GuestPixelOffset(desc,layout,mip,layer,x,y)+component,&initial_bits,
                    std::min(layout.bytes_per_pixel-component,4u));
            std::vector<std::byte> expected(backing,backing+65536);
            const auto check_color=[&](u8 red){
                const u32 rgba=u32(red)|0xff000000;
                for(u32 y=0;y<8;++y)for(u32 x=0;x<8;++x)
                    memcpy(expected.data()+61440+GuestPixelOffset(color,cl,0,0,x,y),&rgba,4);
                rasterizer.Finish();
                if(memcmp(backing,expected.data(),65536))
                    throw std::runtime_error("Independent GCN buffer/image versions, CPU visibility or full-allocation padding mismatch");
            };
            const auto read_buffer=[&](u32 value,bool image_contents=false){
                if(*reinterpret_cast<volatile u32*>(memory+16384+store_offset)!=value)
                    throw std::runtime_error("CPU read did not expose the current buffer-domain result");
                memcpy(expected.data()+16384+store_offset,&value,4);
                // IMAGE_STORE dmask R lowers absent vector channels to zero;
                // the raw buffer writer modifies only its first DWORD.
                if(image_contents&&layout.bytes_per_pixel>4)
                    memset(expected.data()+16384+store_offset+4,0,layout.bytes_per_pixel-4);
            };
            const auto draw_image=[&](const auto& program,u8 red){
                bind_vertex(vertex,4);bind_pixel(program);rasterizer.Draw(false);check_color(red);
            };
            const auto draw_formatted=[&](u8 red,u64 bytes=0){
                bind_vertex(vertex,4);bind_pixel(formatted_pixel);regs.ps_program.settings.num_user_regs=4;
                auto formatted=target;formatted.num_format=7;
                if(normalized||half_components||small_integer||packed_float){
                    // A buffer sharp has only three number-format bits;
                    // the sRGB image's physical bytes are read through UNORM.
                    formatted.data_format=packed16?2:data_format;formatted.num_format=packed16?4:srgb?0:number_format;
                    formatted.stride=layout.bytes_per_pixel;formatted.num_records=u32(layout.guest_bytes/layout.bytes_per_pixel);
                }
                if(bytes)formatted.num_records=u32(bytes/formatted.stride);
                memcpy(regs.ps_program.user_data.data(),&formatted,sizeof(formatted));
                rasterizer.Draw(false);check_color(red);
            };
            const auto draw_raw_byte=[&](u8 red){
                // A large raw read uses the resident arena and cannot refresh
                // it from a cached image or stream the physical CPU bytes.
                bind_vertex(vertex,4);bind_pixel(raw_byte_pixel);regs.ps_program.settings.num_user_regs=4;
                auto raw=target;raw.num_records=u32(extended_bytes/4);
                memcpy(regs.ps_program.user_data.data(),&raw,sizeof(raw));
                rasterizer.Draw(false);check_color(red);
            };
            const auto draw_raw_float=[&](u8 red){
                bind_vertex(vertex,4);bind_pixel(raw_float_pixel);regs.ps_program.settings.num_user_regs=4;
                auto raw=target;raw.num_records=u32(extended_bytes/4);
                memcpy(regs.ps_program.user_data.data(),&raw,sizeof(raw));
                rasterizer.Draw(false);check_color(red);
            };
            const auto clear_image=[&](const float (&values)[4]){
                rasterizer.NotifyCpuWrite(base+8192,16);memcpy(memory+8192,values,16);
                memcpy(expected.data()+8192,values,16);
                auto& program=gpu.GetCsRegs();program={};
                program.address=reinterpret_cast<u64>(zero_clear_compute.data())>>8;
                program.num_thread_x.full=64;program.num_thread_y.full=program.num_thread_z.full=1;
                program.dim_x=u32(layout.guest_bytes/(128*layout.bytes_per_pixel));program.dim_y=program.dim_z=1;
                program.settings.num_user_regs=8;program.settings.num_vgprs=3;program.settings.num_sgprs=3;
                auto source=target;source.base_address=base+8192;source.num_records=4;
                auto destination=target;
                if(packed16){destination.data_format=2;destination.stride=2;
                    destination.num_records=u32(layout.guest_bytes/2);}
                if(data_format==13){destination.data_format=13;destination.stride=12;
                    destination.num_records=u32(layout.guest_bytes/12);}
                memcpy(program.user_data.data(),&source,sizeof(source));
                memcpy(program.user_data.data()+4,&destination,sizeof(destination));rasterizer.DispatchDirect();
            };
            if(linear_download){
                EmulatorSettings.SetReadbackLinearImagesEnabled(true);
                bind_vertex(both_vertex,16);bind_pixel(red_pixel);rasterizer.Draw(false);
                if(!rasterizer.IsGpuBufferModified(base+16384,layout.guest_bytes))
                    throw std::runtime_error("Actual GCN buffer writer did not mark its GPU source domain");
                read_buffer(0);check_color(255);draw_image(load_pixel,image_quarter_red);draw_raw_float(0);
                if(rasterizer.IsGpuBufferModified(base+16384,layout.guest_bytes))
                    throw std::runtime_error("CPU buffer download did not retire its GPU-modified source domain");
                const auto downloaded=[](){auto& hub=Diagnostics::Hub::Get();std::lock_guard lock(hub.mutex);
                    for(const auto& counter:hub.counters)if(counter.name&&
                        std::string_view(counter.name)=="image-download.linear-independent")return counter.value;
                    return u64{};};
                const auto expect_compact=[&](u32 word){
                    for(u32 layer=0;layer<desc.layers;++layer)for(u32 y=0;y<desc.height;++y)
                    for(u32 x=0;x<desc.width;++x)
                        memcpy(expected.data()+16384+((uint64_t(layer)*desc.height+y)*desc.pitch+x)*4,&word,4);
                };
                const float half_clear[]{0.5f,0.f,0.f,0.f};const auto first=downloaded();
                clear_image(half_clear);rasterizer.OnFence();expect_compact(0x3f000000);
                if(downloaded()!=first+1||memcmp(backing,expected.data(),65536))
                    throw std::runtime_error("Fence compact download or full physical padding mismatch");
                draw_raw_float(0);draw_image(load_pixel,128);draw_image(next_pixel,128);
                rasterizer.OnFence();
                if(downloaded()!=first+1)throw std::runtime_error("Empty fence repeated a stale image download");
                clear_image(half_clear);rasterizer.OnFence();
                if(downloaded()!=first+1)throw std::runtime_error("HLE clear alone invented a storage-binding download");
                bind_vertex(image_vertex,12);memcpy(regs.vs_program.user_data.data()+4,&image,sizeof(image));
                bind_pixel(red_pixel);rasterizer.Draw(false);check_color(255);
                const float quarter_clear[]{0.25f,0.f,0.f,0.f};
                clear_image(quarter_clear);rasterizer.OnSubmit();rasterizer.Finish();expect_compact(0x3e800000);
                if(downloaded()!=first+2||memcmp(backing,expected.data(),65536))
                    throw std::runtime_error("Submission compact download or full physical padding mismatch");
                draw_raw_float(0);draw_image(load_pixel,64);
                if(!selected_mip&&layers==1&&initial==0.f){
                    // The preceding raw arena still contains zero, while the
                    // legacy image download placed quarter in physical CPU.
                    // Retiring the image for the next buffer writer must stage
                    // the read-only prewrite image from that CPU domain.
                    bind_vertex(buffer_vertex,8);bind_pixel(load_pixel);rasterizer.Draw(false);check_color(64);
                    draw_raw_float(0);
                    for(u32 mip=0;mip<desc.levels;++mip)
                    for(u32 y=0;y<std::max(desc.height>>mip,1u);++y)
                    for(u32 x=0;x<std::max(desc.width>>mip,1u);++x){
                        const u32 zero{};memcpy(expected.data()+16384+GuestPixelOffset(desc,layout,mip,0,x,y),&zero,4);
                    }
                    read_buffer(0);check_color(0);
                    EmulatorSettings.SetReadbackLinearImagesEnabled(false);continue;
                }
                // A real store, even identical to the downloaded CPU value,
                // finally transfers that CPU page into the old resident arena.
                *reinterpret_cast<volatile u32*>(memory+16384)=0x3e800000;
                draw_raw_float(selected_mip||layers>1?0:64);
                EmulatorSettings.SetReadbackLinearImagesEnabled(false);
                continue;
            }
            if(((data_format==4||data_format==13)&&number_format==7)||data_format==10||packed10||(packed16&&volume)){
                // Prime an ordinary read-only cache image and a raw resident
                // buffer before the first image writer. HLE clear must promote
                // only the image; CPU/raw arena reads keep their prior bits.
                const float linear=srgb?(initial_value<=0.04045f?initial_value/12.92f:
                    std::pow((initial_value+0.055f)/1.055f,2.4f)):initial_value;
                draw_image(load_pixel,u8(std::clamp(linear,0.f,1.f)*255.f+0.5f));
                const bool byte_raw=data_format==10||packed10||packed16;
                const u8 raw_red=byte_raw?u8(initial_bits):u8(std::clamp(initial_value,0.f,1.f)*255.f+0.5f);
                if(byte_raw)draw_raw_byte(raw_red);else draw_raw_float(raw_red);
                const bool integer_clear=small_integer;
                const float value[]{integer_clear?std::bit_cast<float>(15u):0.5f,0.f,0.f,0.f};
                clear_image(value);check_color(raw_red);read_buffer(initial_bits);
                const u8 red=integer_clear?u8(15.f/numeric_scale*255.f+0.5f):srgb?128:image_half_red;
                draw_image(load_pixel,red);draw_image(next_pixel,red);read_buffer(initial_bits);
                if(byte_raw)draw_raw_byte(raw_red);else draw_raw_float(raw_red);
                const float second[]{integer_clear?std::bit_cast<float>(7u):0.25f,0.f,0.f,0.f};
                clear_image(second);check_color(raw_red);read_buffer(initial_bits);
                const u8 second_red=integer_clear?u8(7.f/numeric_scale*255.f+0.5f):srgb?64:image_quarter_red;
                draw_image(load_pixel,second_red);draw_image(next_pixel,second_red);read_buffer(initial_bits);
                if(byte_raw)draw_raw_byte(raw_red);else draw_raw_float(raw_red);
                clear_image(value);check_color(raw_red);draw_image(load_pixel,red);read_buffer(initial_bits);
            }
            bind_vertex(both_vertex,16);bind_pixel(red_pixel);rasterizer.Draw(false);
            read_buffer(0);check_color(255);draw_image(load_pixel,image_quarter_red);
            if((data_format==4&&number_format==7)||data_format==10||data_format==13||packed16||packed10){
                // Original image-clear recognition happens before buffer
                // binding. Zero must clear this cached writable image while
                // leaving its separate physical/resident buffer version alone.
                const float zero[4]{};clear_image(zero);check_color(image_quarter_red);
                draw_image(load_pixel,0);draw_image(next_pixel,0);read_buffer(0);draw_raw_float(0);
                const bool integer_clear=(data_format==10||packed10)&&(number_format==4||number_format==5);
                const float nonzero_clear[]{integer_clear?std::bit_cast<float>(15u):0.5f,0.f,0.f,0.f};
                clear_image(nonzero_clear);check_color(0);
                const u8 clear_red=integer_clear?u8(15.f/numeric_scale*255.f+0.5f):srgb?128:image_half_red;
                draw_image(load_pixel,clear_red);draw_image(next_pixel,clear_red);read_buffer(0);draw_raw_float(0);
                // The next exact-base buffer writer retires the cleared image
                // version and recreates the preceding quarter fixture state.
                bind_vertex(both_vertex,16);bind_pixel(red_pixel);rasterizer.Draw(false);
                read_buffer(0);check_color(255);draw_image(load_pixel,image_quarter_red);
            }
            // After CPU reconciliation the original streams small formatted
            // buffers from physical CPU bytes, leaving the image version intact.
            draw_formatted(layout.guest_bytes>16384?buffer_quarter_red:0);draw_image(load_pixel,image_quarter_red);
            draw_formatted(short_bytes>16384?buffer_quarter_red:0,short_bytes);draw_image(load_pixel,image_quarter_red);
            // A larger clean sharp synchronizes the unique full image even
            // when the image itself is below the 16-KiB streaming threshold.
            // Physical trailing bytes remain in the independent oracle.
            draw_formatted(buffer_quarter_red,extended_bytes);draw_image(load_pixel,image_quarter_red);
            const float initial_linear=srgb?(initial_value<=0.04045f?initial_value/12.92f:
                std::pow((initial_value+0.055f)/1.055f,2.4f)):initial_value;
            draw_image(next_pixel,u8(std::clamp(initial_linear,0.f,1.f)*255.f+0.5f));
            // Real protected guest accesses distinguish read and write faults;
            // the physical alias above observes bytes without resolving images.
            *reinterpret_cast<volatile u32*>(memory+16384+store_offset)=one_word;
            memcpy(expected.data()+16384+store_offset,&one_word,4);draw_image(load_pixel,255);
            bind_vertex(both_vertex,16);bind_pixel(red_pixel);rasterizer.Draw(false);
            check_color(255);
            // Before a CPU read, GPU-modified formatted buffers copy the image
            // into the resident buffer arena, including all other tiled texels.
            draw_formatted(buffer_quarter_red);read_buffer(quarter_word,true);check_color(buffer_quarter_red);draw_image(load_pixel,image_quarter_red);
            bind_vertex(both_vertex,16);bind_pixel(red_pixel);rasterizer.Draw(false);check_color(255);
            draw_formatted(buffer_quarter_red,short_bytes);read_buffer(quarter_word,true);check_color(buffer_quarter_red);draw_image(load_pixel,image_quarter_red);
            bind_vertex(both_vertex,16);bind_pixel(red_pixel);rasterizer.Draw(false);check_color(255);
            draw_formatted(buffer_quarter_red,extended_bytes);read_buffer(quarter_word,true);check_color(buffer_quarter_red);draw_image(load_pixel,image_quarter_red);
            bind_vertex(both_vertex,16);bind_pixel(red_pixel);rasterizer.Draw(false);
            read_buffer(0);check_color(255);draw_image(load_pixel,image_quarter_red);
            // Pinned GPU invalidation considers the original sharp base only.
            // A DWORD-shifted writer keeps this preceding image version even
            // though it updates bytes inside the same protected buffer arena.
            bind_vertex(buffer_vertex,8);bind_pixel(red_pixel);
            auto shifted_target=target;shifted_target.base_address+=4;--shifted_target.num_records;
            memcpy(regs.vs_program.user_data.data()+4,&shifted_target,sizeof(shifted_target));
            rasterizer.Draw(false);
            if(*reinterpret_cast<volatile u32*>(memory+16384+store_offset+4)!=0)
                throw std::runtime_error("Shifted buffer write did not reconcile its own logical result");
            const u32 shifted_zero=0;memcpy(expected.data()+16384+store_offset+4,&shifted_zero,4);
            check_color(255);draw_image(load_pixel,image_quarter_red);
            // Make the native arena's quarter image bytes differ from its
            // clean physical CPU buffer. Only the later exact BYTE/SHORT sharp
            // ranges may be committed, not their unused aligned DWORD bytes.
            draw_formatted(buffer_quarter_red,extended_bytes);draw_image(load_pixel,image_quarter_red);
            for(u32 store_bytes:{1u,2u}){
                bind_vertex(store_bytes==1?byte_vertex:half_vertex,8);bind_pixel(red_pixel);
                auto unaligned_target=target;unaligned_target.base_address+=store_offset+store_bytes;unaligned_target.stride=0;
                unaligned_target.num_records=store_bytes;
                memcpy(regs.vs_program.user_data.data()+4,&unaligned_target,sizeof(unaligned_target));
                rasterizer.Draw(false);
                for(u32 byte=0;byte<store_bytes;++byte){
                    const u8 value=byte==0?0x35:0x7a;
                    if(*reinterpret_cast<volatile u8*>(memory+16384+store_offset+store_bytes+byte)!=value)
                        throw std::runtime_error("Unaligned byte/halfword buffer store lost its exact logical result");
                    expected[16384+store_offset+store_bytes+byte]=std::byte{value};
                }
                check_color(255);draw_image(load_pixel,image_quarter_red);
            }
            if(data_format==1||data_format==3||data_format==10){
                // CPU red remains zero while the resident formatted arena
                // retains quarter red. An actual store of that same zero
                // invalidates the CPU page despite identical snapshot bytes.
                draw_raw_byte(u8(quarter_word));
                auto* cpu_red=reinterpret_cast<volatile u8*>(memory+16384+store_offset);
                if(*cpu_red!=0)throw std::runtime_error("Same-value CPU store precondition changed");
                *cpu_red=0;
                draw_raw_byte(0);
                // Refresh the cached image from its CPU domain before a new
                // formatted read can synchronize that image into the arena.
                draw_image(load_pixel,0);draw_formatted(0,extended_bytes);
            }
            bind_vertex(buffer_vertex,8);bind_pixel(red_pixel);rasterizer.Draw(false);
            read_buffer(0);check_color(255);draw_image(load_pixel,0);
            if(selected_mip||selected_layer){
                // Distinct image mips/layers now diverge from the buffer arena.
                // A formatted read must copy every subresource, even though
                // the pixel shader consumes only one selected mip/layer.
                bind_vertex(both_vertex,16);bind_pixel(red_pixel);rasterizer.Draw(false);check_color(255);
                bind_vertex(image_vertex,12);
                auto first_mip=image;first_mip.base_level=0;
                memcpy(regs.vs_program.user_data.data()+4,&first_mip,sizeof(first_mip));
                bind_pixel(red_pixel);rasterizer.Draw(false);check_color(255);
                draw_formatted(buffer_quarter_red);read_buffer(quarter_word,true);
                memcpy(expected.data()+16384+GuestPixelOffset(desc,layout,0,0,0,0),&half_word,
                    std::min(layout.bytes_per_pixel,4u));
                if(layout.bytes_per_pixel>4)
                    memset(expected.data()+16384+GuestPixelOffset(desc,layout,0,0,0,0)+4,0,layout.bytes_per_pixel-4);
                check_color(buffer_quarter_red);
                const auto selected_image=image;image.base_level=0;
                draw_image(first_pixel,image_half_red);image=selected_image;
            }
        }
        }
    }else if(readonly_depth){
        // Vertex buffer stores execute before attachment reads. Every vertex
        // writes the same zero DWORD, so ordering among vertices is irrelevant.
        // The depth/stencil tests must still consume the preceding contents.
        vertex_words.resize(vertex_words.size()-3);
        if(image_writer)vertex_words.insert(vertex_words.end(),{0x7e020280,0x7e040280,0x7e100280,
            0xf0200100,0x00010801});
        else vertex_words.insert(vertex_words.end(),{0x7e020280,0xe0700000,0x80010100});
        vertex_words.insert(vertex_words.end(),{0xf80008cf,0x07060504,0xbf810000});
        initialize(vertex,vertex_words,image_writer?0x4154544453495601ULL:0x4154544453425601ULL);
        initialize(pixel,{0xBEEB03FF,15,0x7e0002ff,0x3f800000,0x7e020280,0x7e040280,
            0x7e0602ff,0x3f800000,0xf800000f,0x03020100,0xbf810000},0x4154544453425001ULL);
        for(bool z16:{false,true})for(uint32_t samples:{0u,2u})for(bool stencil:{false,true})
        for(bool stencil_writer:{false,true})for(bool full_buffer:{false,true})for(bool htile:{false,true}){
            if(planned_depth&&(z16||samples||!stencil||stencil_writer))continue;
            if(stencil_writer&&!stencil)continue;
            if(mixed_planes&&!stencil)continue;
            if(image_writer&&(z16||samples||stencil_writer||full_buffer))continue;
            rasterizer.ReadMemory(base,65536);rasterizer.InvalidateMemory(base,65536);
            auto& regs=gpu.regs;regs={};regs.stage_enable.raw=AmdGpu::ShaderStageEnable::Vs;
            regs.primitive_type=AmdGpu::PrimitiveType::TriangleList;regs.num_indices=3;
            regs.vs_program.address=reinterpret_cast<u64>(vertex.data())>>8;
            regs.vs_program.settings.num_user_regs=image_writer?12:8;
            regs.vs_program.settings.num_vgprs=image_writer?2:1;
            regs.ps_program.address=reinterpret_cast<u64>(pixel.data())>>8;
            regs.color_export_format.raw=u32(AmdGpu::ShaderExportFormat::ABGR_32);
            regs.color_control.mode=AmdGpu::ColorControl::OperationMode::Normal;
            regs.color_control.rop3=AmdGpu::ColorControl::LogicOp::Copy;
            regs.color_target_mask.raw=regs.color_shader_mask.raw=15;
            regs.clipper_control.clip_space=AmdGpu::ClipSpace::ZeroToW;
            auto& vp=regs.viewport_control;
            vp.xscale_enable=vp.yscale_enable=vp.xoffset_enable=vp.yoffset_enable=vp.zscale_enable=1;
            regs.viewports[0].xscale=4;regs.viewports[0].yscale=-4;
            regs.viewports[0].xoffset=regs.viewports[0].yoffset=4;regs.viewports[0].zscale=1;
            regs.screen_scissor.bottom_right_x=regs.window_scissor.bottom_right_x=regs.generic_scissor.bottom_right_x=8;
            regs.screen_scissor.bottom_right_y=regs.window_scissor.bottom_right_y=regs.generic_scissor.bottom_right_y=8;
            auto& cb=regs.color_buffers[0];cb.base_address=u32((base+4096)>>8);
            cb.info.format=10;cb.info.number_type=0;cb.attrib.tile_mode_index=AmdGpu::TileMode::Thin1DThin;
            cb.attrib.num_fragments_log2=samples;cb.pitch.tile_max=cb.slice.tile_max=1;
            gpu.last_cb_extent[0]={8,8};gpu.last_db_extent=image_writer?AmdGpu::CbDbExtent{32,32}:AmdGpu::CbDbExtent{8,8};
            auto& db=regs.depth_buffer;db.z_read_base=db.z_write_base=u32((base+8192)>>8);
            db.z_info.format=z16?AmdGpu::DepthBuffer::ZFormat::Z16:AmdGpu::DepthBuffer::ZFormat::Z32Float;
            db.z_info.tile_mode_index=5;db.z_info.num_samples=samples;db.z_info.tile_surface_enable=htile;
            db.depth_size.pitch_tile_max=db.depth_size.height_tile_max=db.depth_slice.tile_max=1;
            if(planned_depth)db.depth_size.height_tile_max=0; // 16 x 8 physical pixels in two tiles.
            if(image_writer){db.depth_size.pitch_tile_max=db.depth_size.height_tile_max=3;db.depth_slice.tile_max=15;}
            if(stencil){db.stencil_read_base=db.stencil_write_base=u32((base+16384)>>8);
                db.stencil_info.format=AmdGpu::DepthBuffer::StencilFormat::Stencil8;}
            regs.depth_htile_data_base.address=u32((base+32768)>>8);
            regs.depth_control.depth_enable=1;regs.depth_control.depth_func=AmdGpu::CompareFunc::Less;
            regs.depth_control.stencil_enable=stencil;
            regs.depth_control.stencil_ref_func=regs.depth_control.stencil_bf_func=AmdGpu::CompareFunc::Equal;
            regs.stencil_ref_front.stencil_mask=regs.stencil_ref_front.stencil_write_mask=255;
            regs.stencil_ref_back=regs.stencil_ref_front;
            AmdGpu::Buffer input{};input.base_address=base;input.stride=32;input.num_records=3;
            input.data_format=4;input.num_format=4;input.dst_sel_x=4;input.dst_sel_y=5;input.dst_sel_z=6;input.dst_sel_w=7;
            memcpy(regs.vs_program.user_data.data(),&input,sizeof(input));
            const auto color=DescribeGuestColorBuffer(cb,gpu.last_cb_extent[0]);const auto cl=LayoutGuestTexture(color);
            const auto depth=DescribeGuestDepthBuffer(db,regs.depth_view,gpu.last_db_extent);const auto dl=LayoutGuestTexture(depth);
            const auto sd=stencil?DescribeGuestStencil(depth):GuestTextureDesc{};
            const auto sl=stencil?LayoutGuestTexture(sd):GuestTextureLayout{};
            if(dl.guest_bytes>8192||(stencil&&sl.guest_bytes>8192)||cl.guest_bytes>4096)
                throw std::runtime_error("Read-only depth fixture allocations overlap");
            AmdGpu::Buffer target{};target.base_address=base+49152;target.num_records=4;
            AmdGpu::Image image_target{};image_target.width=image_target.height=image_target.pitch=31;
            image_target.type=u64(AmdGpu::ImageType::Color2D);image_target.data_format=4;image_target.num_format=7;
            image_target.tiling_index=5;image_target.dst_sel_x=4;image_target.dst_sel_y=5;image_target.dst_sel_z=6;image_target.dst_sel_w=7;
            const auto bind_writer=[&]{
                if(image_writer){image_target.base_address=target.base_address>>8;
                    memcpy(regs.vs_program.user_data.data()+4,&image_target,sizeof(image_target));}
                else memcpy(regs.vs_program.user_data.data()+4,&target,sizeof(target));
            };
            bind_writer();
            if(htile){
                // Consume any real first-use HTILE clear with a disjoint writer.
                // A later snapshot must preserve the consumed mask after edits.
                memset(memory,0,65536);regs.depth_clear=0.75f;
                regs.depth_control.depth_func=AmdGpu::CompareFunc::Never;
                rasterizer.Draw(false);rasterizer.ReadMemory(base,65536);rasterizer.Finish();
                regs.depth_control.depth_func=AmdGpu::CompareFunc::Less;
            }
            for(unsigned warm=0;warm<(planned_depth?1u:3u);++warm){
                rasterizer.ReadMemory(base,65536);rasterizer.InvalidateMemory(base,65536);memset(memory,0xee,65536);
                regs.depth_view.z_read_only=regs.depth_view.stencil_read_only=warm!=1;
                regs.depth_control.depth_write_enable=mixed_planes&&stencil_writer;
                regs.stencil_control.stencil_zpass_front=regs.stencil_control.stencil_zpass_back=
                    mixed_planes&&!stencil_writer?AmdGpu::StencilFunc::AddClamp:AmdGpu::StencilFunc::Keep;
                const float xy[3][2]{{-1,-1},{3,-1},{-1,3}};
                for(unsigned i=0;i<3;++i){const float position[]{xy[i][0],xy[i][1],0.5f,1.f};memcpy(memory+i*32,position,sizeof(position));}
                if(planned_depth){
                    // An actual old native D image survives later CPU dirtiness.
                    // Adding S must recreate it at DB binding, after PS buffers.
                    const auto saved_vs=regs.vs_program;const auto saved_db=db;
                    const auto saved_control=regs.depth_control;
                    const auto saved_render=regs.depth_render_control;
                    const auto saved_clear=regs.depth_clear;
                    regs.vs_program.address=reinterpret_cast<u64>(plain_vertex.data())>>8;
                    regs.vs_program.settings.num_user_regs=4;
                    db.stencil_info.format=AmdGpu::DepthBuffer::StencilFormat::Invalid;
                    regs.depth_control.stencil_enable=0;
                    regs.depth_control.depth_func=AmdGpu::CompareFunc::Never;
                    regs.depth_control.depth_write_enable=1;
                    regs.depth_render_control.depth_clear_enable=1;regs.depth_clear=0.875f;
                    rasterizer.Draw(false);rasterizer.Finish();
                    regs.vs_program=saved_vs;db=saved_db;
                    regs.depth_control=saved_control;regs.depth_render_control=saved_render;
                    regs.depth_clear=saved_clear;
                }
                const float initial_depth=planned_depth?0.25f:warm==1?0.25f:warm==2?0.875f:0.75f;
                const u16 packed_depth=u16(initial_depth*65535.f+0.5f);const u8 initial_stencil=u8(37+warm);
                regs.stencil_ref_front.stencil_test_val=regs.stencil_ref_back.stencil_test_val=initial_stencil;
                for(u32 sample=0;sample<depth.samples;++sample)for(u32 y=0;y<depth.height;++y)for(u32 x=0;x<depth.width;++x){
                    const auto offset=GuestPixelOffset(depth,dl,0,0,x,y,sample);
                    memcpy(memory+8192+offset,z16?static_cast<const void*>(&packed_depth):static_cast<const void*>(&initial_depth),z16?2:4);
                    if(stencil)memory[16384+GuestPixelOffset(sd,sl,0,0,x,y,sample)]=std::byte(initial_stencil);
                }
                const auto& written=stencil_writer?sd:depth;const auto& written_layout=stencil_writer?sl:dl;
                const auto offset=full_buffer?0:GuestPixelOffset(written,written_layout,0,0,0,0,0);
                // Keep observed DWORD0 distinct from the VS writer. Exact-base
                // GPU invalidation must not mask premature image publication.
                target.base_address=written.address+(planned_depth?4:offset);
                target.num_records=full_buffer?u32(written_layout.guest_bytes):4;
                bind_writer();
                if(planned_depth){
                    regs.ps_program.address=reinterpret_cast<u64>(metadata_pixel.data())>>8;
                    regs.ps_program.settings.num_user_regs=4;regs.ps_program.settings.num_vgprs=1;
                    AmdGpu::Buffer formatted{};formatted.base_address=depth.address;
                    formatted.stride=4;formatted.num_records=32768/4;
                    formatted.data_format=4;formatted.num_format=7;
                    formatted.dst_sel_x=4;formatted.dst_sel_y=5;formatted.dst_sel_z=6;formatted.dst_sel_w=7;
                    memcpy(regs.ps_program.user_data.data(),&formatted,sizeof(formatted));
                }
                const auto image_desc=image_writer?DescribeGuestTexture(image_target):GuestTextureDesc{};
                const auto image_layout=image_writer?LayoutGuestTexture(image_desc):GuestTextureLayout{};
                const auto store_address=image_writer?image_desc.address+GuestPixelOffset(image_desc,image_layout,0,0,0,0):target.base_address;
                std::vector<std::byte> expected(memory,memory+65536);memset(expected.data()+store_address-base,0,4);
                if((planned_depth?0.875f:initial_depth)>0.5f){const u32 red=planned_depth?0xff000040:0xff0000ff;
                    for(u32 sample=0;sample<color.samples;++sample)for(u32 y=0;y<8;++y)for(u32 x=0;x<8;++x){
                        memcpy(expected.data()+4096+GuestPixelOffset(color,cl,0,0,x,y,sample),&red,4);
                        if(mixed_planes&&stencil_writer){const float z=0.5f;const u16 packed=32768;
                            memcpy(expected.data()+8192+GuestPixelOffset(depth,dl,0,0,x,y,sample),
                                z16?static_cast<const void*>(&packed):static_cast<const void*>(&z),z16?2:4);
                        }
                        if(mixed_planes&&!stencil_writer)
                            expected[16384+GuestPixelOffset(sd,sl,0,0,x,y,sample)]=std::byte(initial_stencil+1);
                    }
                }
                rasterizer.Draw(false);rasterizer.ReadMemory(base,65536);rasterizer.Finish();
                if(memcmp(memory,expected.data(),65536)){
                    for(size_t i=0;i<expected.size();++i)if(memory[i]!=expected[i]){
                        fprintf(stderr,"First mismatch offset=%zu actual=%02x expected=%02x planned=%d full=%d htile=%d\n",
                            i,std::to_integer<unsigned>(memory[i]),std::to_integer<unsigned>(expected[i]),planned_depth,full_buffer,htile);break;}
                    throw std::runtime_error("GCN buffer writer/read-only depth-stencil prewrite contents or padding mismatch");
                }
                if(mode=="readonly-depth-buffer-owned"&&htile&&!stencil&&!z16&&!samples&&!full_buffer){
                    const auto saved_vertex=regs.vs_program;const auto saved_pixel=regs.ps_program;
                    const auto saved_target=target;const auto saved_depth_func=regs.depth_control.depth_func;
                    regs.depth_control.depth_func=AmdGpu::CompareFunc::Always;
                    std::vector<std::byte> meta_expected(backing,backing+65536);
                    const auto check_meta_frame=[&](u8 red){
                        const u32 rgba=u32(red)|0xff000000;
                        for(u32 y=0;y<8;++y)for(u32 x=0;x<8;++x)
                            memcpy(meta_expected.data()+4096+GuestPixelOffset(color,cl,0,0,x,y),&rgba,4);
                        rasterizer.Finish();
                        if(memcmp(backing,meta_expected.data(),65536))
                            throw std::runtime_error("Real GCN HTILE formatted-buffer contents, CPU visibility or padding mismatch");
                    };
                    // HTILE is registered by the actual preceding depth draw.
                    // A raw GPU writer supplies the GPU-modified arena condition.
                    target.base_address=base+32768;target.num_records=4096;bind_writer();
                    rasterizer.Draw(false);check_meta_frame(255);
                    regs.vs_program.address=reinterpret_cast<u64>(plain_vertex.data())>>8;
                    regs.vs_program.settings.num_user_regs=4;regs.vs_program.settings.num_vgprs=1;
                    regs.ps_program.address=reinterpret_cast<u64>(metadata_pixel.data())>>8;
                    regs.ps_program.settings.num_user_regs=4;regs.ps_program.settings.num_vgprs=1;
                    AmdGpu::Buffer formatted{};formatted.base_address=target.base_address;
                    formatted.stride=4;formatted.num_records=1024;formatted.data_format=10;formatted.num_format=0;
                    formatted.dst_sel_x=4;formatted.dst_sel_y=5;formatted.dst_sel_z=6;formatted.dst_sel_w=7;
                    memcpy(regs.ps_program.user_data.data(),&formatted,sizeof(formatted));
                    rasterizer.Draw(false);check_meta_frame(15);
                    if(*reinterpret_cast<volatile u32*>(memory+32768)!=0xf)
                        throw std::runtime_error("GPU HTILE synthesis did not enter the buffer's CPU readback domain");
                    const u32 expanded=0xf;
                    for(u32 i=0;i<1024;++i)memcpy(meta_expected.data()+32768+i*4,&expanded,4);
                    check_meta_frame(15);
                    // With GPU dirtiness cleared, the same small formatted
                    // buffer streams a new CPU word instead of synthesizing it.
                    *reinterpret_cast<volatile u32*>(memory+32768)=0;
                    memset(meta_expected.data()+32768,0,4);
                    rasterizer.Draw(false);check_meta_frame(0);
                    regs.vs_program=saved_vertex;regs.ps_program=saved_pixel;target=saved_target;
                    regs.depth_control.depth_func=saved_depth_func;
                }
                if(planned_depth)continue;
                const auto reject=[&]{const std::vector<std::byte> before(memory,memory+65536);bool rejected=false;
                    try{rasterizer.Draw(false);}catch(const std::runtime_error&){rejected=true;}
                    rasterizer.Finish();rasterizer.ReadMemory(base,65536);
                    if(!rejected||memcmp(memory,before.data(),65536))
                        throw std::runtime_error("Competing buffer/attachment writer was not rejected before guest writes");
                };
                if(stencil_writer){
                    regs.stencil_control.stencil_zpass_front=regs.stencil_control.stencil_zpass_back=AmdGpu::StencilFunc::AddClamp;
                    reject();regs.stencil_control.stencil_zpass_front=regs.stencil_control.stencil_zpass_back=AmdGpu::StencilFunc::Keep;
                    regs.depth_render_control.stencil_clear_enable=1;reject();regs.depth_render_control.stencil_clear_enable=0;
                }else{
                    regs.depth_control.depth_write_enable=1;reject();regs.depth_control.depth_write_enable=0;
                    regs.depth_render_control.depth_clear_enable=1;regs.depth_control.depth_write_enable=1;
                    reject();regs.depth_render_control.depth_clear_enable=0;regs.depth_control.depth_write_enable=0;
                }
            }
            if(htile&&!stencil_writer&&!planned_depth){
                // Rebinding a fresh metadata allocation installs the entry's
                // initial clear mask. It must not escape the two-writer guard.
                regs.depth_htile_data_base.address+=1;
                const std::vector<std::byte> before(memory,memory+65536);bool rejected=false;
                try{rasterizer.Draw(false);}catch(const std::runtime_error&){rejected=true;}
                rasterizer.Finish();rasterizer.ReadMemory(base,65536);
                if(!rejected||memcmp(memory,before.data(),65536))
                    throw std::runtime_error("HTILE clear alias was not rejected before guest writes");
            }
        }
    }else for(uint32_t log2_samples:{0u,2u})for(const auto order:{std::array<uint32_t,3>{0,1,2},std::array<uint32_t,3>{2,0,1}}){
        if(linear_attachment&&log2_samples)continue;
        const u32 width=linear_attachment?56:8,height=linear_attachment?16:8;
        auto& regs=gpu.regs;regs={};regs.stage_enable.raw=AmdGpu::ShaderStageEnable::Vs;
        regs.primitive_type=AmdGpu::PrimitiveType::TriangleList;regs.num_indices=3;
        regs.vs_program.address=reinterpret_cast<u64>(vertex.data())>>8;regs.vs_program.settings.num_user_regs=4;
        regs.vs_program.settings.num_vgprs=1;regs.ps_program.address=reinterpret_cast<u64>(pixel.data())>>8;
        const auto format=u32(AmdGpu::ShaderExportFormat::ABGR_32);
        regs.color_export_format.raw=format|(format<<4)|(format<<8);
        regs.color_control.mode=AmdGpu::ColorControl::OperationMode::Normal;
        regs.color_control.rop3=AmdGpu::ColorControl::LogicOp::Copy;
        regs.color_target_mask.raw=regs.color_shader_mask.raw=0xfff;
        regs.clipper_control.clip_space=AmdGpu::ClipSpace::ZeroToW;
        auto& control=regs.viewport_control;
        control.xscale_enable=control.yscale_enable=control.xoffset_enable=control.yoffset_enable=control.zscale_enable=1;
        regs.viewports[0].xscale=float(width)/2;regs.viewports[0].yscale=-float(height)/2;
        regs.viewports[0].xoffset=float(width)/2;regs.viewports[0].yoffset=float(height)/2;regs.viewports[0].zscale=1;
        regs.screen_scissor.bottom_right_x=regs.window_scissor.bottom_right_x=regs.generic_scissor.bottom_right_x=width;
        regs.screen_scissor.bottom_right_y=regs.window_scissor.bottom_right_y=regs.generic_scissor.bottom_right_y=height;
        AmdGpu::Buffer input{};input.base_address=base;input.stride=32;input.num_records=3;input.data_format=4;input.num_format=4;
        input.dst_sel_x=4;input.dst_sel_y=5;input.dst_sel_z=6;input.dst_sel_w=7;
        memcpy(regs.vs_program.user_data.data(),&input,sizeof(input));
        std::array<GuestTextureDesc,3> descriptions;
        for(size_t i=0;i<descriptions.size();++i){auto& cb=regs.color_buffers[i];
            cb.base_address=u32((base+4096)>>8);cb.info.format=signed_color?9:10;
            cb.info.number_type=signed_color?5:0;
            cb.attrib.tile_mode_index=linear_attachment?AmdGpu::TileMode::DisplayLinearAligned:AmdGpu::TileMode::Thin1DThin;
            cb.attrib.num_fragments_log2=log2_samples;
            cb.pitch.tile_max=linear_attachment?7:1;cb.slice.tile_max=linear_attachment?15:1;
            cb.view.slice_start=cb.view.slice_max=order[i];
            gpu.last_cb_extent[i]={u16(width),u16(height)};descriptions[i]=DescribeGuestColorBuffer(cb,gpu.last_cb_extent[i]);
        }
        MergeGuestColorAttachmentLayers(descriptions);const auto layout=LayoutGuestTexture(descriptions[0]);
        for(unsigned warm=0;warm<2;++warm){
            rasterizer.ReadMemory(base,65536);rasterizer.InvalidateMemory(base,65536);memset(memory,0xee,65536);
            const float xy[3][2]{{-1,-1},{3,-1},{-1,3}};
            for(unsigned i=0;i<3;++i){const float position[]{xy[i][0],xy[i][1],0.5f,1.f};memcpy(memory+i*32,position,sizeof(position));}
            std::vector<std::byte> expected(memory,memory+65536);
            const auto pack=[](s32 r,s32 g,s32 b,s32 a){return(u32(r)&1023)|((u32(g)&1023)<<10)|((u32(b)&1023)<<20)|((u32(a)&3)<<30);};
            const u32 values[]{signed_color?pack(-512,-1,0,1):0xff0000ff,
                signed_color?pack(511,-1,-512,1):0xff00ff00,signed_color?pack(511,0,-512,-2):0xffff0000};
            for(size_t i=0;i<descriptions.size();++i)for(uint32_t sample=0;sample<descriptions[i].samples;++sample)
            for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x){
                const auto offset=GuestPixelOffset(descriptions[i],layout,0,order[i],x,y,sample);
                memcpy(expected.data()+4096+offset,&values[i],4);
            }
            if(linear_attachment){
                const Rasterizer::ShaderBufferRequest raw[]{{base+4096,layout.guest_bytes,false,false,true}};
                rasterizer.PrepareShaderBuffers(raw);
                const auto downloads=[](){auto& hub=Diagnostics::Hub::Get();std::lock_guard lock(hub.mutex);
                    for(const auto& counter:hub.counters)if(counter.name&&
                        std::string_view(counter.name)=="image-download.linear-independent")return counter.value;
                    return u64{};};
                const auto check_raw=[&](bool cpu_store){
                    auto arena=rasterizer.CapturePreparedShaderBuffer(base+4096,layout.guest_bytes);
                    auto output=Buffer(device.Get(),layout.guest_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
                    arena.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
                    rasterizer.RecordingList()->CopyBufferRegion(output.handle.Get(),0,arena.handle.Get(),0,layout.guest_bytes);
                    rasterizer.Finish();void* mapped{};D3D12_RANGE all{0,SIZE_T(layout.guest_bytes)};
                    Check(output.handle->Map(0,&all,&mapped));
                    bool equal=true;for(u64 byte=0;byte<layout.guest_bytes;++byte)
                        equal&=static_cast<const std::byte*>(mapped)[byte]==(cpu_store&&byte<4096?backing[4096+byte]:std::byte{0xee});
                    D3D12_RANGE none{};output.handle->Unmap(0,&none);
                    if(!equal)throw std::runtime_error("Legacy color download changed its independent raw arena");
                };
                const auto before=downloads();rasterizer.Draw(false);
                check_raw(false);rasterizer.OnFence();
                if(downloads()!=before+1||memcmp(backing,expected.data(),65536))
                    throw std::runtime_error("Queued linear MRT download did not preserve full allocation/padding");
                check_raw(false);rasterizer.OnFence();
                if(downloads()!=before+1)throw std::runtime_error("Empty color fence repeated a stale queued download");
                rasterizer.Draw(false);rasterizer.OnSubmit();
                if(downloads()!=before+2||memcmp(backing,expected.data(),65536))
                    throw std::runtime_error("Repeated owned MRT binding did not queue submission download");
                check_raw(false);
                const auto stored=*reinterpret_cast<volatile u32*>(memory+4096);
                *reinterpret_cast<volatile u32*>(memory+4096)=stored;
                rasterizer.PrepareShaderBuffers(raw);check_raw(true);
                continue;
            }
            rasterizer.Draw(false);rasterizer.ReadMemory(base,65536);rasterizer.Finish();
            if(memcmp(memory,expected.data(),65536))throw std::runtime_error("Actual GCN Draw shared MRT layer/sample/padding mismatch");
        }
    }
    for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T size{};Check(messages->GetMessage(i,nullptr,&size));
        std::vector<std::byte> bytes(size);auto* message=reinterpret_cast<D3D12_MESSAGE*>(bytes.data());
        Check(messages->GetMessage(i,message,&size));if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)
            throw std::runtime_error(message->pDescription);
    }
    puts(writable_word?"PASS: actual PM4 writable packed10/word alias fixture and GPU validation":readonly_numeric?"PASS: actual PM4 readonly numeric alias fixture and GPU validation":cached_depth_packed_storage?
        "PASS: eight actual GCN D16 to packed RAW32 storage transfers, full image_store, formatted R16 input before bind, raw B/native word probes, source S/HTILE and full physical padding":cached_depth_cmask_packed?
        "PASS: eight actual GCN D16/packed16/CMASK original component cases,69 UINT word probes over all64 tiles, old I versus B, source S/HTILE, original format18 remap and full physical padding":cached_depth_cmask6?
        "PASS: eight actual GCN native depth/R11G11B10/CMASK clear/draw cases, independent finite mini-float channel probes over every logical tile, old I versus B, optional S/HTILE and full physical padding":cached_depth_cmask?
        "PASS: eight actual GCN native depth/RGBA8/CMASK clear/draw cases, independent UINT word image probes over every logical CMASK tile, old I versus B, optional S/HTILE and full physical padding":cached_depth_storage?
        "PASS: eight actual GCN cached-depth storage transfers (clean/dirty, single/ordered stage mode), formatted-before-image and raw B versus native I probes, optional source S/HTILE and full physical padding":planned_depth?
        "PASS: four actual GCN late cached-depth recreations, PS current-B versus native D, interior VS writer, HTILE and full allocation/padding":ordered_formatted?
        "PASS: actual GCN descriptor-ordered formatted read/write, independent domains, image invalidation and physical padding":linear_attachment?
        "PASS: actual GCN queued linear MRT, repeated owned bindings, independent raw arena, CPU store and full padding":formatted_cmask?
        "PASS: actual GCN CMASK metadata owners, 1/2/4/8 samples, large synthesized arena versus small physical CPU stream and complete padding":separate_versions?
        "PASS: actual GCN R32 float/uint/sint independent versions and sign bits, formatted GPU retile versus CPU stream, CPU/GPU visibility and padding":readonly_depth?
        "PASS: actual GCN vertex buffer writes with read-only/mixed Z16/Z32/stencil/MSAA, prewrite tests, warm edits, HTILE and competing-writer rejection, exact allocation/padding":
        "PASS: actual Liverpool/GCN/GuestRasterizer Draw, typed signed/unsigned shared MRT layers, MSAA, eager readback, warm permutations and padding");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
