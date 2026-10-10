#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_presenter.h"
#include "texture_resources.h"
#include <cmath>
#include <cstdio>
namespace NativeDX12 {
GuestPresenter::GuestPresenter(ID3D12Device* d,ID3D12CommandQueue* q,HWND w,UINT width,UINT height,GuestSurfaceProvider& provider)
    :device_(d),queue_(q),presenter_(d,q,w,width,height),surfaces_(provider),width_(width),height_(height) {}
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
    return Store(surfaces_.Snapshot(group,address));
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
    try{presenter_.Present(*snapshot.texture,nullptr,snapshot.texture);}catch(const std::exception& error){
        fprintf(stderr,"Native D3D12 presentation failed: %s\n",error.what());fflush(stderr);throw;
    }
    diagnostic.Stage("presentation-queued");
    if(!reusing){std::lock_guard lock(mutex_);auto node=pending_.extract(frame);last_=std::move(node.mapped());}
}
void GuestPresenter::SetHDR(bool enabled){presenter_.SetHDR(enabled);}
void GuestPresenter::SetGamma(float gamma){
    if(!std::isfinite(gamma)||gamma<0.1f||gamma>2.f)throw std::invalid_argument("Invalid native VideoOut gamma");
    presenter_.SetGamma(gamma);
}
}
