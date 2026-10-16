// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "resources.h"
#include <dxgi.h>
#include <array>
namespace NativeDX12 {
inline constexpr UINT AmdClockRegister=0,AmdClockSpace=0x7fff0ade;
ComPtr<ID3D12Device> CreateAmdHardwareDevice(IDXGIAdapter*,std::shared_ptr<void>& lifetime);
bool AmdShaderClockAvailable(ID3D12Device*);
std::array<std::byte,32> AmdShaderClockRuntimeDigest(ID3D12Device*);
}
