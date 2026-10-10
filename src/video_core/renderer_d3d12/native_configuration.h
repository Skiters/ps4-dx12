// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdlib>
#include <string_view>
namespace NativeDX12 {
inline bool Enabled() {
#ifdef SHADPS4_NATIVE_DX12_CORE
    return true;
#else
    return false;
#endif
}
}
