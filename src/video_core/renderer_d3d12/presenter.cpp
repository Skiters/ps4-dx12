#include "common/api_trace.h"
#include "common/runtime_module_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "presenter.h"
#include "texture_resources.h"
#include "gpu_markers.h"
#include "device_diagnostics.h"
#include <d3dcompiler.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
namespace NativeDX12 {
struct Presenter::CaptureFrame {
    TextureTransfer transfer;
    CaptureRequest request;
    UINT width{},height{};
    DXGI_FORMAT format{};
    bool hdr{};
    D3D12_BOX region{};
    uint64_t fence{};
};
void Presenter::Capture(bool with_overlays,PresentCaptureCallback callback){
    if(!callback)throw std::invalid_argument("Missing native screenshot callback");
    capture_requests_.push_back({with_overlays,false,std::move(callback)});
}
void Presenter::CaptureSource(PresentCaptureCallback callback){
    if(!callback)throw std::invalid_argument("Missing native source screenshot callback");
    capture_requests_.push_back({false,true,std::move(callback)});
}
void Presenter::CollectCaptures(){
    const auto completed=timeline_.Completed();
    for(auto it=pending_captures_.begin();it!=pending_captures_.end();){
        auto& frame=**it;
        if(!frame.fence||frame.fence>completed){++it;continue;}
        std::vector<std::byte> bytes(size_t(frame.transfer.row_bytes)*frame.height);
        frame.transfer.ReadRows(bytes.data(),bytes.size(),frame.transfer.row_bytes);
        frame.request.callback(frame.width,frame.height,frame.format,frame.hdr,bytes);
        it=pending_captures_.erase(it);
    }
}
void Presenter::EnsureScaler() {
    if(scale_pipeline_)return;
    // The system compiler is used only for this fixed presentation shader.
    // Guest shaders continue through the existing GCN -> DXIL compiler.
    struct Library {HMODULE handle;~Library(){if(handle)FreeLibrary(handle);}} library{
        Common::RuntimeModuleTrace::Load(L"d3dcompiler_47.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32)};
    if(!library.handle)throw std::runtime_error("System presentation shader compiler unavailable");
    auto compile=reinterpret_cast<decltype(&D3DCompile)>(GetProcAddress(library.handle,"D3DCompile"));
    if(!compile)throw std::runtime_error("System D3DCompile unavailable");
    constexpr char code[]=R"(
Texture2D<float4> image:register(t0);SamplerState linear_clamp:register(s0);
cbuffer DisplayParameters:register(b0){float display_gamma;uint hdr_output;uint srgb_input;};
struct Varying {float4 position:SV_Position;float2 uv:TEXCOORD0;};
Varying vertex(uint id:SV_VertexID){Varying o;
o.uv=float2((id<<1)&2,id&2);o.position=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;}
float3 gamma(float3 rgb){return rgb<0.0031308?12.92*rgb/display_gamma:
    1.055*pow(rgb,1.0/(2.4+1.0-display_gamma))-0.055;}
float3 degamma(float3 rgb){return rgb<12.92*0.0031308?rgb/12.92:
    pow(max(rgb+0.055,0.0)/1.055,2.4);}
float4 pixel(Varying i):SV_Target0{float4 color=image.SampleLevel(linear_clamp,i.uv,0);
    if(hdr_output!=0)return color;
    if(srgb_input!=0)color.rgb=degamma(color.rgb);
    return float4(gamma(color.rgb),color.a);}
)";
    ComPtr<ID3DBlob> vs,ps,error;
    auto build=[&](const char* entry,const char* target,ComPtr<ID3DBlob>& result){
        const auto hr=DX12_API_CALL(compile(code,sizeof(code)-1,"native-display-scale",nullptr,nullptr,entry,target,
            D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&result,&error));
        if(FAILED(hr))throw std::runtime_error(error?std::string(static_cast<const char*>(DX12_API_CALL(error->GetBufferPointer())),DX12_API_CALL(error->GetBufferSize())):"Presentation shader compile failed");
    };
    build("vertex","vs_5_0",vs);build("pixel","ps_5_0",ps);
    D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0};
    D3D12_ROOT_PARAMETER parameters[2]{};parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable={1,&range};parameters[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[1].Constants={0,0,3};parameters[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.MaxAnisotropy=1;
    sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC root{2,parameters,1,&sampler,D3D12_ROOT_SIGNATURE_FLAG_NONE};
    ComPtr<ID3DBlob> serialized;Check(DX12_API_CALL(D3D12SerializeRootSignature(&root,D3D_ROOT_SIGNATURE_VERSION_1,&serialized,&error)));
    Check(DX12_API_CALL(device_->CreateRootSignature(0,DX12_API_CALL(serialized->GetBufferPointer()),DX12_API_CALL(serialized->GetBufferSize()),IID_PPV_ARGS(&scale_root_))));
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline{};pipeline.pRootSignature=scale_root_.Get();
    pipeline.VS={DX12_API_CALL(vs->GetBufferPointer()),DX12_API_CALL(vs->GetBufferSize())};pipeline.PS={DX12_API_CALL(ps->GetBufferPointer()),DX12_API_CALL(ps->GetBufferSize())};
    auto& blend=pipeline.BlendState.RenderTarget[0];blend.SrcBlend=blend.SrcBlendAlpha=D3D12_BLEND_ONE;
    blend.DestBlend=blend.DestBlendAlpha=D3D12_BLEND_ZERO;blend.BlendOp=blend.BlendOpAlpha=D3D12_BLEND_OP_ADD;
    blend.LogicOp=D3D12_LOGIC_OP_NOOP;blend.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
    pipeline.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;pipeline.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
    pipeline.RasterizerState.DepthClipEnable=TRUE;pipeline.SampleMask=UINT_MAX;
    pipeline.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;
    pipeline.DepthStencilState.FrontFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_ALWAYS};
    pipeline.DepthStencilState.BackFace=pipeline.DepthStencilState.FrontFace;
    pipeline.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeline.NumRenderTargets=1;pipeline.RTVFormats[0]=output_format_;pipeline.SampleDesc.Count=1;
    Check(DX12_API_CALL(device_->CreateGraphicsPipelineState(&pipeline,IID_PPV_ARGS(&scale_pipeline_))));
    scale_srvs_=std::make_unique<Descriptors>(device_.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,3,true);
    scale_rtvs_=std::make_unique<Descriptors>(device_.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_RTV,3,false);
}
Presenter::Presenter(ID3D12Device* device, ID3D12CommandQueue* queue, HWND window,
                     UINT width, UINT height)
    : device_(device), queue_(queue), timeline_(device), window_(window), width_(width), height_(height) {
    if (!window || !width || !height) throw std::invalid_argument("Invalid presentation target");
    if(DX12_API_CALL(queue->GetDesc()).Type!=D3D12_COMMAND_LIST_TYPE_DIRECT)
        throw std::invalid_argument("Presentation requires a direct queue");
    ComPtr<IDXGIFactory6> factory;
    Check(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))));
    BOOL tearing=FALSE;
    if(SUCCEEDED(DX12_API_CALL(factory->CheckFeatureSupport(
           DXGI_FEATURE_PRESENT_ALLOW_TEARING,&tearing,sizeof(tearing)))))
        allow_tearing_=tearing==TRUE;
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width=width;desc.Height=height;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount=3;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    if(allow_tearing_)desc.Flags|=DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    ComPtr<IDXGISwapChain1> created;
    Check(DX12_API_CALL(factory->CreateSwapChainForHwnd(queue,window,&desc,nullptr,nullptr,&created)));
    Check(DX12_API_CALL(created.As(&swap_)));
    Check(DX12_API_CALL(factory->MakeWindowAssociation(window,DXGI_MWA_NO_ALT_ENTER)));
    UINT color_support{};
    hdr_supported_=SUCCEEDED(DX12_API_CALL(swap_->CheckColorSpaceSupport(
        DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020,&color_support)))&&
        (color_support&DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT);
    for(UINT i=0;i<3;i++) {
        Check(DX12_API_CALL(swap_->GetBuffer(i,IID_PPV_ARGS(&buffers_[i]))));
        Check(DX12_API_CALL(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocators_[i]))));
    }
    Check(DX12_API_CALL(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,
          allocators_[0].Get(),nullptr,IID_PPV_ARGS(&commands_))));
    Check(DX12_API_CALL(commands_->Close()));
}
Presenter::~Presenter(){try{Drain();}catch(...) {}}
void Presenter::Drain(){
    // DXGI may enqueue presentation work after the command-list fence. A
    // marker after Present retires that work before backbuffers are released.
    timeline_.Wait(timeline_.Signal(queue_.Get()));
    CollectCaptures();
}
void Presenter::Resize(UINT width,UINT height){
    if(!width||!height||width>16384||height>16384)
        throw std::invalid_argument("Invalid native swapchain dimensions");
    if(width==width_&&height==height_)return;
    Drain();
    for(auto& source:source_lifetimes_)source.reset();
    for(auto& lease:fsr_leases_)lease.reset();
    for(auto& buffer:buffers_)buffer.Reset();
    const UINT flags=allow_tearing_?DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING:0;
    const auto resized=DX12_API_CALL(swap_->ResizeBuffers(UINT(buffers_.size()),width,height,output_format_,flags));
    if(FAILED(resized)){
        for(UINT i=0;i<buffers_.size();++i)Check(DX12_API_CALL(swap_->GetBuffer(i,IID_PPV_ARGS(&buffers_[i]))));
        Check(resized);
    }
    for(UINT i=0;i<buffers_.size();++i)Check(DX12_API_CALL(swap_->GetBuffer(i,IID_PPV_ARGS(&buffers_[i]))));
    completed_.fill(0);width_=width;height_=height;fsr_.reset();
}
void Presenter::SetHDR(bool enabled){
    if(enabled==hdr_enabled_)return;
    if(enabled&&!hdr_supported_)throw std::runtime_error("DXGI output does not support HDR10 presentation");
    Drain();
    for(auto& source:source_lifetimes_)source.reset();
    for(auto& lease:fsr_leases_)lease.reset();
    for(auto& buffer:buffers_)buffer.Reset();
    const auto requested_format=enabled?DXGI_FORMAT_R10G10B10A2_UNORM:DXGI_FORMAT_R8G8B8A8_UNORM;
    const UINT flags=allow_tearing_?DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING:0;
    const auto resized=DX12_API_CALL(swap_->ResizeBuffers(UINT(buffers_.size()),width_,height_,requested_format,flags));
    if(FAILED(resized)){
        for(UINT i=0;i<buffers_.size();++i)Check(DX12_API_CALL(swap_->GetBuffer(i,IID_PPV_ARGS(&buffers_[i]))));
        Check(resized);
    }
    try{
        for(UINT i=0;i<buffers_.size();++i)Check(DX12_API_CALL(swap_->GetBuffer(i,IID_PPV_ARGS(&buffers_[i]))));
        Check(DX12_API_CALL(swap_->SetColorSpace1(enabled?DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
            DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709)));
    }catch(...){
        for(auto& buffer:buffers_)buffer.Reset();
        Check(DX12_API_CALL(swap_->ResizeBuffers(UINT(buffers_.size()),width_,height_,output_format_,flags)));
        for(UINT i=0;i<buffers_.size();++i)Check(DX12_API_CALL(swap_->GetBuffer(i,IID_PPV_ARGS(&buffers_[i]))));
        Check(DX12_API_CALL(swap_->SetColorSpace1(hdr_enabled_?DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
            DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709)));
        throw;
    }
    completed_.fill(0);hdr_enabled_=enabled;output_format_=requested_format;
    // The RTV format is part of the PSO; rebuild it lazily on the next frame.
    scale_pipeline_.Reset();scale_root_.Reset();scale_srvs_.reset();scale_rtvs_.reset();
}
void Presenter::Present(Resource& source, TextureTransfer* capture,std::shared_ptr<void> source_lifetime,
    DXGI_FORMAT view_format,UINT mapping,bool srgb_input,bool reusing,const std::function<void()>& before_present) try {
    if(!source.handle)throw std::invalid_argument("Null presentation texture");
    CollectCaptures();
    const auto desc=DX12_API_CALL(source.handle->GetDesc());
    if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D || !desc.Width || desc.Width>16384 ||
       !desc.Height || desc.Height>16384 ||
       desc.MipLevels!=1 || desc.DepthOrArraySize!=1 || desc.SampleDesc.Count!=1)
        throw std::invalid_argument("Native presentation requires a single-plane 2D texture");
    if(view_format==DXGI_FORMAT_UNKNOWN){
        switch(desc.Format){
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:view_format=DXGI_FORMAT_R8G8B8A8_UNORM;break;
        case DXGI_FORMAT_R10G10B10A2_TYPELESS:view_format=DXGI_FORMAT_R10G10B10A2_UNORM;break;
        case DXGI_FORMAT_R16G16B16A16_TYPELESS:view_format=DXGI_FORMAT_R16G16B16A16_FLOAT;break;
        default:view_format=desc.Format;break;
        }
    }
    DXGI_FORMAT family{};
    switch(view_format){
    case DXGI_FORMAT_R8G8B8A8_UNORM:case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:family=DXGI_FORMAT_R8G8B8A8_TYPELESS;break;
    case DXGI_FORMAT_R10G10B10A2_UNORM:family=DXGI_FORMAT_R10G10B10A2_TYPELESS;break;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:family=DXGI_FORMAT_R16G16B16A16_TYPELESS;break;
    default:throw std::invalid_argument("Unsupported native VideoOut source view format");
    }
    if(desc.Format!=view_format&&desc.Format!=family)
        throw std::invalid_argument("Native VideoOut source view does not match its resource family");
    RECT client{};
    if(window_&&GetClientRect(window_,&client)&&client.right>client.left&&client.bottom>client.top)
        Resize(UINT(client.right-client.left),UINT(client.bottom-client.top));
    // Match the original FitImage rounding and centered viewport.
    const float frame_aspect=float(desc.Width)/float(desc.Height);
    const float output_aspect=float(width_)/float(height_);
    UINT image_width=width_,image_height=height_;
    if(frame_aspect>output_aspect)image_height=UINT(float(width_)/frame_aspect);
    else image_width=UINT(float(height_)*frame_aspect);
    PresentRegion region{LONG((width_-image_width)/2),LONG((height_-image_height)/2),image_width,image_height,
        {0,0,LONG(width_),LONG(height_)},true};
    if(layout_)region=layout_(UINT(desc.Width),desc.Height,reusing);
    if(!region.width||!region.height||region.width>16384||region.height>16384)
        throw std::out_of_range("Invalid native game presentation region");
    image_width=region.width;image_height=region.height;
    const auto image_x=region.x,image_y=region.y;
    using_fsr_=fsr_settings_.enable&&image_width&&image_height&&
        (desc.Width<image_width||desc.Height<image_height);
    const bool scale=bool(compositor_)||desc.Width!=width_||desc.Height!=height_||desc.Format!=output_format_||
        std::abs(gamma_-1.f)>0.0001f||hdr_enabled_||srgb_input||
        view_format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB||mapping!=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING||using_fsr_;
    if(scale)EnsureScaler();
    const UINT i=DX12_API_CALL(swap_->GetCurrentBackBufferIndex());
    if(completed_[i])timeline_.Wait(completed_[i]);
    // The previous use of this backbuffer has retired, including reads from
    // its presentation source.  Release that source only now; the new one is
    // retained through the fence recorded below.
    source_lifetimes_[i].reset();
    fsr_leases_[i].reset();
    if(using_fsr_){
        if(!fsr_)fsr_=std::make_unique<FsrPass>(device_.Get());
        fsr_leases_[i]=fsr_->Prepare(i,source,view_format,mapping,image_width,image_height,fsr_settings_,hdr_enabled_);
    }
    std::vector<std::shared_ptr<CaptureFrame>> captures;
    captures.reserve(capture_requests_.size());
    pending_captures_.reserve(pending_captures_.size()+capture_requests_.size());
    for(const auto& request:capture_requests_){
        auto frame=std::make_shared<CaptureFrame>();frame->request=request;
        frame->width=request.source?UINT(desc.Width):request.overlays?width_:image_width;
        frame->height=request.source?desc.Height:request.overlays?height_:image_height;
        frame->format=request.source?view_format:output_format_;frame->hdr=!request.source&&hdr_enabled_;
        if(!request.source&&!request.overlays&&(image_x<0||image_y<0||
            uint64_t(image_x)+image_width>width_||uint64_t(image_y)+image_height>height_))
            throw std::out_of_range("Native fitted screenshot region exceeds the output");
        const UINT left=request.overlays||request.source?0:UINT(image_x),top=request.overlays||request.source?0:UINT(image_y);
        frame->region={left,top,0,left+frame->width,top+frame->height,1};
        auto description=request.source?desc:buffers_[i]->GetDesc();description.Width=frame->width;description.Height=frame->height;
        frame->transfer=TextureTransfer::Create(device_.Get(),description,true);
        captures.push_back(std::move(frame));
    }
    Resource* displayed=&source;
    if(fsr_leases_[i])displayed=&fsr_leases_[i]->Output();
    if(scale){
        D3D12_SHADER_RESOURCE_VIEW_DESC view{};view.Format=using_fsr_?DXGI_FORMAT_R16G16B16A16_FLOAT:view_format;
        view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
        view.Shader4ComponentMapping=using_fsr_?D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING:mapping;view.Texture2D.MipLevels=1;
        DX12_API_CALL(device_->CreateShaderResourceView(displayed->handle.Get(),&view,scale_srvs_->Cpu(i)));
        DX12_API_CALL(device_->CreateRenderTargetView(buffers_[i].Get(),nullptr,scale_rtvs_->Cpu(i)));
    }
    const std::function<void()> draw_game=[&]{
        if(!region.visible)return;
        GpuMarkers::Scope postprocess(commands_.Get(),host_markers_,"Postprocess");
        ID3D12DescriptorHeap* heaps[]{scale_srvs_->Heap()};DX12_API_CALL(commands_->SetDescriptorHeaps(1,heaps));
        DX12_API_CALL(commands_->SetGraphicsRootSignature(scale_root_.Get()));DX12_API_CALL(commands_->SetPipelineState(scale_pipeline_.Get()));
        DX12_API_CALL(commands_->SetGraphicsRootDescriptorTable(0,scale_srvs_->Gpu(i)));
        const UINT parameters[]{std::bit_cast<UINT>(gamma_),UINT(hdr_enabled_),UINT(srgb_input)};
        DX12_API_CALL(commands_->SetGraphicsRoot32BitConstants(1,3,parameters,0));
        const auto rtv=scale_rtvs_->Cpu(i);DX12_API_CALL(commands_->OMSetRenderTargets(1,&rtv,FALSE,nullptr));
        const D3D12_VIEWPORT viewport{float(image_x),float(image_y),float(image_width),float(image_height),0,1};
        const D3D12_RECT scissor{std::max({image_x,region.clip.left,0L}),std::max({image_y,region.clip.top,0L}),
            std::min({LONG(image_x+image_width),region.clip.right,LONG(width_)}),
            std::min({LONG(image_y+image_height),region.clip.bottom,LONG(height_)})};
        if(scissor.right<=scissor.left||scissor.bottom<=scissor.top)return;
        DX12_API_CALL(commands_->RSSetViewports(1,&viewport));DX12_API_CALL(commands_->RSSetScissorRects(1,&scissor));
        DX12_API_CALL(commands_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST));DX12_API_CALL(commands_->DrawInstanced(3,1,0,0));
    };
    Check(DX12_API_CALL(allocators_[i]->Reset()));
    Check(DX12_API_CALL(commands_->Reset(allocators_[i].Get(),nullptr)));
    GpuMarkers::Scope present_marker(commands_.Get(),host_markers_,"Present");
    for(const auto& frame:captures)if(frame->request.source)
        frame->transfer.Readback(commands_.Get(),source,&frame->region);
    if(fsr_leases_[i]){
        GpuMarkers::Scope fsr_marker(commands_.Get(),host_markers_,"FSR EASU/RCAS");
        fsr_leases_[i]->Record(commands_.Get(),source);
    }
    displayed->Transition(commands_.Get(),scale?D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition={buffers_[i].Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                        D3D12_RESOURCE_STATE_PRESENT,scale?D3D12_RESOURCE_STATE_RENDER_TARGET:D3D12_RESOURCE_STATE_COPY_DEST};
    DX12_API_CALL(commands_->ResourceBarrier(1,&barrier));
    auto current_state=barrier.Transition.StateAfter;
    const bool fitted_capture=std::any_of(captures.begin(),captures.end(),[](const auto& frame){
        return !frame->request.source&&!frame->request.overlays;
    });
    if(scale){
        const float background[]{0,0,0,1};
        DX12_API_CALL(commands_->ClearRenderTargetView(scale_rtvs_->Cpu(i),background,0,nullptr));
        if(!compositor_||fitted_capture)draw_game();
    }else DX12_API_CALL(commands_->CopyResource(buffers_[i].Get(),source.handle.Get()));
    const auto record_captures=[&](bool with_overlays){
        Resource backbuffer{buffers_[i],current_state};
        for(const auto& frame:captures)if(!frame->request.source&&frame->request.overlays==with_overlays)
            frame->transfer.Readback(commands_.Get(),backbuffer,&frame->region);
        current_state=backbuffer.state;
    };
    record_captures(false);
    if(compositor_){
        if(current_state!=D3D12_RESOURCE_STATE_RENDER_TARGET){
            barrier.Transition.StateBefore=current_state;barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_RENDER_TARGET;
            DX12_API_CALL(commands_->ResourceBarrier(1,&barrier));current_state=D3D12_RESOURCE_STATE_RENDER_TARGET;
        }
        if(fitted_capture){
            const float background[]{0,0,0,1};
            DX12_API_CALL(commands_->ClearRenderTargetView(scale_rtvs_->Cpu(i),background,0,nullptr));
        }
        GpuMarkers::Scope ui_marker(commands_.Get(),host_markers_,"ImGui compositor");
        compositor_(commands_.Get(),buffers_[i].Get(),draw_game);
    }
    if(overlay_ && current_state!=D3D12_RESOURCE_STATE_RENDER_TARGET) {
        barrier.Transition.StateBefore=current_state;
        barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_RENDER_TARGET;
        DX12_API_CALL(commands_->ResourceBarrier(1,&barrier));
        current_state=D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    if(overlay_){GpuMarkers::Scope overlay_marker(commands_.Get(),host_markers_,"ImGui overlay");
        overlay_(commands_.Get(),buffers_[i].Get());}
    record_captures(true);
    if(capture) {
        Resource backbuffer{buffers_[i],current_state};
        capture->Readback(commands_.Get(),backbuffer);
        barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_SOURCE;
    } else {
        barrier.Transition.StateBefore=current_state;
    }
    barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_PRESENT;
    DX12_API_CALL(commands_->ResourceBarrier(1,&barrier));
    present_marker.End();
    Check(DX12_API_CALL(commands_->Close()));
    completed_[i]=timeline_.Submit(queue_.Get(),commands_.Get());
    for(auto& frame:captures){frame->fence=completed_[i];pending_captures_.push_back(std::move(frame));}
    capture_requests_.clear();
    source_lifetimes_[i]=std::move(source_lifetime);
    game_width_=image_width;game_height_=image_height;
    if(submitted_)submitted_(timeline_.Fence(),completed_[i]);
    if(before_present)before_present();
    Check(DX12_API_CALL(swap_->Present(sync_interval_,present_flags_)));
}catch(...){ReportDeviceRemoval(device_.Get());throw;}
void Presenter::SetPresentMode(std::string_view mode){
    if(mode==present_mode_)return;
    present_mode_=mode;sync_interval_=present_flags_=0;
    if(mode=="Fifo")sync_interval_=1;
    else if(mode=="Immediate"){
        if(allow_tearing_)present_flags_=DXGI_PRESENT_ALLOW_TEARING;
        else{
            sync_interval_=1;
            fprintf(stderr,"Native Immediate presentation is unavailable; using Fifo\n");
        }
    }else if(mode!="Mailbox")
        fprintf(stderr,"Unknown native present mode; using Mailbox\n");
}
}
