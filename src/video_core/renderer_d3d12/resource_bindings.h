// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "resources.h"
#include <span>
#include <string>
#include <functional>
namespace NativeDX12 {
// Reuse driver allocations only after the caller retires their GPU fence.
// Unlike Vulkan descriptor pools, D3D12 heaps are shader-visible objects.
class DescriptorHeapPool {
public:
    struct Page {ComPtr<ID3D12DescriptorHeap> heap;D3D12_DESCRIPTOR_HEAP_TYPE type{};UINT capacity{},cursor{},active{};};
    struct Slice {std::shared_ptr<Page> page;UINT offset{},count{};explicit operator bool()const{return bool(page);}};
private:
    ComPtr<ID3D12Device> device_;
    std::vector<std::shared_ptr<Page>> pages_;
    uint64_t created_{},reused_{},released_{},discarded_{};
public:
    explicit DescriptorHeapPool(ID3D12Device* device):device_(device){}
    Slice Acquire(UINT,D3D12_DESCRIPTOR_HEAP_TYPE);
    void Release(Slice&);
    ID3D12Device* Device()const{return device_.Get();}
    struct Statistics {uint64_t created,reused,released,discarded;size_t available;};
    Statistics Stats()const{return {created_,reused_,released_,discarded_,pages_.size()};}
};
struct ShaderBinding {
    D3D12_DESCRIPTOR_RANGE_TYPE type{};
    UINT reg{},space{},count{},offset{};
};
// Reflect actual DXIL resource registers, including Mesa's runtime/push CBVs.
// No guessed register layout and no Vulkan descriptor objects.
class ResourceLayout {
    ComPtr<ID3D12RootSignature> root_;
    std::vector<ShaderBinding> bindings_;
    UINT resource_count_{},sampler_count_{};
    UINT resource_parameter_=UINT_MAX,sampler_parameter_=UINT_MAX;
    D3D12_RESOURCE_BINDING_TIER binding_tier_{};
    friend class ResourceBindings;
public:
    ResourceLayout(ID3D12Device*,const std::wstring& dxc_dll,
        std::span<const std::span<const uint8_t>> shader_bytecodes);
    ResourceLayout(const ResourceLayout&)=delete;
    ResourceLayout& operator=(const ResourceLayout&)=delete;
    ID3D12RootSignature* Root()const{return root_.Get();}
    const std::vector<ShaderBinding>& Bindings()const{return bindings_;}
};
// Immutable after first binding. Keep this object and its layout alive until the
// submission fence completes; owned COM references retain bound GPU resources.
class ResourceBindings {
    ComPtr<ID3D12Device> device_;
    const ResourceLayout& layout_;
    DescriptorHeapPool::Slice resources_,samplers_;
    ComPtr<ID3D12DescriptorHeap> owned_resources_,owned_samplers_;
    std::vector<bool> written_resources_,written_samplers_;
    std::vector<ComPtr<ID3D12Resource>> keep_alive_;
    std::vector<std::shared_ptr<void>> allocation_leases_;
    bool sealed_{};
    bool recycled_{};
    DescriptorHeapPool* pool_{};
    std::function<void(ResourceBindings&)> before_bind_;
    D3D12_CPU_DESCRIPTOR_HANDLE Slot(D3D12_DESCRIPTOR_RANGE_TYPE,UINT,UINT,UINT);
public:
    void Retain(ID3D12Resource*);
    void RetainAllocation(std::shared_ptr<void> lease){if(lease)allocation_leases_.push_back(std::move(lease));}
    void BeforeBind(std::function<void(ResourceBindings&)> prepare){
        if(sealed_||recycled_||before_bind_)throw std::logic_error("Invalid binding preparation");
        before_bind_=std::move(prepare);
    }
public:
    ResourceBindings(ID3D12Device*,const ResourceLayout&,DescriptorHeapPool* = nullptr);
    // Explicit retirement, never automatic destructor recycling: callers can
    // also record bindings without waiting for the GPU.
    void RecycleRetiredHeaps();
    ResourceBindings(const ResourceBindings&)=delete;
    ResourceBindings& operator=(const ResourceBindings&)=delete;
    const ResourceLayout& Layout()const{return layout_;}
    bool References(ID3D12Resource* resource)const {
        for(const auto& held:keep_alive_)if(held.Get()==resource)return true;
        return false;
    }
    void Srv(UINT reg,UINT space,ID3D12Resource*,const D3D12_SHADER_RESOURCE_VIEW_DESC&,UINT index=0);
    void Uav(UINT reg,UINT space,ID3D12Resource*,const D3D12_UNORDERED_ACCESS_VIEW_DESC&,UINT index=0);
    // Tier 3 only: the shader must guard every descriptor index against the
    // populated prefix. Used by DMA's GPU-visible active-bank count contract.
    void UnreachableUavTail(UINT reg,UINT space,UINT first);
    void Cbv(UINT reg,UINT space,ID3D12Resource*,UINT64 offset,UINT bytes,UINT index=0);
    void Sampler(UINT reg,UINT space,const D3D12_SAMPLER_DESC&,UINT index=0);
    void Bind(ID3D12GraphicsCommandList*,bool compute);
};
}
