// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_shader_debug.h"
#include "common/path_util.h"
#include "common/io_file.h"
#include "core/debug_state.h"
#include "core/emulator_settings.h"
#include "shader_recompiler/shader_name.h"
#include <fmt/format.h>
namespace NativeDX12 {
namespace {
std::string Name(const GuestShaderStage& stage,size_t index){
    return Shader::GetShaderName(stage.info.hw_stage,stage.info.pgm_hash,index);
}
void Dump(std::span<const u32> code,const std::string& name,std::string_view extension){
    if(!EmulatorSettings.IsDumpShaders())return;
    const auto directory=Common::FS::GetUserPath(Common::FS::PathType::ShaderDir)/"dumps";
    std::filesystem::create_directories(directory);
    const Common::FS::IOFile file{directory/fmt::format("{}.{}",name,extension),Common::FS::FileAccessMode::Create};
    file.WriteSpan(code);
}
}
void LoadShaderPatch(GuestShaderStage& stage,size_t index){
    const auto directory=Common::FS::GetUserPath(Common::FS::PathType::ShaderDir)/"patch";
    std::filesystem::create_directories(directory);
    const auto path=directory/(Name(stage,index)+".spv");
    if(std::filesystem::exists(path)){
        const Common::FS::IOFile file{path,Common::FS::FileAccessMode::Read};
        const auto bytes=file.GetSize();
        if(EmulatorSettings.IsPatchShaders()&&(bytes<20||bytes%4))throw std::runtime_error("Malformed native shader patch "+path.string());
        stage.patch_spirv.resize(size_t(bytes/4));
        if(file.Read(stage.patch_spirv)!=stage.patch_spirv.size())throw std::runtime_error("Short native shader patch "+path.string());
        if(EmulatorSettings.IsPatchShaders()&&(stage.patch_spirv.empty()||stage.patch_spirv[0]!=0x07230203U))
            throw std::runtime_error("Invalid native shader patch "+path.string());
    }
    stage.is_patched=EmulatorSettings.IsPatchShaders()&&!stage.patch_spirv.empty();
}
void DumpShaderSource(const GuestShaderStage& stage,size_t index,std::span<const u32> code){Dump(code,Name(stage,index),"bin");}
void CollectShaderDebug(GuestShaderStage& stage,size_t index,std::span<const u32> code){
    if(stage.debug_collected)return;stage.debug_collected=true;
    const auto name=Name(stage,index);Dump(stage.spirv,name,"spv");
    if(EmulatorSettings.IsShaderCollect())DebugState.CollectShader(name,stage.info.sw_stage,
        stage.id,stage.spirv,code,stage.patch_spirv,stage.is_patched);
}
}
