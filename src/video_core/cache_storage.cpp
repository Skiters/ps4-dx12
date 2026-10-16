// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/cache_storage.h"
#include "common/elf_info.h"
#include "common/logging/log.h"
#include "common/thread.h"
#include "core/emulator_settings.h"
#include "video_core/renderer_d3d12/persistent_cache.h"
#include <miniz.h>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <mutex>
#include <stdexcept>

namespace Storage {
namespace {
std::string GetBlobFileExtension(BlobType type){
    switch(type){
    case BlobType::ShaderMeta:return "meta";
    case BlobType::ShaderBinary:return "spv";
    case BlobType::PipelineKey:return "key";
    case BlobType::ShaderProfile:return "bin";
    default:throw std::invalid_argument("Invalid native cache blob type");
    }
}
std::filesystem::path BlobPath(BlobType type,const std::string& name){
    std::filesystem::path path(name);
    if(path.empty()||path.has_parent_path()||path=="."||path=="..")
        throw std::invalid_argument("Invalid native cache blob name");
    return path.replace_extension(GetBlobFileExtension(type));
}
std::vector<u8> Encode(std::vector<u8> data){
    const auto digest=NativeDX12::CacheDigest::Of(std::as_bytes(std::span(data)));
    std::vector<u8> blob(48+data.size());memcpy(blob.data(),"NDXP001",8);
    const u64 size=data.size();memcpy(blob.data()+8,&size,8);memcpy(blob.data()+16,digest.data(),32);
    memcpy(blob.data()+48,data.data(),data.size());return blob;
}
std::vector<u8> Decode(std::vector<u8> blob){
    if(blob.size()<48||memcmp(blob.data(),"NDXP001",8))throw std::runtime_error("Invalid native cache blob");
    u64 size{};memcpy(&size,blob.data()+8,8);
    if(size!=blob.size()-48)throw std::runtime_error("Short native cache blob");
    const auto digest=NativeDX12::CacheDigest::Of(std::as_bytes(std::span(blob).subspan(48)));
    if(memcmp(blob.data()+16,digest.data(),32))throw std::runtime_error("Corrupted native cache blob");
    return {blob.begin()+48,blob.end()};
}
}
struct DataBase::State{
    std::mutex lifecycle_mutex,jobs_mutex,io_mutex;
    std::condition_variable cv;
    std::deque<std::pair<std::filesystem::path,std::vector<u8>>> jobs;
    std::jthread worker;
    std::filesystem::path path;
    mz_zip_archive archive{};
    bool archived{},reader{},writer{},preloading{true},stopping{};
    u64 next_temp{};
    std::vector<u8> Read(const std::filesystem::path& name){
        std::vector<u8> blob;
        if(archived){
            if(!reader)throw std::logic_error("Native cache archive is not in preload mode");
            const auto index=mz_zip_reader_locate_file(&archive,name.string().c_str(),nullptr,0);
            if(index<0)return {};
            mz_zip_archive_file_stat stat{};
            if(!mz_zip_reader_file_stat(&archive,index,&stat)||stat.m_uncomp_size>blob.max_size())
                throw std::runtime_error("Invalid native cache archive extent");
            blob.resize(size_t(stat.m_uncomp_size));
            if(!mz_zip_reader_extract_to_mem(&archive,index,blob.data(),blob.size(),0))
                throw std::runtime_error("Native cache archive extraction failed");
        }else{
            std::ifstream file(path/name,std::ios::binary|std::ios::ate);
            if(!file)return {};
            const auto size=file.tellg();
            if(size<0||u64(size)>blob.max_size())throw std::runtime_error("Invalid native cache file extent");
            blob.resize(size_t(size));file.seekg(0);
            file.read(reinterpret_cast<char*>(blob.data()),blob.size());
            if(!file)throw std::runtime_error("Native cache file read failed");
        }
        return Decode(std::move(blob));
    }
    void Write(const std::filesystem::path& name,std::vector<u8> data){
        const auto blob=Encode(std::move(data));
        if(archived){
            if(!writer||!mz_zip_writer_add_mem(&archive,name.string().c_str(),blob.data(),blob.size(),MZ_BEST_COMPRESSION))
                throw std::runtime_error("Native cache archive write failed");
        }else{
            const auto final=path/name;auto temporary=final;
            temporary+=L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(next_temp++);
            try{
                {std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
                    file.write(reinterpret_cast<const char*>(blob.data()),blob.size());file.flush();
                    if(!file)throw std::runtime_error("Native cache file write failed");}
                if(!MoveFileExW(temporary.c_str(),final.c_str(),MOVEFILE_REPLACE_EXISTING))
                    throw std::runtime_error("Native cache file replacement failed");
            }catch(...){std::error_code error;std::filesystem::remove(temporary,error);throw;}
        }
    }
    void ProcessIO(){
        Common::SetCurrentThreadName("shadPS4:NativePipelineCacheIO");
        for(;;){
            std::pair<std::filesystem::path,std::vector<u8>> request;
            {std::unique_lock lock(jobs_mutex);cv.wait(lock,[&]{return stopping||!jobs.empty();});
                if(jobs.empty()&&stopping)return;
                request=std::move(jobs.front());jobs.pop_front();}
            try{std::lock_guard lock(io_mutex);Write(request.first,std::move(request.second));}
            catch(const std::exception& error){LOG_ERROR(Render_DX12,"Native cache write: {}",error.what());}
        }
    }
};
DataBase::DataBase():state(std::make_unique<State>()){}
DataBase::~DataBase(){Close();}
void DataBase::Open(){
    auto& s=*state;std::lock_guard lifecycle(s.lifecycle_mutex);
    std::lock_guard lock(s.jobs_mutex);if(opened)return;
    std::lock_guard io(s.io_mutex);
    if(!EmulatorSettings.IsPipelineCacheEnabled())return;
    const auto directory=NativeDX12::NativeCacheDirectory("pipeline");
    if(directory.empty())return;
    s.archived=EmulatorSettings.IsPipelineCacheArchived();s.stopping=false;s.preloading=true;
    s.path=directory;
    std::filesystem::create_directories(directory);
    if(s.archived){
        s.path=directory/"pipeline.zip";mz_zip_zero_struct(&s.archive);
        s.reader=mz_zip_reader_init_file(&s.archive,s.path.string().c_str(),MZ_ZIP_FLAG_READ_ALLOW_WRITING);
        if(s.reader&&!mz_zip_validate_archive(&s.archive,0)){
            mz_zip_reader_end(&s.archive);s.reader=false;
            LOG_WARNING(Render_DX12,"Ignoring corrupted native pipeline archive {}",s.path.string());
        }
    }
    opened=true;
}
void DataBase::FinishPreload(){
    auto& s=*state;std::lock_guard lifecycle(s.lifecycle_mutex);
    std::lock_guard lock(s.jobs_mutex);if(!opened||!s.preloading)return;
    std::lock_guard io(s.io_mutex);
    if(s.archived){
        s.writer=s.reader?mz_zip_writer_init_from_reader(&s.archive,s.path.string().c_str()):
            mz_zip_writer_init_file(&s.archive,s.path.string().c_str(),0);
        if(!s.writer)throw std::runtime_error("Native pipeline archive writer initialization failed");
        s.reader=false;
    }
    s.worker=std::jthread([&s]{s.ProcessIO();});s.preloading=false;
}
void DataBase::Close(){
    auto& s=*state;std::lock_guard lifecycle(s.lifecycle_mutex);
    {std::lock_guard lock(s.jobs_mutex);if(!opened.exchange(false))return;s.stopping=true;}
    s.cv.notify_all();if(s.worker.joinable())s.worker.join();
    std::lock_guard lock(s.io_mutex);
    if(s.writer){
        if(!mz_zip_writer_finalize_archive(&s.archive))LOG_ERROR(Render_DX12,"Native pipeline archive finalization failed");
        mz_zip_writer_end(&s.archive);
    }else if(s.reader)mz_zip_reader_end(&s.archive);
    s.writer=s.reader=false;s.preloading=true;
}
bool DataBase::Save(BlobType type,const std::string& name,std::vector<u8>&& data){
    auto& s=*state;std::lock_guard lock(s.jobs_mutex);if(!opened)return false;
    if(s.preloading)throw std::logic_error("Native cache save before FinishPreload");
    s.jobs.emplace_back(BlobPath(type,name),std::move(data));s.cv.notify_one();return true;
}
bool DataBase::Save(BlobType type,const std::string& name,std::vector<u32>&& data){
    std::vector<u8> bytes(data.size()*sizeof(u32));memcpy(bytes.data(),data.data(),bytes.size());
    return Save(type,name,std::move(bytes));
}
void DataBase::Load(BlobType type,const std::string& name,std::vector<u8>& data){
    data.clear();if(!opened)return;
    auto& s=*state;std::lock_guard lock(s.io_mutex);
    if(!opened)return;
    try{if(!s.archived||s.reader)data=s.Read(BlobPath(type,name));}
    catch(const std::exception& error){LOG_WARNING(Render_DX12,"Native cache load {}: {}",name,error.what());}
}
void DataBase::Load(BlobType type,const std::string& name,std::vector<u32>& data){
    std::vector<u8> bytes;Load(type,name,bytes);data.clear();
    if(bytes.size()%sizeof(u32))return;
    data.resize(bytes.size()/sizeof(u32));memcpy(data.data(),bytes.data(),bytes.size());
}
void DataBase::ForEachBlob(BlobType type,const std::function<void(std::vector<u8>&&)>& func){
    if(!opened)return;auto& s=*state;std::vector<std::vector<u8>> blobs;
    {std::lock_guard lock(s.io_mutex);
        if(!opened)return;
        const auto read=[&](const std::filesystem::path& name){
            if(name.extension()!="."+GetBlobFileExtension(type))return;
            try{auto data=s.Read(name);if(!data.empty())blobs.push_back(std::move(data));}
            catch(const std::exception& error){LOG_WARNING(Render_DX12,"Native cache enumeration: {}",error.what());}
        };
        if(s.archived){
            if(s.reader)for(mz_uint i=0;i<mz_zip_reader_get_num_files(&s.archive);++i){
                mz_zip_archive_file_stat stat{};
                if(mz_zip_reader_file_stat(&s.archive,i,&stat)&&!stat.m_is_directory)read(stat.m_filename);
            }
        }else for(const auto& file:std::filesystem::directory_iterator(s.path))
            if(file.is_regular_file())read(file.path().filename());
    }
    for(auto& blob:blobs)func(std::move(blob));
}
} // namespace Storage

