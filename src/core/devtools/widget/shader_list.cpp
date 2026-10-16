//  SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
//  SPDX-License-Identifier: GPL-2.0-or-later

#include <fstream>

#include "shader_list.h"

#include <imgui.h>

#include "common.h"
#include "common/path_util.h"
#include "common/string_util.h"
#include "common/logging/log.h"
#include "core/debug_state.h"
#include "core/devtools/options.h"
#include "core/emulator_settings.h"
#include "imgui/imgui_std.h"
#include "sdl_window.h"
#include "video_core/renderer_d3d12/backend_factory.h"
#include "video_core/renderer_d3d12/guest_rasterizer.h"
#include "video_core/amdgpu/liverpool.h"

extern std::unique_ptr<NativeDX12::Backend> native_backend;
extern std::unique_ptr<AmdGpu::Liverpool> liverpool;
extern Frontend::WindowSDL* g_window;

using namespace ImGui;

namespace Core::Devtools::Widget {

ShaderList::Selection::Selection(int index)
    : index(index), isa_editor(std::make_unique<TextEditor>()),
      glsl_editor(std::make_unique<TextEditor>()) {
    isa_editor->SetPalette(TextEditor::GetDarkPalette());
    isa_editor->SetReadOnly(true);
    glsl_editor->SetPalette(TextEditor::GetDarkPalette());
    glsl_editor->SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
    if(g_window)g_window->RequestKeyboard();
}

ShaderList::Selection::~Selection() {
    if (index >= 0) {
        if(g_window)g_window->ReleaseKeyboard();
    }
}

ShaderList::Selection::Selection(Selection&& other) noexcept
    : index{other.index}, isa_editor{std::move(other.isa_editor)},
      glsl_editor{std::move(other.glsl_editor)}, open{other.open}, showing_bin{other.showing_bin},
      replacement{std::move(other.replacement)},applied_patch{other.applied_patch},requested_patch{other.requested_patch},
      patch_path{std::move(other.patch_path)}, patch_bin_path{std::move(other.patch_bin_path)} {
    other.index = -1;
}

ShaderList::Selection& ShaderList::Selection::operator=(Selection other) {
    using std::swap;
    swap(index,other.index);swap(isa_editor,other.isa_editor);swap(glsl_editor,other.glsl_editor);
    swap(open,other.open);swap(showing_bin,other.showing_bin);swap(replacement,other.replacement);
    swap(applied_patch,other.applied_patch);swap(requested_patch,other.requested_patch);
    swap(patch_path,other.patch_path);swap(patch_bin_path,other.patch_bin_path);
    return *this;
}

void ShaderList::Selection::ReloadShader(DebugStateType::ShaderDump& value) {
    auto& spv = value.is_patched ? value.patch_spv : value.spv;
    if (spv.empty()) {
        return;
    }
    if(replacement.valid())return;
    if(!native_backend||!native_backend->rasterizer||!liverpool){
        value.is_patched=applied_patch;value.loaded_data=false;
        DebugState.ShowDebugMessage("Shader replacement failed:\nNative shader renderer is not initialized");
        return;
    }
    auto result=std::make_shared<std::promise<std::optional<u64>>>();replacement=result->get_future();
    requested_patch=value.is_patched;
    try{liverpool->SendCommand([result,id=value.module,patched=value.is_patched,previous=applied_patch,words=std::vector<u32>(spv)]{
        try{
            if(!native_backend||!native_backend->rasterizer)throw std::runtime_error("Native shader renderer is no longer initialized");
            auto* rasterizer=static_cast<NativeDX12::GuestRasterizer*>(native_backend->rasterizer.get());
            const auto module=rasterizer->ReplaceShader(id,words,patched);
            if(!module)throw std::runtime_error("Shader is no longer loaded");
            DebugState.CompleteShaderReplacement(id,*module,patched);result->set_value(module);}
        catch(const std::exception& error){LOG_ERROR(Render_DX12,"Shader replacement failed: {}",error.what());
            DebugState.CompleteShaderReplacement(id,id,previous);result->set_exception(std::current_exception());}
        catch(...){LOG_ERROR(Render_DX12,"Shader replacement failed with an unknown exception");
            DebugState.CompleteShaderReplacement(id,id,previous);result->set_exception(std::current_exception());}
    });}catch(...){result->set_exception(std::current_exception());}
}

bool ShaderList::Selection::DrawShader(DebugStateType::ShaderDump& value) {
    if(replacement.valid()&&replacement.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        try{const auto module=replacement.get();if(!module)throw std::runtime_error("Shader is no longer loaded");
            value.module=*module;applied_patch=requested_patch;value.is_patched=applied_patch;}
        catch(const std::exception& error){value.is_patched=applied_patch;value.loaded_data=false;
            DebugState.ShowDebugMessage(std::string("Shader replacement failed:\n")+error.what());}
    }
    if (!value.loaded_data) {
        value.loaded_data = true;
        if (value.cache_isa_disasm.empty()) {
            value.cache_isa_disasm = RunDisassembler(Options.disassembler_cli_isa, value.isa);
        }
        if (value.cache_spv_disasm.empty()) {
            value.cache_spv_disasm = RunDisassembler(Options.disassembler_cli_spv, value.spv);
        }
        if (!value.patch_spv.empty() && value.cache_patch_disasm.empty()) {
            value.cache_patch_disasm = RunDisassembler("spirv-dis {src}", value.patch_spv);
        }
        patch_path =
            Common::FS::GetUserPath(Common::FS::PathType::ShaderDir) / "patch" / value.name;
        patch_bin_path = patch_path;
        patch_bin_path += ".spv";
        patch_path += ".glsl";
        if (std::filesystem::exists(patch_path)) {
            std::ifstream file{patch_path};
            value.patch_source =
                std::string{std::istreambuf_iterator{file}, std::istreambuf_iterator<char>{}};
        }

        applied_patch=value.is_patched;
        if (!value.is_patched) { // No patch
            isa_editor->SetText(value.cache_isa_disasm);
            glsl_editor->SetText(value.cache_spv_disasm);
        } else {
            isa_editor->SetText(value.cache_patch_disasm);
            isa_editor->SetLanguageDefinition(TextEditor::LanguageDefinition::SPIRV());
            glsl_editor->SetText(value.patch_source);
            glsl_editor->SetReadOnly(false);
        }
    }

    char name[64];
    snprintf(name, sizeof(name), "Shader %s", value.name.c_str());
    SetNextWindowSize({450.0f, 600.0f}, ImGuiCond_FirstUseEver);
    if (!Begin(name, &open, ImGuiWindowFlags_NoNav)) {
        End();
        return open;
    }

    Text("%s", value.name.c_str());
    SameLine(0.0f, 7.0f);
    BeginDisabled(replacement.valid());
    if (Checkbox("Enable patch", &value.is_patched)) {
        if (value.is_patched) {
            if (value.patch_source.empty()) {
                value.patch_source = value.cache_spv_disasm;
            }
            isa_editor->SetText(value.cache_patch_disasm);
            isa_editor->SetLanguageDefinition(TextEditor::LanguageDefinition::SPIRV());
            glsl_editor->SetText(value.patch_source);
            glsl_editor->SetReadOnly(false);
            if (!value.patch_spv.empty()) {
                ReloadShader(value);
            }
        } else {
            isa_editor->SetText(value.cache_isa_disasm);
            isa_editor->SetLanguageDefinition(TextEditor::LanguageDefinition());
            glsl_editor->SetText(value.cache_spv_disasm);
            glsl_editor->SetReadOnly(true);
            ReloadShader(value);
        }
    }
    SameLine();
    if (Button("Copy name")) {
        SetClipboardText(value.name.c_str());
    }

    if (value.is_patched) {
        if (BeginCombo("Shader type", showing_bin ? "SPIRV" : "GLSL",
                       ImGuiComboFlags_WidthFitPreview)) {
            if (Selectable("GLSL")) {
                showing_bin = false;
            }
            if (Selectable("SPIRV")) {
                showing_bin = true;
            }
            EndCombo();
        }
    } else {
        if (BeginCombo("Shader type", showing_bin ? "ISA" : "GLSL",
                       ImGuiComboFlags_WidthFitPreview)) {
            if (Selectable("GLSL")) {
                showing_bin = false;
            }
            if (Selectable("ISA")) {
                showing_bin = true;
            }
            EndCombo();
        }
    }

    if (value.is_patched) {
        bool save = false;
        bool compile = false;
        SameLine(0.0f, 3.0f);
        if (Button("Save")) {
            save = true;
        }
        SameLine();
        if (Button("Save & Compile")) {
            save = true;
            compile = true;
        }
        if (save) {
            value.patch_source = glsl_editor->GetText();
            std::ofstream file{patch_path, std::ios::binary | std::ios::trunc};
            file << value.patch_source;
            std::string msg = "Patch saved to ";
            msg += Common::U8stringToString(patch_path.u8string());
            DebugState.ShowDebugMessage(msg);
        }
        if (compile) {
            static std::map<Shader::SwStage, std::string> stage_arg = {
                {Shader::SwStage::Vertex, "vert"},
                {Shader::SwStage::TessellationControl, "tesc"},
                {Shader::SwStage::TessellationEval, "tese"},
                {Shader::SwStage::Geometry, "geom"},
                {Shader::SwStage::Fragment, "frag"},
                {Shader::SwStage::Compute, "comp"},
            };
            auto stage = stage_arg.find(value.l_stage);
            if (stage == stage_arg.end()) {
                DebugState.ShowDebugMessage(std::string{"Invalid shader stage"});
            } else {
                std::string cmd =
                    fmt::format("glslc --target-env=vulkan1.3 --target-spv=spv1.6 "
                                "-fshader-stage={} {{src}} -o \"{}\"",
                                stage->second, Common::U8stringToString(patch_bin_path.u8string()));
                bool success = false;
                auto res = RunDisassembler(cmd, value.patch_source, &success);
                if (!res.empty() || !success) {
                    DebugState.ShowDebugMessage("Compilation failed:\n" + res);
                } else {
                    Common::FS::IOFile file{patch_bin_path, Common::FS::FileAccessMode::Read};
                    value.patch_spv.resize(file.GetSize() / sizeof(u32));
                    file.Read(value.patch_spv);
                    value.cache_patch_disasm =
                        RunDisassembler("spirv-dis {src}", value.patch_spv, &success);
                    if (!success) {
                        DebugState.ShowDebugMessage("Decompilation failed (Compile was ok):\n" +
                                                    res);
                    } else {
                        isa_editor->SetText(value.cache_patch_disasm);
                        ReloadShader(value);
                    }
                }
            }
        }
    }

    EndDisabled();
    if (showing_bin) {
        isa_editor->Render(value.is_patched ? "SPIRV" : "ISA", GetContentRegionAvail());
    } else {
        glsl_editor->Render("GLSL", GetContentRegionAvail());
    }

    End();
    return open;
}

void ShaderList::Draw() {
    std::lock_guard lock(DebugState.shader_dump_mutex);
    for (auto it = open_shaders.begin(); it != open_shaders.end();) {
        auto& selection = *it;
        auto& shader = DebugState.shader_dump_list[selection.index];
        if (!selection.DrawShader(shader)) {
            it = open_shaders.erase(it);
        } else {
            ++it;
        }
    }

    SetNextWindowSize({500.0f, 600.0f}, ImGuiCond_FirstUseEver);
    if (!Begin("Shader list", &open)) {
        End();
        return;
    }

    if (!EmulatorSettings.IsShaderCollect()) {
        DrawCenteredText("Enable 'shader_collect' in config to see shaders");
        End();
        return;
    }

    InputTextEx("##search_shader", "Search by name", search_box, sizeof(search_box), {},
                ImGuiInputTextFlags_None);

    auto width = GetContentRegionAvail().x;
    int i = 0;
    for (const auto& shader : DebugState.shader_dump_list) {
        if (search_box[0] != '\0' && !shader.name.contains(search_box)) {
            i++;
            continue;
        }
        char name[128];
        if (shader.is_patched) {
            snprintf(name, sizeof(name), "%s (PATCH ON)", shader.name.c_str());
        } else if (!shader.patch_spv.empty()) {
            snprintf(name, sizeof(name), "%s (PATCH OFF)", shader.name.c_str());
        } else {
            snprintf(name, sizeof(name), "%s", shader.name.c_str());
        }
        if (ButtonEx(name, {width, 20.0f}, ImGuiButtonFlags_NoHoveredOnFocus)) {
            if (std::find_if(open_shaders.begin(), open_shaders.end(),
                             [i](auto& v) { return v.index == i; }) == open_shaders.end()) {
                open_shaders.emplace_back(i);
            }
        }
        i++;
    }

    End();
}

} // namespace Core::Devtools::Widget
