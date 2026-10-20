// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_pipeline_cache.h"
#include "video_core/renderer_d3d12/guest_compute.h"
#include "video_core/renderer_d3d12/guest_graphics_state.h"
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "video_core/cache_storage.h"
#include "common/serdes.h"
#include "core/emulator_settings.h"
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <cstdio>

using namespace NativeDX12;
static ComPtr<ID3D12Device> HardwareDevice(){
    ComPtr<ID3D12Debug> debug;Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> adapter;
        const auto hr=factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter));
        if(hr==DXGI_ERROR_NOT_FOUND)break;Check(hr);DXGI_ADAPTER_DESC1 desc{};Check(adapter->GetDesc1(&desc));
        if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
        ComPtr<ID3D12Device> device;
        if(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))return device;
    }
    throw std::runtime_error("No hardware D3D12 device");
}
static std::vector<u8> Metadata(const GuestShaderFrontend& frontend,const GuestShaderStage& stage){
    Serialization::Archive ar;frontend.SerializeStage(ar,stage);return ar.TakeOff();
}
int main(int argc,char** argv){try{
    if(argc!=3&&argc!=4)throw std::invalid_argument("Expected converter and DXC paths");
    const bool archived=argc==4&&std::string_view(argv[3])=="archive";
    const auto root=std::filesystem::current_path()/("shader-reload-fixture-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    if(!std::filesystem::create_directory(root))throw std::runtime_error("Fixture directory already exists");
    _wputenv_s(L"SHADPS4_NATIVE_SHADER_CACHE",root.c_str());
    EmulatorSettings.SetPipelineCacheEnabled(true);EmulatorSettings.SetPipelineCacheArchived(archived);
    EmulatorSettings.SetShaderCollect(false);EmulatorSettings.SetDumpShaders(false);
    auto device=HardwareDevice();ComPtr<ID3D12InfoQueue> messages;Check(device.As(&messages));
    auto* memory=static_cast<std::byte*>(VirtualAlloc(nullptr,65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!memory)throw std::runtime_error("Guest fixture allocation failed");
    struct Allocation{void* value;~Allocation(){VirtualFree(value,0,MEM_RELEASE);}} allocation{memory};
    const auto address=reinterpret_cast<u64>(memory);
    alignas(256) std::array<u32,128> reader{},writer{},patch_writer{};
    const auto initialize=[](auto& program,std::initializer_list<u32> words,u64 hash){
        std::copy(words.begin(),words.end(),program.begin());AmdGpu::BinaryInfo binary{};
        binary.signature=binary.signature_ref;binary.length=u32(words.size()*4);binary.shader_hash=hash;
        memcpy(program.data()+32,&binary,sizeof(binary));
    };
    initialize(reader,{0xBEEB03FF,15,0x7e000280,0x7e020280,0x7e040280,0x7e060280,
        0xf0000f00,0x00000400,0xf80008cf,0x07060504,0xbf810000},0x52454c4f41440001ULL);
    initialize(writer,{0xBEEB03FF,15,0x7e000280,0x7e020280,0x7e0802ff,0x3e800000,
        0x7e0a0280,0x7e0c0280,0x7e0e0280,0xf0200f00,0x00000400,0xbf810000},0x52454c4f41440002ULL);
    initialize(patch_writer,{0xBEEB03FF,15,0x7e000280,0x7e020280,0x7e0802ff,0x3f400000,
        0x7e0a0280,0x7e0c0280,0x7e0e0280,0xf0200f00,0x00000400,0xbf810000},0x52454c4f41440003ULL);
    AmdGpu::Regs regs{};regs.stage_enable.raw=AmdGpu::ShaderStageEnable::Vs;
    regs.primitive_type=AmdGpu::PrimitiveType::PointList;
    regs.vs_program.address=reinterpret_cast<u64>(reader.data())>>8;
    regs.ps_program.address=reinterpret_cast<u64>(writer.data())>>8;
    regs.vs_program.settings.num_user_regs=regs.ps_program.settings.num_user_regs=8;
    regs.vs_program.settings.num_vgprs=regs.ps_program.settings.num_vgprs=1;
    AmdGpu::Image image{};image.base_address=address>>8;image.width=image.height=image.pitch=7;
    image.type=u64(AmdGpu::ImageType::Color2D);image.data_format=14;image.num_format=7;image.tiling_index=13;
    image.dst_sel_x=4;image.dst_sel_y=5;image.dst_sel_z=6;image.dst_sel_w=7;
    memcpy(regs.vs_program.user_data.data(),&image,sizeof(image));
    memcpy(regs.ps_program.user_data.data(),&image,sizeof(image));
    Shader::Profile profile{};profile.supported_spirv=0x00010500;profile.native_coherent_images=true;
    GuestShaderFrontend frontend(profile);auto guest=frontend.BuildGraphics(regs);
    auto* pixel=guest.stages[u32(Shader::SwStage::Fragment)];const auto canonical=pixel->spirv;
    auto patch_regs=regs;patch_regs.ps_program.address=reinterpret_cast<u64>(patch_writer.data())>>8;
    const auto patch=frontend.BuildGraphics(patch_regs).stages[u32(Shader::SwStage::Fragment)]->spirv;
    const auto desc=DescribeGuestTexture(image);const auto layout=LayoutGuestTexture(desc);
    const auto offset=GuestPixelOffset(desc,layout,0,0,0,0);
    const auto state=BuildGuestGraphicsState(regs,{});
    auto alternate=state.pipeline;alternate.SampleMask=0x7fffffffU;
    Shader::Backend::DX12::SpirvConvertOptions options;options.native_storage_buffer_uavs=true;
    options.runtime_viewport_flips=true;
    GuestShaderCache shaders(std::filesystem::path(argv[1]).wstring(),options);
    Rasterizer rasterizer(device.Get());rasterizer.MapMemory(address,65536);GuestTextureCache textures(device.Get());
    const auto draw=[&](const ShaderPipeline& pipeline,float red){
        std::fill_n(memory,layout.guest_bytes,std::byte{0x5a});const float position[]{0,0,0,1};
        memcpy(memory+offset,position,sizeof(position));rasterizer.InvalidateMemory(address,layout.guest_bytes);
        std::vector<std::byte> expected(memory,memory+layout.guest_bytes);
        const float color[]{red,0,0,0};memcpy(expected.data()+offset,color,sizeof(color));
        const auto* vertex=guest.stages[u32(Shader::SwStage::Vertex)];
        const GuestResourceStage resources[]{{&pixel->info,pixel->bindings_start},{&vertex->info,vertex->bindings_start}};
        Shader::PushData push{};push.xscale=push.yscale=1;
        GraphicsRuntime runtime{};runtime.viewport_width=runtime.viewport_height=8;
        const D3D12_VIEWPORT viewport{0,0,8,8,0,1};const D3D12_RECT scissor{0,0,8,8};const float blend[]{1,1,1,1};
        ExecuteGuestGraphicsResources(device.Get(),rasterizer,pipeline,resources,push,runtime,
            [&](ResourceBindings& bindings){rasterizer.ExecuteGraphics(pipeline,bindings,state,{},nullptr,
                std::span(&viewport,1),std::span(&scissor,1),blend,{1U},{},nullptr,false);},textures);
        rasterizer.Finish();
        if(memcmp(memory,expected.data(),expected.size()))throw std::runtime_error("Reload draw contents or padding mismatch");
    };
    std::vector<u8> saved;
    {
        GuestPipelineCache pipelines(device.Get(),std::filesystem::path(argv[2]).wstring(),frontend,shaders);
        if(!pipelines.SupportsRootGraphicsRuntime(guest)||!pipelines.SupportsRootGraphicsRuntime(guest))
            throw std::runtime_error("Guest indirect runtime admission rejected the reflected runtime CBV");
        const auto* first=&pipelines.Graphics(guest,state.pipeline);
        const auto* second=&pipelines.Graphics(guest,alternate);draw(*first,0.25f);draw(*second,0.25f);
        const auto* first_root=&pipelines.Graphics(guest,state.pipeline,false,{},true);
        const auto* second_root=&pipelines.Graphics(guest,alternate,false,{},true);
        if(first_root==first||second_root==second||pipelines.GraphicsCount()!=4||
           first_root->Layout().GraphicsRuntimeParameter()==UINT_MAX||first->Layout().GraphicsRuntimeParameter()!=UINT_MAX||
           &pipelines.Graphics(guest,state.pipeline,false,{},true)!=first_root)
            throw std::runtime_error("Root runtime and descriptor-table pipeline profiles collided");
        draw(*first_root,0.25f);draw(*second_root,0.25f);
        const auto old_id=pixel->id;const auto replacement=pipelines.ReplaceShader(old_id,patch,true);
        if(!replacement||*replacement==old_id||frontend.FindStage(old_id)||frontend.FindStage(*replacement)!=pixel||
           pixel->spirv!=canonical||pixel->patch_spirv!=patch||!pixel->is_patched)
            throw std::runtime_error("Replacement did not retain stable stage/canonical words");
        const auto* patched_first=&pipelines.Graphics(guest,state.pipeline);
        const auto* patched_second=&pipelines.Graphics(guest,alternate);
        const auto* patched_first_root=&pipelines.Graphics(guest,state.pipeline,false,{},true);
        const auto* patched_second_root=&pipelines.Graphics(guest,alternate,false,{},true);
        if(patched_first==first||patched_second==second)throw std::runtime_error("An affected PSO was not replaced");
        draw(*patched_first,0.75f);draw(*patched_second,0.75f);
        if(patched_first_root==first_root||patched_second_root==second_root||pipelines.GraphicsCount()!=4||
           patched_first_root->Layout().GraphicsRuntimeParameter()==UINT_MAX)
            throw std::runtime_error("Shader replacement lost its root runtime pipeline profile");
        draw(*patched_first_root,0.75f);draw(*patched_second_root,0.75f);
        auto invalid=patch;
        for(size_t at=5;at<invalid.size();at+=invalid[at]>>16){
            if((invalid[at]&0xffffU)==15){invalid.erase(invalid.begin()+at,invalid.begin()+at+(invalid[at]>>16));break;}
        }
        bool rejected=false;try{pipelines.ReplaceShader(pixel->id,invalid,true);}catch(const std::exception&){rejected=true;}
        if(!rejected||pixel->id!=*replacement||pixel->patch_spirv!=patch||
           &pipelines.Graphics(guest,state.pipeline)!=patched_first||&pipelines.Graphics(guest,alternate)!=patched_second||
           &pipelines.Graphics(guest,state.pipeline,false,{},true)!=patched_first_root||
           &pipelines.Graphics(guest,alternate,false,{},true)!=patched_second_root)
            throw std::runtime_error("Failed compiler replacement changed stage or cached PSOs");
        draw(*patched_first,0.75f);draw(*patched_second,0.75f);
        if(!pipelines.ReplaceShader(pixel->id,canonical,false)||pixel->is_patched)
            throw std::runtime_error("Disabling the patch failed");
        draw(pipelines.Graphics(guest,state.pipeline),0.25f);draw(pipelines.Graphics(guest,alternate),0.25f);
        draw(pipelines.Graphics(guest,state.pipeline,false,{},true),0.25f);
        draw(pipelines.Graphics(guest,alternate,false,{},true),0.25f);
        saved=Metadata(frontend,*pixel);pipelines.Sync();
    }
    auto& database=Storage::DataBase::Instance();database.Open();
    GuestShaderFrontend restored(profile);Serialization::Archive metadata{std::vector<u8>(saved)};
    auto& restored_pixel=restored.RestoreStage(metadata);
    if(!metadata.IsEoS()||restored_pixel.spirv!=canonical||restored_pixel.specialization!=pixel->specialization||
       restored_pixel.specialization.info!=&restored_pixel.info)
        throw std::runtime_error("Native shader metadata/permutation round trip failed");
    for(const auto size:std::array<size_t,6>{0,4,5,13,saved.size()/2,saved.size()-1}){
        GuestShaderFrontend malformed(profile);Serialization::Archive truncated(std::vector<u8>(saved.begin(),saved.begin()+size));
        bool rejected=false;try{malformed.RestoreStage(truncated);}catch(const std::exception&){rejected=true;}
        if(!rejected)throw std::runtime_error("Truncated native metadata was accepted");
    }
    database.Close();
    {
        GuestShaderFrontend warm_frontend(profile);GuestShaderCache warm_shaders(std::filesystem::path(argv[1]).wstring(),options);
        GuestPipelineCache warm(device.Get(),std::filesystem::path(argv[2]).wstring(),warm_frontend,warm_shaders);
        auto warm_guest=warm_frontend.BuildGraphics(regs);
        const auto preloaded=warm.GraphicsCount();
        if(preloaded<4)throw std::runtime_error("Pipeline preload omitted descriptor/root runtime variants");
        auto* warm_pixel=warm_guest.stages[u32(Shader::SwStage::Fragment)];
        if(warm_pixel->spirv!=canonical||warm_shaders.PersistentHits()<1)
            throw std::runtime_error("Actual pipeline preload did not reconstruct cached shaders/PSOs");
        guest=warm_guest;pixel=warm_pixel;draw(warm.Graphics(guest,state.pipeline),0.25f);draw(warm.Graphics(guest,alternate),0.25f);
        const auto& warm_root=warm.Graphics(guest,state.pipeline,false,{},true);
        const auto& warm_root_alternate=warm.Graphics(guest,alternate,false,{},true);
        if(warm.GraphicsCount()!=preloaded||warm_root.Layout().GraphicsRuntimeParameter()==UINT_MAX)
            throw std::runtime_error("Persisted root runtime profile was rebuilt or restored as a table");
        draw(warm_root,0.25f);draw(warm_root_alternate,0.25f);
        warm.Sync();
    }
    for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T size{};Check(messages->GetMessage(i,nullptr,&size));
        std::vector<std::byte> data(size);auto* message=reinterpret_cast<D3D12_MESSAGE*>(data.data());Check(messages->GetMessage(i,message,&size));
        if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);
    }
    if(GetModuleHandleW(L"vulkan-1.dll"))throw std::runtime_error("Unexpected Vulkan runtime");
    puts("PASS: GCN -> two native PSOs -> changed GPU contents -> failed replacement rollback -> canonical restore -> metadata and real pipeline preload");
    return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
