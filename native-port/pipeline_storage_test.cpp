// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/cache_storage.h"
#include "video_core/renderer_d3d12/persistent_cache.h"
#include "core/emulator_settings.h"
#include <barrier>
#include <cstdio>
#include <thread>

static std::vector<u8> Payload(u32 index){
    std::vector<u8> result(512);for(size_t i=0;i<result.size();++i)result[i]=u8(index*37+i*13);
    memcpy(result.data(),&index,sizeof(index));return result;
}
int main(int argc,char** argv){try{
    const bool archived=argc==4&&std::string_view(argv[3])=="archive";
    const auto root=std::filesystem::current_path()/("pipeline-storage-fixture-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    if(!std::filesystem::create_directory(root))throw std::runtime_error("Fixture directory already exists");
    _wputenv_s(L"SHADPS4_NATIVE_SHADER_CACHE",root.c_str());
    EmulatorSettings.SetPipelineCacheEnabled(true);EmulatorSettings.SetPipelineCacheArchived(archived);
    Storage::DataBase database;database.Open();
    bool rejected=false;try{database.Save(Storage::BlobType::PipelineKey,"early",Payload(1));}catch(const std::logic_error&){rejected=true;}
    if(!rejected)throw std::runtime_error("Preload accepted writes");
    database.FinishPreload();
    std::array<std::vector<u32>,4> accepted;std::barrier ready(5);
    std::array<std::jthread,4> producers;
    for(u32 thread=0;thread<producers.size();++thread)producers[thread]=std::jthread([&,thread]{
        for(u32 i=0;i<256;++i){
            const auto id=thread*256+i;
            if(database.Save(Storage::BlobType::PipelineKey,"key-"+std::to_string(id),Payload(id)))accepted[thread].push_back(id);
            if(i==31)ready.arrive_and_wait();
        }
    });
    ready.arrive_and_wait();database.Close();for(auto& producer:producers)producer.join();
    if(database.Save(Storage::BlobType::PipelineKey,"closed",Payload(2)))throw std::runtime_error("Closed database accepted a write");
    database.Open();size_t count{};
    for(const auto& list:accepted)for(const auto id:list){std::vector<u8> actual;
        database.Load(Storage::BlobType::PipelineKey,"key-"+std::to_string(id),actual);
        if(actual!=Payload(id))throw std::runtime_error("Close lost an accepted IO request");++count;
    }
    size_t enumerated{};database.ForEachBlob(Storage::BlobType::PipelineKey,[&](std::vector<u8>&& data){
        if(data.size()!=512)throw std::runtime_error("Enumerated payload size changed");u32 id{};memcpy(&id,data.data(),sizeof(id));
        if(data!=Payload(id))throw std::runtime_error("Enumerated payload changed");++enumerated;
    });
    if(enumerated!=count||count<128)throw std::runtime_error("Native enumeration lost accepted records");
    database.FinishPreload();database.Save(Storage::BlobType::ShaderBinary,"words",std::vector<u32>{0x07230203,0xffffffff,0x80000000,1});
    database.Save(Storage::BlobType::ShaderMeta,"empty",std::vector<u8>{});database.Close();database.Open();
    std::vector<u32> words;database.Load(Storage::BlobType::ShaderBinary,"words",words);
    if(words!=std::vector<u32>{0x07230203,0xffffffff,0x80000000,1})throw std::runtime_error("Word payload changed");
    database.Close();
    if(!archived){
        const auto damaged=root/"pipeline"/"key-0.key";
        {std::fstream file(damaged,std::ios::binary|std::ios::in|std::ios::out);file.seekp(48);const char bad=7;file.write(&bad,1);}
        database.Open();std::vector<u8> data{1};database.Load(Storage::BlobType::PipelineKey,"key-0",data);
        if(!data.empty())throw std::runtime_error("Checksum-corrupted cache contents were accepted");database.Close();
    }
    std::array<std::jthread,3> lifecycles;
    for(auto& thread:lifecycles)thread=std::jthread([&]{for(unsigned i=0;i<12;++i){database.Open();database.FinishPreload();database.Close();}});
    for(auto& thread:lifecycles)thread.join();database.Close();database.Open();
    database.Load(Storage::BlobType::ShaderBinary,"words",words);database.Close();
    if(words!=std::vector<u32>{0x07230203,0xffffffff,0x80000000,1})throw std::runtime_error("Concurrent cache lifecycle changed persisted data");
    printf("PASS: %zu accepted native IO requests drained, exact directory/archive contents, preload/close and concurrent lifecycle\n",count);
    return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
