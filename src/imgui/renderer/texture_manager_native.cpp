// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "texture_manager.h"
#include "imgui_impl_dx12.h"
#include "common/io_file.h"
#include "common/stb.h"
#include "common/thread.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <cstring>

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
static std::mutex jobs_mutex, ready_mutex;
static std::condition_variable jobs_cv;
static std::deque<Job> jobs;
static std::deque<Ready> ready;
static std::jthread worker;
static bool running{};
static void Release(Inner* value) {
    if(value && value->count.fetch_sub(1,std::memory_order_acq_rel)==1) {
        std::lock_guard lock(ready_mutex); ready.push_back({value,true});
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
                if(!file.IsOpen() || file.GetSize()>32*1024*1024) throw std::runtime_error("Invalid UI PNG file");
                job.bytes.resize(file.GetSize());file.ReadRaw<u8>(job.bytes.data(),job.bytes.size());
            }
            if(job.bytes.empty() || job.bytes.size()>32*1024*1024) throw std::runtime_error("Invalid UI PNG input");
            int width{},height{};
            std::unique_ptr<stbi_uc,decltype(&stbi_image_free)> pixels(
                stbi_load_from_memory(job.bytes.data(),int(job.bytes.size()),&width,&height,nullptr,4),stbi_image_free);
            if(!pixels || width<1 || height<1 || width>8192 || height>8192 || u64(width)*height>16*1024*1024)
                throw std::runtime_error("UI PNG decode/dimension limit");
            job.core->texture.Create(ImTextureFormat_RGBA32,width,height);
            memcpy(job.core->texture.Pixels,pixels.get(),size_t(width)*height*4);
            job.core->width=width;job.core->height=height;
            std::lock_guard lock(ready_mutex);ready.push_back({job.core,false});
        }catch(const std::exception& e){LOG_ERROR(ImGui,"Native UI image: {}",e.what());Release(job.core);}
    }
}
void StartWorker() {
    std::lock_guard lock(jobs_mutex);
    if(running)throw std::logic_error("Native UI image worker already running");
    running=true;worker=std::jthread(Work);
}
void StopWorker() {
    {std::lock_guard lock(jobs_mutex);running=false;}
    jobs_cv.notify_all();if(worker.joinable())worker.join();
}
void DecodePngTexture(std::vector<u8> data,Inner* value) {
    {std::lock_guard lock(jobs_mutex);++value->count;jobs.push_back({value,std::move(data),{}});}
    jobs_cv.notify_one();
}
void DecodePngFile(std::filesystem::path path,Inner* value) {
    {std::lock_guard lock(jobs_mutex);++value->count;jobs.push_back({value,{},std::move(path)});}
    jobs_cv.notify_one();
}
// Native presentation drains the queue between frames before resource retirement.
void Submit() {
    std::deque<Ready> batch;
    {std::lock_guard lock(ready_mutex);batch.swap(ready);}
    for(auto item:batch) {
        auto* value=item.core;
        if(item.destroy) {
            if(value->texture.BackendUserData) {
                value->texture.SetStatus(ImTextureStatus_WantDestroy);value->texture.UnusedFrames=3;
                ImGui_ImplDX12_UpdateTexture(&value->texture);
                if(value->texture.BackendUserData)throw std::runtime_error("Native UI texture retirement incomplete");
            }
            delete value;
        }else {
            ImGui_ImplDX12_UpdateTexture(&value->texture);
            value->id.store(value->texture.GetTexID(),std::memory_order_release);
            Release(value);
        }
    }
}
}
using Core::TextureManager::Inner;
RefCountedTexture::RefCountedTexture(Inner* value):inner(value){}
RefCountedTexture::RefCountedTexture():inner(nullptr){}
RefCountedTexture RefCountedTexture::DecodePngTexture(std::vector<u8> data) {
    auto* value=new Inner;Core::TextureManager::DecodePngTexture(std::move(data),value);return RefCountedTexture(value);
}
RefCountedTexture RefCountedTexture::DecodePngFile(std::filesystem::path path) {
    auto* value=new Inner;Core::TextureManager::DecodePngFile(std::move(path),value);return RefCountedTexture(value);
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
