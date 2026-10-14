#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_presenter.h"
#include "texture_resources.h"
#include "display_capture.h"
#include <cmath>
#include <cstdio>
#ifdef SHADPS4_NATIVE_DX12_CORE
#include "core/emulator_settings.h"
#include "core/debug_state.h"
#include "video_core/renderdoc.h"
#include "core/libraries/videoout/video_out.h"
#include "imgui/renderer/imgui_core.h"
#endif
namespace NativeDX12 {
#ifdef SHADPS4_NATIVE_DX12_CORE
static PresentCaptureCallback ScreenshotCallback(VideoCore::ScreenshotKind kind,
    std::vector<std::filesystem::path> paths){
    return [kind,paths=std::move(paths)](UINT width,UINT height,DXGI_FORMAT format,bool hdr,
        std::span<const std::byte> bytes){
        VideoCore::ScreenshotFormat image_format;
        switch(format){
        case DXGI_FORMAT_R8G8B8A8_UNORM:image_format=VideoCore::ScreenshotFormat::R8G8B8A8Unorm;break;
        case DXGI_FORMAT_R10G10B10A2_UNORM:image_format=VideoCore::ScreenshotFormat::A2B10G10R10UnormPack32;break;
        default:throw std::runtime_error("Unsupported native screenshot output format");
        }
        VideoCore::SavePendingScreenshot({kind,paths,
            {{reinterpret_cast<const u8*>(bytes.data()),bytes.size()}},width,height,image_format,hdr});
    };
}
#endif
GuestPresenter::GuestPresenter(ID3D12Device* d,ID3D12CommandQueue* q,HWND w,UINT width,UINT height,GuestSurfaceProvider& provider)
    :device_(d),queue_(q),presenter_(d,q,w,width,height),surfaces_(provider),width_(width),height_(height) {
    wchar_t value[2]{};
    independent_vblank_=GetEnvironmentVariableW(L"SHADPS4_DX12_INDEPENDENT_VBLANK",value,2)==1&&value[0]==L'1';
}
GuestPresenter::~GuestPresenter(){try{presenter_.Drain();}catch(...) {}}
GuestPresenter::Frame* GuestPresenter::Store(GuestSurfaceSnapshot snapshot) {
    if(!snapshot.texture||!snapshot.texture->handle)throw std::invalid_argument("Guest surface provider returned no GPU texture");
    auto frame=std::make_unique<Frame>();frame->owner=this;frame->surface=std::move(snapshot);auto ptr=frame.get();
    std::lock_guard lock(mutex_);
    if(pending_.size()>=32)throw std::runtime_error("Native VideoOut frame queue is full");
    pending_.emplace(ptr,std::move(frame));return ptr;
}
void GuestPresenter::RegisterSurface(const Libraries::VideoOut::BufferAttributeGroup& group,VAddr address) {
    if(!address||!group.attrib.width||!group.attrib.height)throw std::invalid_argument("Invalid guest display buffer");
    surfaces_.Register(group,address);
}
VideoCore::HostFrame* GuestPresenter::PrepareFrame(const Libraries::VideoOut::BufferAttributeGroup& group,VAddr address) {
    auto snapshot=surfaces_.Snapshot(group,address);
#ifdef SHADPS4_NATIVE_DX12_CORE
    auto screenshot_paths=VideoCore::BuildScreenshotPaths(VideoCore::ScreenshotKind::GameOnly,
        VideoCore::ConsumeGameOnlyScreenshotRequests());
    VideoCore::ScreenshotFormat screenshot_format{};
    using PixelFormat=Libraries::VideoOut::PixelFormat;
    switch(group.attrib.pixel_format){
    case PixelFormat::A8B8G8R8Srgb:screenshot_format=VideoCore::ScreenshotFormat::R8G8B8A8Srgb;break;
    case PixelFormat::A8R8G8B8Srgb:screenshot_format=VideoCore::ScreenshotFormat::B8G8R8A8Srgb;break;
    case PixelFormat::A2R10G10B10:case PixelFormat::A2R10G10B10Srgb:case PixelFormat::A2R10G10B10Bt2020Pq:
        screenshot_format=VideoCore::ScreenshotFormat::A2R10G10B10UnormPack32;break;
    default:if(!screenshot_paths.empty())throw std::invalid_argument("Unsupported original game screenshot format");
    }
#endif
    auto* frame=Store(std::move(snapshot));
#ifdef SHADPS4_NATIVE_DX12_CORE
    frame->game_screenshot_paths=std::move(screenshot_paths);
    frame->game_screenshot_format=screenshot_format;
    frame->game_screenshot_hdr=group.attrib.pixel_format==PixelFormat::A2R10G10B10Bt2020Pq;
#endif
    return frame;
}
VideoCore::HostFrame* GuestPresenter::PrepareBlankFrame(bool) {
    std::shared_ptr<Resource> blank;
    {
        std::lock_guard lock(mutex_);
        if(!blank_) {
            auto value=std::make_shared<Resource>(Texture2D(device_.Get(),width_,height_,DXGI_FORMAT_R8G8B8A8_UNORM,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST));
            std::vector<std::byte> zero(size_t(width_)*height_*4);
            auto upload=TextureTransfer::Create(device_.Get(),*value,false);upload.WriteRows(zero.data(),zero.size(),UINT64(width_)*4);
            ComPtr<ID3D12CommandAllocator> allocator;Check(DX12_API_CALL(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))));
            ComPtr<ID3D12GraphicsCommandList> list;Check(DX12_API_CALL(device_->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list))));
            upload.Upload(list.Get(),*value);Check(DX12_API_CALL(list->Close()));Timeline completion(device_.Get());completion.Wait(completion.Submit(queue_.Get(),list.Get()));
            blank_=std::move(value);
        }
        blank=blank_;
    }
    return Store({std::move(blank),{}});
}
VideoCore::HostFrame* GuestPresenter::PrepareLastFrame(){std::lock_guard lock(mutex_);return last_.get();}
void GuestPresenter::Present(VideoCore::HostFrame* host,bool reusing,bool) {
    Diagnostics::Scope diagnostic(Diagnostics::Kind::Present,"frame-validation");
    diagnostic.Detail("reused=%u",unsigned(reusing));
    auto* frame=dynamic_cast<Frame*>(host);
    if(!frame||frame->owner!=this)throw std::invalid_argument("Frame belongs to another presenter");
    GuestSurfaceSnapshot snapshot;
    {
        std::lock_guard lock(mutex_);
        if(reusing) {if(last_.get()!=frame)throw std::invalid_argument("Expired repeated frame");}
        else if(!pending_.contains(frame))throw std::invalid_argument("Frame is not pending");
        snapshot=frame->surface;
    }
    diagnostic.Stage("snapshot-ready");
    if(snapshot.wait_ready)snapshot.wait_ready();
    diagnostic.Stage("swapchain-present");
#ifdef SHADPS4_NATIVE_DX12_CORE
    presenter_.SetPresentMode(EmulatorSettings.GetPresentMode());
    presenter_.SetFsr({EmulatorSettings.IsFsrEnabled(),EmulatorSettings.IsRcasEnabled(),
        float(EmulatorSettings.GetRcasAttenuation())/1000.f});
    if(!reusing&&!frame->game_screenshot_paths.empty()){
        presenter_.CaptureSource([paths=std::move(frame->game_screenshot_paths),
            format=frame->game_screenshot_format,hdr=frame->game_screenshot_hdr]
            (UINT width,UINT height,DXGI_FORMAT,bool,std::span<const std::byte> bytes){
                VideoCore::SavePendingScreenshot({VideoCore::ScreenshotKind::GameOnly,paths,
                    {{reinterpret_cast<const u8*>(bytes.data()),bytes.size()}},width,height,format,hdr});
            });
    }
    const auto overlay_requests=VideoCore::ConsumeWithOverlaysScreenshotRequests();
    if(overlay_requests){
        presenter_.Capture(true,ScreenshotCallback(VideoCore::ScreenshotKind::WithOverlays,
            VideoCore::BuildScreenshotPaths(VideoCore::ScreenshotKind::WithOverlays,overlay_requests)));
    }
#endif
    if(!reusing&&snapshot.diagnostic_capture_id){
        const auto id=snapshot.diagnostic_capture_id;
        DisplayCapture::Save(id,"flip",{},"trace_id="+std::to_string(frame->trace_id));
        presenter_.CaptureSource([id](UINT width,UINT height,DXGI_FORMAT format,bool,std::span<const std::byte> bytes){
            DisplayCapture::Save(id,"presenter-source",bytes,"width="+std::to_string(width)+" height="+
                std::to_string(height)+" format="+std::to_string(format));
        });
        presenter_.Capture(true,[id](UINT width,UINT height,DXGI_FORMAT format,bool,std::span<const std::byte> bytes){
            DisplayCapture::Save(id,"backbuffer",bytes,"width="+std::to_string(width)+" height="+
                std::to_string(height)+" format="+std::to_string(format));
        });
    }
    try{presenter_.Present(*snapshot.texture,nullptr,snapshot.texture,snapshot.view_format,
        snapshot.component_mapping,snapshot.srgb_input,reusing,[frame,reusing]{
            if(frame->guest_flip&&!reusing)frame->TraceGpuCompletion();
        });}catch(const std::exception& error){
        if(frame->guest_flip&&!reusing)Common::GuestFrameTrace::Invalid("guest-presentation-error");
        fprintf(stderr,"Native D3D12 presentation failed: %s\n",error.what());fflush(stderr);throw;
    }
#ifdef SHADPS4_NATIVE_DX12_CORE
    DebugState.is_using_fsr=presenter_.UsingFsr();
    const auto source=snapshot.texture->handle->GetDesc();
    DebugState.game_resolution={UINT(source.Width),source.Height};
    DebugState.output_resolution={presenter_.GameWidth(),presenter_.GameHeight()};
#endif
    diagnostic.Stage("presentation-queued");
    if(!reusing){std::lock_guard lock(mutex_);auto node=pending_.extract(frame);last_=std::move(node.mapped());}
}
void GuestPresenter::SetHDR(bool enabled){
    presenter_.SetHDR(enabled);
#ifdef SHADPS4_NATIVE_DX12_CORE
    ImGui::Core::OnSurfaceFormatChange(presenter_.OutputFormat());
#endif
}
void GuestPresenter::SetGamma(float gamma){
    if(!std::isfinite(gamma)||gamma<0.1f||gamma>2.f)throw std::invalid_argument("Invalid native VideoOut gamma");
    presenter_.SetGamma(gamma);
}
}
