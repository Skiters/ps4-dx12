// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_graphics_state.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include "video_core/renderer_d3d12/shader_pipeline.h"
#include "video_core/renderer_d3d12/compute_pipeline.h"
#include "video_core/renderer_d3d12/texture_resources.h"
#include "video_core/renderer_d3d12/guest_viewports.h"
#include "shader_recompiler/backend/dxil/dxil_compiler.h"
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <cstdio>
#include <cstring>

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
int wmain(int argc,wchar_t** argv){try{
    if(argc!=3)throw std::invalid_argument("Expected DXC path and graphics contract mode");
    const std::wstring_view mode=argv[2];
    const bool blend=mode==L"minmax",srgb=mode==L"minmax-srgb",discard=mode==L"rasterizer-discard";
    const bool cull=mode==L"cull-both",indices16=mode==L"indices16-oob",indices32=mode==L"indices32-oob";
    const bool empty_viewport=mode==L"empty-viewports";
    if(!blend&&!srgb&&!discard&&!cull&&!indices16&&!indices32&&!empty_viewport)throw std::invalid_argument("Unknown graphics contract mode");
    auto device=HardwareDevice();ComPtr<ID3D12InfoQueue> messages;Check(device.As(&messages));
    auto vs=Shader::Backend::DX12::CompileDxil(argv[1],R"(
RWByteAddressBuffer writes:register(u0);
float4 main(uint id:SV_VertexID):SV_Position{
    uint previous;writes.InterlockedOr(id*4,1,previous);
    return float4(id==1?3:-1,id==2?3:-1,0,1);
})",Shader::Backend::DX12::Stage::Vertex);
    auto ps=Shader::Backend::DX12::CompileDxil(argv[1],R"(
float4 main():SV_Target0{return float4(1,0,1,0);}
)",Shader::Backend::DX12::Stage::Pixel);
    if(!vs.success||!ps.success)throw std::runtime_error(vs.diagnostics+ps.diagnostics);
    Rasterizer rasterizer(device.Get());
    auto writes=UavBuffer(device.Get(),16),readback=Buffer(device.Get(),16,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    auto upload=Buffer(device.Get(),16,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
    const u32 initial[]{0,0,0,0xdeadbeef};void* mapped{};D3D12_RANGE empty{};
    Check(upload.handle->Map(0,&empty,&mapped));memcpy(mapped,initial,sizeof(initial));upload.handle->Unmap(0,nullptr);
    const auto format=srgb?DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:DXGI_FORMAT_R8G8B8A8_UNORM;
    auto color=Texture2D(device.Get(),8,8,format,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_COMMON);
    auto color_upload=TextureTransfer::Create(device.Get(),color,false),color_readback=TextureTransfer::Create(device.Get(),color,true);
    std::array<u8,256> before{};for(UINT i=0;i<64;++i){before[i*4]=64;before[i*4+1]=128;before[i*4+2]=192;before[i*4+3]=255;}
    color_upload.WriteRows(before.data(),before.size(),32);
    Resource index,index_upload;Rasterizer::IndexBinding ibv{};
    if(indices16||indices32){
        const u32 values32[]{0,1,2,2,1,0};const u16 values16[]{0,1,2,2,1,0};const UINT element=indices16?2:4;
        index=Buffer(device.Get(),6*element,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
        index_upload=Buffer(device.Get(),6*element,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        Check(index_upload.handle->Map(0,&empty,&mapped));memcpy(mapped,indices16?static_cast<const void*>(values16):values32,6*element);
        index_upload.handle->Unmap(0,nullptr);rasterizer.RecordingList()->CopyBufferRegion(index.handle.Get(),0,index_upload.handle.Get(),0,6*element);
        ibv={&index,0,3*element,indices16?DXGI_FORMAT_R16_UINT:DXGI_FORMAT_R32_UINT};
    }
    AmdGpu::Regs regs{};regs.primitive_type=AmdGpu::PrimitiveType::TriangleList;
    regs.color_control.mode=AmdGpu::ColorControl::OperationMode::Normal;regs.color_control.rop3=AmdGpu::ColorControl::LogicOp::Copy;
    regs.color_target_mask.raw=regs.color_shader_mask.raw=15;regs.color_buffers[0].info.format=10;
    regs.color_buffers[0].info.number_type=u32(srgb?AmdGpu::NumberFormat::Srgb:AmdGpu::NumberFormat::Unorm);
    regs.clipper_control.dx_rasterization_kill=discard;regs.polygon_control.cull_front=regs.polygon_control.cull_back=cull;
    const std::array<DXGI_FORMAT,1> formats{format};const auto fallback=BuildGuestViewports(regs);
    const D3D12_VIEWPORT viewport=empty_viewport?fallback.viewports[0]:D3D12_VIEWPORT{0,0,8,8,0,1};
    const D3D12_RECT scissor=empty_viewport?fallback.scissors[0]:D3D12_RECT{0,0,8,8};
    const float constants[]{1,1,1,1};using Factor=AmdGpu::BlendControl::BlendFactor;using Op=AmdGpu::BlendControl::BlendFunc;
    const unsigned cases=(blend||srgb)?8:1;
    for(unsigned n=0;n<cases;++n){
        const bool minimum=n<4;const unsigned factors=n%4;
        auto& control=regs.blend_control[0];control.enable=blend||srgb;control.separate_alpha_blend=true;
        control.color_func=control.alpha_func=minimum?Op::Min:Op::Max;
        control.color_src_factor=(factors&1)?Factor::One:Factor::Zero;
        control.color_dst_factor=(factors&2)?Factor::One:Factor::Zero;
        control.alpha_src_factor=control.color_dst_factor;control.alpha_dst_factor=control.color_src_factor;
        const auto state=BuildGuestGraphicsState(regs,formats);
        if(state.discard||state.rasterizer_discard!=(discard||cull))throw std::runtime_error("Draw incorrectly skipped vertex shader execution");
        ShaderPipeline pipeline(device.Get(),argv[1],{vs.bytecode,ps.bytecode},state.pipeline);
        for(unsigned run=0;run<2;++run){
            color_upload.Upload(rasterizer.RecordingList(),color);
            writes.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
            rasterizer.RecordingList()->CopyBufferRegion(writes.handle.Get(),0,upload.handle.Get(),0,16);
            writes.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            ResourceBindings bindings(device.Get(),pipeline.Layout());D3D12_UNORDERED_ACCESS_VIEW_DESC view{};
            view.Format=DXGI_FORMAT_R32_TYPELESS;view.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;view.Buffer.NumElements=4;view.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
            bindings.Uav(0,0,writes.handle.Get(),view);
            const Rasterizer::ColorAttachment attachment{&color};Rasterizer::DrawArguments draw{3};
            if(indices16||indices32)draw.first=3;
            rasterizer.ExecuteGraphics(pipeline,bindings,state,std::span(&attachment,1),nullptr,
                std::span(&viewport,1),std::span(&scissor,1),constants,draw,{},ibv.resource?&ibv:nullptr,false);
            color_readback.Readback(rasterizer.RecordingList(),color);
            writes.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            rasterizer.RecordingList()->CopyBufferRegion(readback.handle.Get(),0,writes.handle.Get(),0,16);rasterizer.Finish();
            D3D12_RANGE range{0,16};Check(readback.handle->Map(0,&range,&mapped));u32 result[4];memcpy(result,mapped,16);readback.handle->Unmap(0,&empty);
            if(result[0]!=1||result[1]!=((indices16||indices32)?0:1)||result[2]!=((indices16||indices32)?0:1)||result[3]!=0xdeadbeef)
                throw std::runtime_error("Vertex UAV effects, bounded indices or padding mismatch");
            std::array<u8,256> actual{};color_readback.ReadRows(actual.data(),actual.size(),32);
            auto expected=before;const u8 source[]{255,0,255,0};
            if(blend||srgb)for(unsigned pixel=0;pixel<64;++pixel)for(unsigned component=0;component<4;++component){
                const bool alpha=component==3;const bool src=(factors&(alpha?2:1))!=0,dst=(factors&(alpha?1:2))!=0;
                if(src&&dst)expected[pixel*4+component]=minimum?std::min(source[component],before[pixel*4+component]):std::max(source[component],before[pixel*4+component]);
                else expected[pixel*4+component]=minimum?0:src?source[component]:dst?before[pixel*4+component]:0;
            }
            if(actual!=expected){const auto mismatch=std::mismatch(actual.begin(),actual.end(),expected.begin());
                const auto offset=size_t(mismatch.first-actual.begin());
                throw std::runtime_error("MIN/MAX or discarded raster target contents mismatch: case="+std::to_string(n)+
                    " offset="+std::to_string(offset)+" actual="+std::to_string(actual[offset])+" expected="+std::to_string(expected[offset]));}
        }
    }
    for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T size{};Check(messages->GetMessage(i,nullptr,&size));std::vector<std::byte> bytes(size);
        auto* message=reinterpret_cast<D3D12_MESSAGE*>(bytes.data());Check(messages->GetMessage(i,message,&size));
        if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);
    }
    puts("PASS: actual graphics contents, retained vertex UAV side effects, bounded index reads, padding and D3D12 validation");return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
