// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/types.h"
namespace Shader::NativeDma {
// Guest pointers are translated to cache offsets, never host GPU addresses.
// A zero entry is nonresident. Ordinary entries hold cache byte offset + 1;
// PartialFlag selects a secondary 4-KiB table slot (slot index + 1).
inline constexpr u32 GuestAddressBits=40;
inline constexpr u32 PageShift=16;
inline constexpr u32 SubpageShift=12;
inline constexpr u32 BankShift=26;
// One descriptor per host subpage covers the worst possible scattered backing
// in the current buffer batch. Only the actually used prefix is populated.
inline constexpr u64 ResidentBufferBytes=2ull*1024*1024*1024;
inline constexpr u32 BankCount=u32(ResidentBufferBytes>>SubpageShift);
inline constexpr u32 DescriptorSet=32;
inline constexpr u32 PageSize=1u<<PageShift;
inline constexpr u32 SubpageSize=1u<<SubpageShift;
inline constexpr u32 SubpagesPerPage=PageSize/SubpageSize;
inline constexpr u32 BankSize=1u<<BankShift;
inline constexpr u64 AddressLimit=1ull<<GuestAddressBits;
inline constexpr u64 CacheBytes=u64(BankSize)*BankCount;
inline constexpr u32 PageCount=1u<<(GuestAddressBits-PageShift);
inline constexpr u64 PartialFlag=1ull<<63;
inline constexpr u32 PartialPageCount=u32(ResidentBufferBytes>>SubpageShift);
inline constexpr u32 BankCountEntry=PageCount;
inline constexpr u32 SecondaryBase=PageCount+1;
inline constexpr u32 TableEntries=SecondaryBase+PartialPageCount*SubpagesPerPage;
}
