// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "guest_shader_frontend.h"
namespace NativeDX12 {
void LoadShaderPatch(GuestShaderStage&,size_t);
void DumpShaderSource(const GuestShaderStage&,size_t,std::span<const u32>);
void CollectShaderDebug(GuestShaderStage&,size_t,std::span<const u32>);
}
