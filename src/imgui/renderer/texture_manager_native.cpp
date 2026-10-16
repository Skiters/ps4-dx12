// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "texture_manager.h"
#include "imgui_impl_dx12.h"
#include "common/io_file.h"
#include "common/stb.h"
#include "common/thread.h"
#include "video_core/renderer_d3d12/resources.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <cstring>
#include <unordered_set>

namespace ImGui {
namespace Core::TextureManager {
struct Inner {
    std::atomic_uint count{1};
    ImTextureData texture;
    std::atomic<ImTextureID> id{nullptr};
    u32 width{}, height{};
};
struct Job { Inner* core; std::vector<u8> bytes; std::filesystem::path path; };
struct Ready { Inner* core; bool destroy; };
struct Retired { Inner* core;NativeDX12::ComPtr<ID3D12Fence> fence;u64 value; };
static std::mutex jobs_mutex, ready_mutex;
static std::condition_variable jobs_cv;
static std::deque<Job> jobs;
static std::deque<Ready> ready;
static std::deque<Retired> retired;
static std::unordered_set<Inner*> live_textures;
static bool renderer_active{};
static void (*update_texture)(ImTextureData*)=ImGui_ImplDX12_UpdateTexture;
static std::jthread worker;
static bool running{};
static void Release(Inner* value) {
    if(value && value->count.fetch_sub(1,std::memory_order_acq_rel)==1) {
        std::lock_guard lock(ready_mutex);
        if(renderer_active)ready.push_back({value,true});else delete value;
    }
}
static void Work() {
    Common::SetCurrentThreadName("shadPS4:NativeUiImages");
    for(;;) {
        Job job;
        {
            std::unique_lock lock(jobs_mutex);
            jobs_cv.wait(lock,[]{return !running || !jobs.empty();});
            if(!running && jobs.empty()) return;
            job=std::move(jobs.front());jobs.pop_front();
        }
        try {
            if(!job.path.empty()) {
                Common::FS::IOFile file(job.path,Common::FS::FileAccessMode::Read);
                if(!file.IsOpen() || file.GetSize()>INT_MAX) throw std::runtime_error("Invalid UI PNG file");
                job.bytes.resize(file.GetSize());file.ReadRaw<u8>(job.bytes.data(),job.bytes.size());
            }
            if(job.bytes.empty() || job.bytes.size()>INT_MAX) throw std::runtime_error("Invalid UI PNG input");
            int width{},height{};
            std::unique_ptr<stbi_uc,decltype(&stbi_image_free)> pixels(
                stbi_load_from_memory(job.bytes.data(),int(job.bytes.size()),&width,&height,nullptr,4),stbi_image_free);
            if(!pixels || width<1 || height<1 || width>D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
               height>D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION||u64(width)*height>SIZE_MAX/4)
                throw std::runtime_error("UI PNG decode/dimension limit");
            job.core->texture.Create(ImTextureFormat_RGBA32,width,height);
            memcpy(job.core->texture.Pixels,pixels.get(),size_t(width)*height*4);
            job.core->width=width;job.core->height=height;
            std::lock_guard lock(ready_mutex);ready.push_back({job.core,false});
        }catch(const std::exception& e){LOG_ERROR(ImGui,"Native UI image: {}",e.what());Release(job.core);}
    }
}
void StartWorker(void (*provider)(ImTextureData*)) {
    std::lock_guard lock(jobs_mutex);
    if(running)throw std::logic_error("Native UI image worker already running");
    {std::lock_guard ready_lock(ready_mutex);renderer_active=true;}
    update_texture=provider?provider:ImGui_ImplDX12_UpdateTexture;
    running=true;
    try{worker=std::jthread(Work);}
    catch(...){running=false;std::lock_guard ready_lock(ready_mutex);renderer_active=false;throw;}
}
void StopWorker() {
    {std::lock_guard lock(jobs_mutex);running=false;}
    jobs_cv.notify_all();if(worker.joinable())worker.join();
}
void DecodePngTexture(std::vector<u8> data,Inner* value) {
    {std::lock_guard lock(jobs_mutex);
        if(!running)throw std::logic_error("Native UI image worker is stopped");
        jobs.push_back({value,std::move(data),{}});++value->count;}
    jobs_cv.notify_one();
}
void DecodePngFile(std::filesystem::path path,Inner* value) {
    {std::lock_guard lock(jobs_mutex);
        if(!running)throw std::logic_error("Native UI image worker is stopped");
        jobs.push_back({value,{},std::move(path)});++value->count;}
    jobs_cv.notify_one();
}
void Submit(ID3D12Fence* fence,u64 fence_value) {
    std::deque<Ready> batch;
    {std::lock_guard lock(ready_mutex);batch.swap(ready);}
    for(auto item:batch) {
        auto* value=item.core;
        if(item.destroy) {
            retired.push_back({value,fence,fence_value});
        }else {
            live_textures.insert(value);
            update_texture(&value->texture);
            value->id.store(value->texture.GetTexID(),std::memory_order_release);
            Release(value);
        }
    }
    for(auto item=retired.begin();item!=retired.end();){
        if(item->fence){
            const auto completed=item->fence->GetCompletedValue();
            if(completed==UINT64_MAX)throw std::runtime_error("Native UI texture retirement device removed");
            if(completed<item->value){++item;continue;}
        }
        auto* value=item->core;
        if(value->texture.GetTexID()!=ImTextureID_Invalid){
            value->texture.SetStatus(ImTextureStatus_WantDestroy);value->texture.UnusedFrames=3;
            update_texture(&value->texture);
            if(value->texture.GetTexID()!=ImTextureID_Invalid)throw std::runtime_error("Native UI texture retirement incomplete");
        }
        live_textures.erase(value);delete value;item=retired.erase(item);
    }
}

void Shutdown(ID3D12Fence* fence,u64 value){
    Submit(fence,value);Submit(fence,value);
    if(!retired.empty())throw std::logic_error("Native UI shutdown has unretired image submissions");
    for(auto* image:live_textures){
        image->id.store(nullptr,std::memory_order_release);
        if(image->texture.GetTexID()!=ImTextureID_Invalid){
            image->texture.SetStatus(ImTextureStatus_WantDestroy);image->texture.UnusedFrames=3;
            update_texture(&image->texture);
            if(image->texture.GetTexID()!=ImTextureID_Invalid)throw std::runtime_error("Native UI image shutdown incomplete");
        }
    }
    live_textures.clear();
    std::lock_guard lock(ready_mutex);
    for(const auto& item:ready){
        if(!item.destroy||item.core->texture.BackendUserData)
            throw std::logic_error("Native UI shutdown still has undecoded images");
    }
    renderer_active=false;
    for(const auto& item:ready)delete item.core;
    ready.clear();
}
}
using Core::TextureManager::Inner;
RefCountedTexture::RefCountedTexture(Inner* value):inner(value){}
RefCountedTexture::RefCountedTexture():inner(nullptr){}
RefCountedTexture RefCountedTexture::DecodePngTexture(std::vector<u8> data) {
    auto value=std::make_unique<Inner>();Core::TextureManager::DecodePngTexture(std::move(data),value.get());
    return RefCountedTexture(value.release());
}
RefCountedTexture RefCountedTexture::DecodePngFile(std::filesystem::path path) {
    auto value=std::make_unique<Inner>();Core::TextureManager::DecodePngFile(std::move(path),value.get());
    return RefCountedTexture(value.release());
}
RefCountedTexture::RefCountedTexture(const RefCountedTexture& other):inner(other.inner){if(inner)++inner->count;}
RefCountedTexture::RefCountedTexture(RefCountedTexture&& other)noexcept:inner(std::exchange(other.inner,nullptr)){}
RefCountedTexture& RefCountedTexture::operator=(const RefCountedTexture& other) {
    if(this!=&other){auto* next=other.inner;if(next)++next->count;Core::TextureManager::Release(inner);inner=next;}return *this;
}
RefCountedTexture& RefCountedTexture::operator=(RefCountedTexture&& other)noexcept {
    if(this!=&other){Core::TextureManager::Release(inner);inner=std::exchange(other.inner,nullptr);}return *this;
}
RefCountedTexture::~RefCountedTexture(){Core::TextureManager::Release(inner);}
RefCountedTexture::Image RefCountedTexture::GetTexture()const {
    if(!inner)return {};const auto id=inner->id.load(std::memory_order_acquire);
    if(!id)return {};return {id,inner->width,inner->height};
}
RefCountedTexture::operator bool()const{return inner && inner->id.load(std::memory_order_acquire);}
}
