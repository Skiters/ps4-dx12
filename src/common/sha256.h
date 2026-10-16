// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <span>
#include <stdexcept>
#pragma comment(lib,"bcrypt.lib")
namespace Common {
class Sha256 {
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    static void Check(NTSTATUS status){if(status<0)throw std::runtime_error("SHA256 failed");}
public:
    Sha256(){Check(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0));
        const auto status=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0);
        if(status<0){BCryptCloseAlgorithmProvider(algorithm,0);algorithm=nullptr;Check(status);}}
    Sha256(const Sha256&)=delete;
    Sha256& operator=(const Sha256&)=delete;
    ~Sha256(){if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);}
    void Update(std::span<const std::byte> bytes){
        while(!bytes.empty()){const auto count=ULONG(std::min<size_t>(bytes.size(),1024*1024));
            Check(BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<std::byte*>(bytes.data())),count,0));bytes=bytes.subspan(count);}}
    std::array<std::byte,32> Finish(){std::array<std::byte,32> result{};Check(BCryptFinishHash(hash,reinterpret_cast<PUCHAR>(result.data()),32,0));return result;}
    static auto Of(std::span<const std::byte> bytes){Sha256 digest;digest.Update(bytes);return digest.Finish();}
    static auto File(const std::filesystem::path& path){
        std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Cannot open SHA256 input");
        Sha256 digest;std::array<char,65536> block{};
        while(file.read(block.data(),block.size())||file.gcount())digest.Update(std::as_bytes(std::span(block.data(),size_t(file.gcount()))));
        if(!file.eof())throw std::runtime_error("Cannot read SHA256 input");return digest.Finish();
    }
};
}
