#pragma once

#include "resources.h"
#include "common/types.h"
#include <map>
#include <mutex>

namespace NativeDX12 {
class BufferBudgetExhausted : public std::runtime_error {
public:
    BufferBudgetExhausted():std::runtime_error("D3D12 buffer arena residency budget exhausted"){}
};

// Buffer heap arena following D3D12 placed-resource alignment and residency
// rules.  Each lease returns its range when the last Resource copy retires.
class PlacedBufferAllocator {
    struct Page;
    struct Lease {
        std::shared_ptr<Page> page;
        u64 offset{};
        u64 size{};
        ~Lease();
    };
    struct Page {
        ComPtr<ID3D12Heap> heap;
        u64 size{};
        std::map<u64,u64> free_ranges;
        std::mutex mutex;
        void Release(u64,u64);
    };

    ComPtr<ID3D12Device> device_;
    std::vector<std::shared_ptr<Page>> pages_;
    std::mutex mutex_;
    u64 page_size_;
    u64 committed_{};
    u64 budget_;
    bool compact_large_pages_{};

    std::shared_ptr<Page> CreatePage(u64 minimum);

public:
    explicit PlacedBufferAllocator(ID3D12Device*,u64 page_size=256ull*1024*1024,
                                   u64 budget=1024ull*1024*1024);
    Resource Allocate(u64 size,D3D12_RESOURCE_FLAGS flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                      D3D12_RESOURCE_STATES initial=D3D12_RESOURCE_STATE_COMMON);
    void SetBudget(u64 bytes);
    void Trim(); // Releases only pages with no resource leases.
    void SetCompactLargePages(bool enabled){compact_large_pages_=enabled;}
    u64 CommittedBytes() const { return committed_; }
};

// Texture heaps must be separated into RT/DS and non-RT/DS classes.  Pages
// remain resident while small image lifetimes reuse their returned ranges.
class PlacedTextureAllocator {
    struct Page;
    struct Lease {std::shared_ptr<Page> page;u64 offset{},size{};~Lease();};
    struct Page {
        ComPtr<ID3D12Heap> heap;u64 size{};D3D12_HEAP_FLAGS flags{};
        std::map<u64,u64> free_ranges;std::mutex mutex;void Release(u64,u64);
    };
    ComPtr<ID3D12Device> device_;
    std::vector<std::shared_ptr<Page>> pages_;
    std::mutex mutex_;
    u64 page_size_,committed_{},budget_;
    std::shared_ptr<Page> CreatePage(u64,u64,D3D12_HEAP_FLAGS);
public:
    explicit PlacedTextureAllocator(ID3D12Device*,u64 page_size=256ull*1024*1024,
        u64 budget=1536ull*1024*1024);
    Resource Allocate(const D3D12_RESOURCE_DESC&,D3D12_RESOURCE_STATES,const D3D12_CLEAR_VALUE* = nullptr);
    void SetBudget(u64 bytes);
    u64 CommittedBytes()const{return committed_;}
};

} // namespace NativeDX12
