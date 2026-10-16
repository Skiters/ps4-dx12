// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "video_core/renderer_d3d12/rasterizer.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace NativeDX12 {

// Authored only: hardware execution is a separate acceptance step.
inline void RunTextureHeapPressureFixture(ID3D12Device* device) {
    constexpr u64 Page = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    constexpr u64 MsaaPage = D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT;
    constexpr u64 DefaultPage = 256ull * 1024 * 1024;
    if (!device) throw std::invalid_argument("Texture heap fixture needs a hardware device");
    Rasterizer rasterizer(device);
    const auto expect_budget = [](auto&& action) {
        try { action(); } catch (const TextureBudgetExhausted&) { return; }
        throw std::runtime_error("Texture heap pressure did not report typed budget exhaustion");
    };
    const auto expect_invalid = [](auto&& action) {
        try { action(); }
        catch (const TextureBudgetExhausted&) {
            throw std::runtime_error("Invalid texture input was mislabeled as budget pressure");
        } catch (const std::invalid_argument&) { return; }
        throw std::runtime_error("Invalid texture input was accepted");
    };
    D3D12_RESOURCE_DESC plain{};
    plain.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    plain.Width = plain.Height = 128;
    plain.DepthOrArraySize = plain.MipLevels = 1;
    plain.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    plain.SampleDesc.Count = 1;
    auto rt = plain;
    rt.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    const auto check_class = [](const Resource& resource, D3D12_HEAP_FLAGS expected) {
        D3D12_HEAP_PROPERTIES properties{}; D3D12_HEAP_FLAGS flags{};
        Check(resource.handle->GetHeapProperties(&properties, &flags));
        const auto classes = D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES |
            D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES;
        if (properties.Type != D3D12_HEAP_TYPE_DEFAULT || (flags & classes) != expected)
            throw std::runtime_error("RT/DS and non-RT/DS texture heap classes were mixed");
    };
    const auto pattern = [](u32 x, u32 y) { return 0x6a3100ffu ^ (x << 8) ^ (y << 16); };
    const auto write_pattern = [&](Resource& texture) {
        const auto desc = texture.handle->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT rows{}; UINT64 row_bytes{}, bytes{};
        device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &rows, &row_bytes, &bytes);
        if (rows != plain.Height || row_bytes != plain.Width * 4 || !bytes)
            throw std::runtime_error("Texture heap fixture lost its simple RGBA8 footprint");
        auto upload = Buffer(device, bytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        void* mapped{}; D3D12_RANGE empty{};
        Check(upload.handle->Map(0, &empty, &mapped));
        std::memset(mapped, 0xcd, size_t(bytes));
        for (u32 y = 0; y < plain.Height; ++y) for (u32 x = 0; x < plain.Width; ++x) {
            const auto value = pattern(x, y);
            std::memcpy(static_cast<std::byte*>(mapped) + footprint.Offset +
                u64(y) * footprint.Footprint.RowPitch + u64(x) * 4, &value, 4);
        }
        D3D12_RANGE written{0, SIZE_T(bytes)};
        upload.handle->Unmap(0, &written);
        // Retain both placed backing and upload before recording any command.
        rasterizer.RetainUntilFinish([upload, texture](u64) { (void)upload; (void)texture; });
        auto* list = rasterizer.RecordingList();
        D3D12_RESOURCE_BARRIER alias{}; alias.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
        alias.Aliasing.pResourceAfter = texture.handle.Get();
        list->ResourceBarrier(1, &alias);
        texture.Transition(list, D3D12_RESOURCE_STATE_COPY_DEST);
        D3D12_TEXTURE_COPY_LOCATION source{}, destination{};
        source.pResource = upload.handle.Get(); source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        source.PlacedFootprint = footprint;
        destination.pResource = texture.handle.Get(); destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        texture.Transition(list, D3D12_RESOURCE_STATE_COPY_SOURCE);
        rasterizer.Finish();
    };
    const auto read_pattern = [&](Resource& texture) {
        const auto desc = texture.handle->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT rows{}; UINT64 row_bytes{}, bytes{};
        device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &rows, &row_bytes, &bytes);
        auto readback = Buffer(device, bytes, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
        rasterizer.RetainUntilFinish([readback, texture](u64) { (void)readback; (void)texture; });
        auto* list = rasterizer.RecordingList();
        texture.Transition(list, D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION source{}, destination{};
        source.pResource = texture.handle.Get(); source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destination.pResource = readback.handle.Get(); destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        destination.PlacedFootprint = footprint;
        list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        rasterizer.Finish();
        void* mapped{}; D3D12_RANGE range{0, SIZE_T(bytes)};
        Check(readback.handle->Map(0, &range, &mapped));
        bool matches = true;
        for (u32 y = 0; y < plain.Height; ++y) for (u32 x = 0; x < plain.Width; ++x) {
            u32 value{};
            std::memcpy(&value, static_cast<const std::byte*>(mapped) + footprint.Offset +
                u64(y) * footprint.Footprint.RowPitch + u64(x) * 4, 4);
            matches &= value == pattern(x, y);
        }
        D3D12_RANGE empty{}; readback.handle->Unmap(0, &empty);
        if (!matches) throw std::runtime_error("Texture contents did not survive reduced budget and Resource copy");
    };
    {
        PlacedTextureAllocator textures(device);
        const auto info = textures.AllocationInfo(plain);
        const auto rt_info = textures.AllocationInfo(rt);
        if (info.SizeInBytes != Page || info.Alignment != Page ||
            rt_info.SizeInBytes != Page || rt_info.Alignment != Page)
            throw std::runtime_error("Texture heap fixture requires ordinary 64-KiB RGBA8 placement");
        auto original = textures.Allocate(plain, D3D12_RESOURCE_STATE_COMMON);
        check_class(original, D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES);
        if (textures.CommittedBytes() != DefaultPage)
            throw std::runtime_error("Texture heap fixture did not establish the default 256-MiB page");
        write_pattern(original);
        auto copy = original;
        original = {};
        textures.SetBudget(Page);
        expect_budget([&] { auto resource = textures.Allocate(rt, D3D12_RESOURCE_STATE_COMMON); });
        if (textures.CommittedBytes() != DefaultPage)
            throw std::runtime_error("Reduced budget discarded a live Resource lease");
        read_pattern(copy); // No rewrite after shrink: this reads the original GPU data.
        auto standalone_lease = copy.allocation;
        copy = {};
        expect_budget([&] { auto resource = textures.Allocate(rt, D3D12_RESOURCE_STATE_COMMON); });
        if (textures.CommittedBytes() != DefaultPage)
            throw std::runtime_error("Standalone allocation lease failed to retain its heap");
        standalone_lease.reset();
        textures.SetBudget(Page);
        // Observe eager retirement BEFORE any allocation can invoke CreatePage
        // and hide an idle oversized heap behind that later slow-path trim.
        if (textures.CommittedBytes() > Page)
            throw std::runtime_error("SetBudget retained an idle oversized texture heap");
        auto compact = textures.Allocate(rt, D3D12_RESOURCE_STATE_COMMON);
        check_class(compact, D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES);
        if (textures.CommittedBytes() != Page)
            throw std::runtime_error("Released default heap did not fit the reduced one-page budget");
    }
    {
        PlacedTextureAllocator textures(device);
        textures.SetBudget(2 * Page);
        auto non_rt = textures.Allocate(plain, D3D12_RESOURCE_STATE_COMMON);
        check_class(non_rt, D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES);
        const auto first_commit = textures.CommittedBytes();
        if (first_commit != 2 * Page)
            throw std::runtime_error("Reduced texture budget still demanded a default-sized page");
        // The first live class has a spare resource-sized range, but cannot host RT.
        expect_budget([&] { auto resource = textures.Allocate(rt, D3D12_RESOURCE_STATE_COMMON); });
        textures.SetBudget(first_commit + Page);
        auto render_target = textures.Allocate(rt, D3D12_RESOURCE_STATE_COMMON);
        check_class(render_target, D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES);
        if (textures.CommittedBytes() != first_commit + Page)
            throw std::runtime_error("New texture heap did not fit only the remaining budget");
    }
    {
        PlacedTextureAllocator textures(device);
        textures.SetBudget(0);
        expect_budget([&] { auto resource = textures.Allocate(plain, D3D12_RESOURCE_STATE_COMMON); });
        auto invalid = plain; invalid.Width = 0;
        expect_invalid([&] { auto resource = textures.Allocate(invalid, D3D12_RESOURCE_STATE_COMMON); });
        if (textures.CommittedBytes()) throw std::runtime_error("Zero-budget or invalid request committed a heap");
        if (!textures.SupportsFormatCasting()) {
            bool rejected = false;
            const std::array<DXGI_FORMAT, 1> formats{plain.Format};
            try { (void)textures.AllocationInfo(plain, formats); }
            catch (const TextureBudgetExhausted&) {
                throw std::runtime_error("Unsupported format casting was mislabeled as budget pressure");
            } catch (const std::runtime_error&) { rejected = true; }
            if (!rejected) throw std::runtime_error("Unsupported format-list request was accepted");
        } else std::puts("SKIP: unsupported-cast error branch; format-list allocation is supported");
    }
    bool small_msaa_exercised = false;
    D3D12_FEATURE_DATA_FORMAT_SUPPORT support{plain.Format};
    Check(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support)));
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS quality{plain.Format, 4,
        D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE, 0};
    Check(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &quality, sizeof(quality)));
    if ((support.Support1 & D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RENDERTARGET) && quality.NumQualityLevels) {
        auto msaa = rt;
        msaa.Width = msaa.Height = 16;
        msaa.SampleDesc.Count = 4;
        msaa.Alignment = D3D12_SMALL_MSAA_RESOURCE_PLACEMENT_ALIGNMENT;
        PlacedTextureAllocator textures(device);
        const auto info = textures.AllocationInfo(msaa);
        if (info.SizeInBytes == Page && info.Alignment == Page) {
            textures.SetBudget(2 * Page);
            auto single = textures.Allocate(rt, D3D12_RESOURCE_STATE_COMMON);
            const auto before_msaa = textures.CommittedBytes();
            textures.SetBudget(before_msaa + MsaaPage);
            // A spare 64-KiB range exists in the live RT heap, whose alignment
            // cannot serve MSAA even though this resource's placement is 64 KiB.
            auto multisample = textures.Allocate(msaa, D3D12_RESOURCE_STATE_RENDER_TARGET);
            check_class(multisample, D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES);
            if (before_msaa != 2 * Page || textures.CommittedBytes() != before_msaa + MsaaPage)
                throw std::runtime_error("Small MSAA texture did not acquire a separate 4-MiB-aligned heap");
            small_msaa_exercised = true;
        } else std::printf("SKIP: small MSAA placement is not 64 KiB (size=%llu alignment=%llu)\n",
            static_cast<unsigned long long>(info.SizeInBytes), static_cast<unsigned long long>(info.Alignment));
    } else std::puts("SKIP: RGBA8 4x MSAA render targets are unsupported");
    rasterizer.Finish();
    rasterizer.ValidateDebugMessages();
    std::printf("PASS: texture-heaps; default/shrunken/remaining budgets, live Resource/shared leases, RT classes, exact GPU texels; small-MSAA64KiB=%s\n",
        small_msaa_exercised ? "covered" : "device-conditional skip");
}
} // namespace NativeDX12
