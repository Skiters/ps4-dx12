// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>
#include <vector>

#include "common/types.h"
#include "imgui/imgui_texture.h"

struct ID3D12Fence;

namespace ImGui::Core::TextureManager {

struct Inner;

void StartWorker(void (*update_texture)(ImTextureData*)=nullptr);

void StopWorker();

void DecodePngTexture(std::vector<u8> data, Inner* core);

void DecodePngFile(std::filesystem::path path, Inner* core);

void Submit(ID3D12Fence* fence=nullptr,u64 value=0);
void Shutdown(ID3D12Fence* fence,u64 value);

}; // namespace ImGui::Core::TextureManager
