#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "presenter.h"
#include "texture_resources.h"
#include <d3dcompiler.h>
#include <bit>
#include <cmath>
#include <cstring>
namespace NativeDX12 {
void Presenter::EnsureScaler() {
    if(scale_pipeline_)return;
    // The system compiler is used only for this fixed presentation shader.
    // Guest shaders continue through the existing GCN -> DXIL compiler.
    struct Library {HMODULE handle;~Library(){if(handle)FreeLibrary(handle);}} library{
        LoadLibraryExW(L"d3dcompiler_47.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32)};
    if(!library.handle)throw std::runtime_error("System presentation shader compiler unavailable");
    auto compile=reinterpret_cast<decltype(&D3DCompile)>(GetProcAddress(library.handle,"D3DCompile"));
    if(!compile)throw std::runtime_error("System D3DCompile unavailable");
    constexpr char code[]=R"(
Texture2D<float4> image:register(t0);SamplerState linear_clamp:register(s0);
cbuffer DisplayParameters:register(b0){float display_gamma;uint hdr_output;
float2 source_texel;float sharpen_strength;};
struct Varying {float4 position:SV_Position;float2 uv:TEXCOORD0;};
Varying vertex(uint id:SV_VertexID){Varying o;
o.uv=float2((id<<1)&2,id&2);o.position=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;}
float3 pq(float3 v){v=max(v,0.0)*0.0203;
const float m1=0.1593017578125,m2=78.84375,c1=0.8359375,c2=18.8515625,c3=18.6875;
float3 p=pow(v,m1);return pow((c1+c2*p)/(1+c3*p),m2);}
float4 spatial(float2 uv){float4 c=image.SampleLevel(linear_clamp,uv,0);
if(sharpen_strength<=0.0)return c;
float3 n=image.SampleLevel(linear_clamp,uv-float2(0,source_texel.y),0).rgb;
float3 s=image.SampleLevel(linear_clamp,uv+float2(0,source_texel.y),0).rgb;
float3 w=image.SampleLevel(linear_clamp,uv-float2(source_texel.x,0),0).rgb;
float3 e=image.SampleLevel(linear_clamp,uv+float2(source_texel.x,0),0).rgb;
float3 mn=min(c.rgb,min(min(n,s),min(w,e))),mx=max(c.rgb,max(max(n,s),max(w,e)));
// Contrast-adaptive spatial sharpening. The negative lobe shrinks near
// black/white extrema, avoiding the ringing of a fixed unsharp kernel.
float3 amplitude=saturate(min(mn,1.0-mx)/max(mx,1e-4));
amplitude=rsqrt(max(amplitude,1e-4));
float3 weight=amplitude*(-1.0/(8.0-3.0*saturate(sharpen_strength)));
c.rgb=saturate((c.rgb+weight*(n+s+w+e))/(1.0+4.0*weight));return c;}
float4 pixel(Varying i):SV_Target0{float4 c=spatial(i.uv);
c.rgb=pow(saturate(c.rgb),rcp(max(display_gamma,0.001)));
if(hdr_output!=0){c.rgb=mul(float3x3(0.627404,0.329283,0.043313,
0.069097,0.919540,0.011362,0.016391,0.088013,0.895595),c.rgb);c.rgb=pq(c.rgb);}return c;}
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
    parameters[1].Constants={0,0,5};parameters[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
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
    : device_(device), queue_(queue), timeline_(device), width_(width), height_(height) {
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
}
void Presenter::SetHDR(bool enabled){
    if(enabled==hdr_enabled_)return;
    if(enabled&&!hdr_supported_)throw std::runtime_error("DXGI output does not support HDR10 presentation");
    Drain();
    for(auto& source:source_lifetimes_)source.reset();
    for(auto& buffer:buffers_)buffer.Reset();
    output_format_=enabled?DXGI_FORMAT_R10G10B10A2_UNORM:DXGI_FORMAT_R8G8B8A8_UNORM;
    const UINT flags=allow_tearing_?DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING:0;
    Check(DX12_API_CALL(swap_->ResizeBuffers(UINT(buffers_.size()),width_,height_,output_format_,flags)));
    for(UINT i=0;i<buffers_.size();++i)Check(DX12_API_CALL(swap_->GetBuffer(i,IID_PPV_ARGS(&buffers_[i]))));
    Check(DX12_API_CALL(swap_->SetColorSpace1(enabled?DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
        DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709)));
    completed_.fill(0);hdr_enabled_=enabled;
    // The RTV format is part of the PSO; rebuild it lazily on the next frame.
    scale_pipeline_.Reset();scale_root_.Reset();scale_srvs_.reset();scale_rtvs_.reset();
}
void Presenter::Present(Resource& source, TextureTransfer* capture,std::shared_ptr<void> source_lifetime) {
    if(!source.handle)throw std::invalid_argument("Null presentation texture");
    const auto desc=DX12_API_CALL(source.handle->GetDesc());
    if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D || !desc.Width || desc.Width>16384 ||
       !desc.Height || desc.Height>16384 || desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM ||
       desc.MipLevels!=1 || desc.DepthOrArraySize!=1 || desc.SampleDesc.Count!=1)
        throw std::invalid_argument("Native presentation requires single-plane RGBA8 texture");
    const bool scale=desc.Width!=width_||desc.Height!=height_||desc.Format!=output_format_||
        std::abs(gamma_-1.f)>0.0001f||hdr_enabled_;
    if(scale)EnsureScaler();
    const UINT i=DX12_API_CALL(swap_->GetCurrentBackBufferIndex());
    if(completed_[i])timeline_.Wait(completed_[i]);
    // The previous use of this backbuffer has retired, including reads from
    // its presentation source.  Release that source only now; the new one is
    // retained through the fence recorded below.
    source_lifetimes_[i].reset();
    Check(DX12_API_CALL(allocators_[i]->Reset()));
    Check(DX12_API_CALL(commands_->Reset(allocators_[i].Get(),nullptr)));
    source.Transition(commands_.Get(),scale?D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition={buffers_[i].Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                        D3D12_RESOURCE_STATE_PRESENT,scale?D3D12_RESOURCE_STATE_RENDER_TARGET:D3D12_RESOURCE_STATE_COPY_DEST};
    DX12_API_CALL(commands_->ResourceBarrier(1,&barrier));
    auto current_state=barrier.Transition.StateAfter;
    if(scale){
        D3D12_SHADER_RESOURCE_VIEW_DESC view{};view.Format=desc.Format;view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
        view.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;view.Texture2D.MipLevels=1;
        DX12_API_CALL(device_->CreateShaderResourceView(source.handle.Get(),&view,scale_srvs_->Cpu(i)));
        DX12_API_CALL(device_->CreateRenderTargetView(buffers_[i].Get(),nullptr,scale_rtvs_->Cpu(i)));
        ID3D12DescriptorHeap* heaps[]{scale_srvs_->Heap()};DX12_API_CALL(commands_->SetDescriptorHeaps(1,heaps));
        DX12_API_CALL(commands_->SetGraphicsRootSignature(scale_root_.Get()));DX12_API_CALL(commands_->SetPipelineState(scale_pipeline_.Get()));
        DX12_API_CALL(commands_->SetGraphicsRootDescriptorTable(0,scale_srvs_->Gpu(i)));
        const float inv_width=1.f/float(desc.Width),inv_height=1.f/float(desc.Height);
        const float sharpen=desc.Width!=width_||desc.Height!=height_?0.6f:0.f;
        const UINT parameters[]{std::bit_cast<UINT>(gamma_),UINT(hdr_enabled_),
            std::bit_cast<UINT>(inv_width),std::bit_cast<UINT>(inv_height),std::bit_cast<UINT>(sharpen)};
        DX12_API_CALL(commands_->SetGraphicsRoot32BitConstants(1,5,parameters,0));
        const auto rtv=scale_rtvs_->Cpu(i);DX12_API_CALL(commands_->OMSetRenderTargets(1,&rtv,FALSE,nullptr));
        const D3D12_VIEWPORT viewport{0,0,float(width_),float(height_),0,1};const D3D12_RECT scissor{0,0,LONG(width_),LONG(height_)};
        DX12_API_CALL(commands_->RSSetViewports(1,&viewport));DX12_API_CALL(commands_->RSSetScissorRects(1,&scissor));
        DX12_API_CALL(commands_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST));DX12_API_CALL(commands_->DrawInstanced(3,1,0,0));
    }else DX12_API_CALL(commands_->CopyResource(buffers_[i].Get(),source.handle.Get()));
    if(overlay_ && current_state!=D3D12_RESOURCE_STATE_RENDER_TARGET) {
        barrier.Transition.StateBefore=current_state;
        barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_RENDER_TARGET;
        DX12_API_CALL(commands_->ResourceBarrier(1,&barrier));
        current_state=D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    if(overlay_)overlay_(commands_.Get(),buffers_[i].Get());
    if(capture) {
        Resource backbuffer{buffers_[i],current_state};
        capture->Readback(commands_.Get(),backbuffer);
        barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_SOURCE;
    } else {
        barrier.Transition.StateBefore=current_state;
    }
    barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_PRESENT;
    DX12_API_CALL(commands_->ResourceBarrier(1,&barrier));
    Check(DX12_API_CALL(commands_->Close()));
    completed_[i]=timeline_.Submit(queue_.Get(),commands_.Get());
    source_lifetimes_[i]=std::move(source_lifetime);
    // Presentation is deliberately ungated. The independent 60 Hz guest
    // vblank remains the PS4 time source, while DXGI no longer stalls the
    // render queue for a host vertical blank.
    Check(DX12_API_CALL(swap_->Present(0,allow_tearing_?DXGI_PRESENT_ALLOW_TEARING:0)));
}
}
