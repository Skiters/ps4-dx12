// SPDX-License-Identifier: GPL-2.0-or-later
#include "emulator.h"
#include "common/path_util.h"
#include "common/elf_info.h"
#include <SDL3/SDL_init.h>
#include <array>
#include <cstdio>
#include <memory>
#include <string_view>

int main(int argc,char** argv){try{
    // Prove construction initializes cleanup state, rather than depending on
    // the allocator supplying zero-filled memory before Run initializes input.
    const auto profile=std::filesystem::current_path()/"isolated-user";
    std::filesystem::create_directories(profile);
    Common::FS::SetUserPath(Common::FS::PathType::UserDir,profile);
    const bool synthetic=argc==2&&std::string_view(argv[1])=="synthetic";
    if(argc!=1&&!synthetic)throw std::invalid_argument("Expected no arguments or synthetic");
    if(synthetic)Common::ElfInfo::Instance().InitializeApp("DX12FIXTURE", "Early shutdown fixture", "1.0",
        Common::ElfInfo::FW_900,Common::ElfInfo::FW_900,{});
    if(SDL_WasInit(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD))
        throw std::runtime_error("Early shutdown fixture must start without SDL input/video");
    alignas(Core::Emulator) std::array<std::byte,sizeof(Core::Emulator)> storage;
    storage.fill(std::byte{0xba});
    auto* emulator=std::construct_at(reinterpret_cast<Core::Emulator*>(storage.data()));
    emulator->Shutdown();
    emulator->Shutdown();
    std::destroy_at(emulator);
    if(SDL_WasInit(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD)||std::filesystem::exists(profile/"play_time.txt"))
        throw std::runtime_error("Early cleanup created game/input state");
    puts("PASS: poisoned-storage emulator construction and repeated pre-Run shutdown without input, game or play time");
    return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
