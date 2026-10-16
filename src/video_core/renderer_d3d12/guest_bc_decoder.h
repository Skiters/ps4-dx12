// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
namespace NativeDX12 {
class GuestBcDecoder {
    HMODULE module_{};
    using Decode=bool(*)(uint32_t,uint32_t,const uint8_t*,size_t,uint32_t,uint32_t,uint8_t*,size_t);
    Decode decode_{};
public:
    explicit GuestBcDecoder(const std::wstring& converter);
    ~GuestBcDecoder();
    GuestBcDecoder(const GuestBcDecoder&)=delete;
    GuestBcDecoder& operator=(const GuestBcDecoder&)=delete;
    std::vector<std::byte> Expand(uint32_t format,uint32_t number,uint32_t width,uint32_t height,
        std::span<const std::byte> blocks)const;
};
}
