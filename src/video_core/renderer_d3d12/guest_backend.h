// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "backend_factory.h"
namespace AmdGpu {struct Liverpool;}
namespace NativeDX12 {
std::unique_ptr<Backend> CreateGuestBackend(const BackendConfiguration&,AmdGpu::Liverpool&,
    std::wstring converter,std::wstring dxc);
}
