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
// Descriptor bank capacity is independent of resident byte size. Contiguous
// backing uses 64-MiB windows; only the actually used prefix is populated.
inline constexpr u32 BankCount=1u<<19;
inline constexpr u64 MaxRawViewBytes=u64(UINT32_MAX)*sizeof(u32);
inline constexpr u64 MaxBufferBytes=(MaxRawViewBytes-15)&~u64(3);
inline constexpr u32 DescriptorSet=32;
inline constexpr u32 PageSize=1u<<PageShift;
inline constexpr u32 SubpageSize=1u<<SubpageShift;
inline constexpr u32 SubpagesPerPage=PageSize/SubpageSize;
inline constexpr u32 BankSize=1u<<BankShift;
inline constexpr u64 AddressLimit=1ull<<GuestAddressBits;
inline constexpr u64 CacheBytes=u64(BankSize)*BankCount;
inline constexpr u32 PageCount=1u<<(GuestAddressBits-PageShift);
inline constexpr u64 PartialFlag=1ull<<63;
inline constexpr u32 InitialPartialPages=32;
inline constexpr u32 BankCountEntry=PageCount;
inline constexpr u32 PartialCapacityEntry=PageCount+1;
inline constexpr u32 SecondaryBase=PageCount+2;
inline constexpr u32 TableEntries=SecondaryBase+InitialPartialPages*SubpagesPerPage;
}
