// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdlib>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
namespace NativeDX12::DisplayCapture {
inline std::atomic<uint64_t> next_start{};
inline bool Inputs(uint64_t id){const auto begin=next_start.load();return begin&&id>=begin&&id<begin+3;}
// Three consecutive snapshots only. Never enabled by ordinary diagnostics.
inline bool Selected(uint64_t id) {
    return std::getenv("SHADPS4_NATIVE_DISPLAY_CAPTURE") && ((id>=60 && id<63)||Inputs(id));
}
inline bool SelectSnapshot(uint64_t id){
    const auto* root=std::getenv("SHADPS4_NATIVE_DISPLAY_CAPTURE");
    if(root&&id>=63&&!next_start.load()&&std::filesystem::exists(std::filesystem::path(root)/"NEXT"))
        next_start.store(id);
    return Selected(id);
}
inline void Save(uint64_t id,const std::string& stage,std::span<const std::byte> bytes,
                 const std::string& metadata) {
    const auto* root=std::getenv("SHADPS4_NATIVE_DISPLAY_CAPTURE");
    if(!root||!Selected(id))return;
    const auto directory=std::filesystem::path(root);std::filesystem::create_directories(directory);
    const auto path=directory/(std::to_string(id)+"-"+stage);
    std::ofstream data(path.string()+".bin",std::ios::binary);
    data.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    std::ofstream info(path.string()+".txt");info<<metadata<<"\n";
    if(!data||!info)throw std::runtime_error("Cannot save bounded display capture");
}
}
