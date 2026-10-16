// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <d3d12.h>
#include <array>
#include <cstddef>
namespace NativeDX12 {
inline constexpr UINT NvClockRegister=1,NvClockSpace=29;
inline bool IsNvShaderClockBinding(D3D12_DESCRIPTOR_RANGE_TYPE type,UINT reg,UINT space,UINT count){
    return type==D3D12_DESCRIPTOR_RANGE_TYPE_UAV&&reg==NvClockRegister&&space==NvClockSpace&&count==1;
}
bool NvShaderClockAvailable(ID3D12Device*);
std::array<std::byte,32> NvShaderClockRuntimeDigest();
class NvShaderClockScope {
    ID3D12Device* device_{};
public:
    NvShaderClockScope(ID3D12Device*,bool enable);
    ~NvShaderClockScope();
    NvShaderClockScope(const NvShaderClockScope&)=delete;
    NvShaderClockScope& operator=(const NvShaderClockScope&)=delete;
    void Finish();
};
}
