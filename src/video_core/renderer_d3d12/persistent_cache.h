// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <cstdint>
#include <type_traits>
#pragma comment(lib,"bcrypt.lib")
namespace NativeDX12 {
class CacheKey {
public:
    std::vector<std::byte> bytes;
    template<class T> void Add(T value){
        static_assert(std::is_trivially_copyable_v<T>);
        auto span=std::as_bytes(std::span(&value,1));bytes.insert(bytes.end(),span.begin(),span.end());
    }
    void Blob(std::span<const std::byte> value){Add(uint64_t(value.size()));bytes.insert(bytes.end(),value.begin(),value.end());}
    void Text(const std::string& value){Blob(std::as_bytes(std::span(value)));}
};
class CacheDigest {
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    static void Check(NTSTATUS status){if(status<0)throw std::runtime_error("Native cache SHA256 failed");}
public:
    CacheDigest(){Check(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0));
        const auto status=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0);
        if(status<0){BCryptCloseAlgorithmProvider(algorithm,0);algorithm=nullptr;Check(status);}}
    ~CacheDigest(){if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);}
    void Update(std::span<const std::byte> bytes){
        while(!bytes.empty()){const auto count=ULONG(std::min<size_t>(bytes.size(),1024*1024));
            Check(BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<std::byte*>(bytes.data())),count,0));bytes=bytes.subspan(count);}}
    std::array<std::byte,32> Finish(){std::array<std::byte,32> result{};Check(BCryptFinishHash(hash,reinterpret_cast<PUCHAR>(result.data()),32,0));return result;}
    static auto Of(std::span<const std::byte> bytes){CacheDigest digest;digest.Update(bytes);return digest.Finish();}
    static auto File(const std::filesystem::path& path){
        std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Cannot fingerprint native compiler DLL");
        CacheDigest digest;std::array<char,65536> block{};
        while(file.read(block.data(),block.size())||file.gcount())digest.Update(std::as_bytes(std::span(block.data(),size_t(file.gcount()))));
        if(!file.eof())throw std::runtime_error("Cannot read native compiler DLL fingerprint");return digest.Finish();
    }
};
class PersistentCache {
    std::filesystem::path directory;
    inline static std::atomic<uint64_t> next_temp{};
    std::filesystem::path Path(std::span<const std::byte> key)const{
        const auto digest=CacheDigest::Of(key);constexpr char hex[]="0123456789abcdef";std::string name;
        for(auto value:digest){auto byte=std::to_integer<unsigned>(value);name+=hex[byte>>4];name+=hex[byte&15];}
        return directory/(name+".bin");
    }
public:
    explicit PersistentCache(std::filesystem::path path):directory(std::move(path)){}
    bool Load(std::span<const std::byte> key,std::vector<std::byte>& result)const{
        if(directory.empty())return false;
        try{
            std::ifstream file(Path(key),std::ios::binary|std::ios::ate);if(!file)return false;
            const auto total=file.tellg();file.seekg(0);char magic[8]{};uint64_t keys{},data{};std::array<std::byte,32> digest{};
            file.read(magic,8);file.read(reinterpret_cast<char*>(&keys),8);file.read(reinterpret_cast<char*>(&data),8);file.read(reinterpret_cast<char*>(digest.data()),32);
            if(!file||memcmp(magic,"NDXC001",8)||keys!=key.size()||keys>16*1024*1024||data>64*1024*1024||uint64_t(total)!=56+keys+data)return false;
            std::vector<std::byte> stored_key(size_t(keys),std::byte{}),stored(size_t(data),std::byte{});
            file.read(reinterpret_cast<char*>(stored_key.data()),keys);file.read(reinterpret_cast<char*>(stored.data()),data);
            if(!file||!std::equal(key.begin(),key.end(),stored_key.begin())||CacheDigest::Of(stored)!=digest)return false;
            result=std::move(stored);return true;
        }catch(const std::exception&){return false;}
    }
    void Store(std::span<const std::byte> key,std::span<const std::byte> data)const{
        if(directory.empty()||key.size()>16*1024*1024||data.size()>64*1024*1024)return;
        std::filesystem::path temporary;
        try{
            std::filesystem::create_directories(directory);const auto final=Path(key);temporary=final;
            temporary+=L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(next_temp.fetch_add(1));
            {std::ofstream file(temporary,std::ios::binary|std::ios::trunc);const uint64_t keys=key.size(),size=data.size();const auto digest=CacheDigest::Of(data);
                file.write("NDXC001",8);file.write(reinterpret_cast<const char*>(&keys),8);file.write(reinterpret_cast<const char*>(&size),8);
                file.write(reinterpret_cast<const char*>(digest.data()),32);file.write(reinterpret_cast<const char*>(key.data()),keys);file.write(reinterpret_cast<const char*>(data.data()),size);
                file.flush();if(!file)throw std::runtime_error("Cannot persist native cache");}
            if(!MoveFileExW(temporary.c_str(),final.c_str(),MOVEFILE_REPLACE_EXISTING))throw std::runtime_error("Cannot commit native cache");
        }catch(const std::exception&){if(!temporary.empty()){std::error_code error;std::filesystem::remove(temporary,error);}}
    }
};
}
