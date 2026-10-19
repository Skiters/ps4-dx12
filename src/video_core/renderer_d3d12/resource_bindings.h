// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "resources.h"
#include "texture_resources.h"
#include <span>
#include <string>
#include <functional>
#include <unordered_map>
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
    bool nv_clock{};
    bool amd_clock{};
    D3D_SRV_DIMENSION dimension=D3D_SRV_DIMENSION_UNKNOWN;
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
    std::unordered_map<ID3D12Resource*,UINT> shader_planes_;
    struct ShaderTextureRange {UINT mip,mips,layer,layers,planes;SIZE_T descriptor{};
        D3D12_RESOURCE_STATES access=D3D12_RESOURCE_STATES(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);};
    std::unordered_map<ID3D12Resource*,std::vector<ShaderTextureRange>> shader_texture_ranges_;
    std::vector<std::shared_ptr<void>> allocation_leases_;
    struct BufferView {ID3D12Resource* resource;UINT reg,space,index;D3D12_DESCRIPTOR_RANGE_TYPE type;
        uint64_t offset,bytes;SIZE_T descriptor;};
    std::vector<BufferView> capture_buffers_;
    struct TextureView {Resource source;D3D12_SHADER_RESOURCE_VIEW_DESC view;UINT reg,space,index;SIZE_T descriptor;};
    std::vector<TextureView> capture_textures_;
    void TrackCaptureBuffer(BufferView);
    bool sealed_{};
    bool recycled_{};
    bool preparing_{},preparation_failed_{};
    DescriptorHeapPool* pool_{};
    std::function<void(ResourceBindings&)> before_bind_;
    D3D12_CPU_DESCRIPTOR_HANDLE Slot(D3D12_DESCRIPTOR_RANGE_TYPE,UINT,UINT,UINT);
    void TrackShaderTextureRange(ID3D12Resource*,ShaderTextureRange,SIZE_T);
public:
    void Retain(ID3D12Resource*);
    // Finish logical resource preparation before selecting IA/native pointers.
    // Idempotent; a failed preparation can never be bound partially.
    void Prepare();
    void RetainAllocation(std::shared_ptr<void> lease){if(lease)allocation_leases_.push_back(std::move(lease));}
    void BeforeBind(std::function<void(ResourceBindings&)> prepare){
        if(sealed_||recycled_||before_bind_||preparing_||preparation_failed_)throw std::logic_error("Invalid binding preparation");
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
    struct BufferCapture {ComPtr<ID3D12Resource> source;Resource readback;UINT reg,space,index;
        D3D12_DESCRIPTOR_RANGE_TYPE type;uint64_t offset,view_bytes,bytes;};
    // Diagnostic copies of the actual bound ranges. Caller supplies the state
    // of DEFAULT raw UAV buffers; UPLOAD CBVs stay in GENERIC_READ. Normal
    // idempotent preparation precedes copies; the copies do not resolve guest
    // ownership and restore source states. Retire with this table.
    std::vector<BufferCapture> CaptureBufferInputs(ID3D12GraphicsCommandList*,
        uint64_t per_buffer_bytes,uint64_t total_bytes,D3D12_RESOURCE_STATES raw_state);
    struct TextureCapture {Resource source;D3D12_SHADER_RESOURCE_VIEW_DESC view;UINT reg,space,index;
        TextureTransfer transfer;std::string omitted;};
    std::vector<TextureCapture> CaptureTextureInputs(ID3D12GraphicsCommandList*,uint64_t total_bytes);
    bool References(ID3D12Resource* resource)const {
        for(const auto& held:keep_alive_)if(held.Get()==resource)return true;
        return false;
    }
    UINT ShaderPlanes(ID3D12Resource* resource)const {
        const auto it=shader_planes_.find(resource);return it==shader_planes_.end()?0:it->second;
    }
    bool ShaderReferences(ID3D12Resource* resource,UINT mip,UINT layer,UINT plane)const {
        return ShaderAccess(resource,mip,layer,plane)!=D3D12_RESOURCE_STATE_COMMON;
    }
    D3D12_RESOURCE_STATES ShaderAccess(ID3D12Resource* resource,UINT mip,UINT layer,UINT plane)const {
        if(plane>=2)return D3D12_RESOURCE_STATE_COMMON;
        const auto found=shader_texture_ranges_.find(resource);
        if(found==shader_texture_ranges_.end())return D3D12_RESOURCE_STATE_COMMON;
        auto access=D3D12_RESOURCE_STATE_COMMON;
        for(const auto& range:found->second)if((range.planes&(1u<<plane))&&
            mip>=range.mip&&mip-range.mip<range.mips&&layer>=range.layer&&layer-range.layer<range.layers)
                access=D3D12_RESOURCE_STATES(access|range.access);
        return access;
    }
    bool HasShaderUav(ID3D12Resource* resource)const {
        const auto found=shader_texture_ranges_.find(resource);if(found==shader_texture_ranges_.end())return false;
        for(const auto& range:found->second)if(range.access&D3D12_RESOURCE_STATE_UNORDERED_ACCESS)return true;
        return false;
    }
    void Srv(UINT reg,UINT space,ID3D12Resource*,const D3D12_SHADER_RESOURCE_VIEW_DESC&,UINT index=0,const Resource* capture_source=nullptr);
    void Uav(UINT reg,UINT space,ID3D12Resource*,const D3D12_UNORDERED_ACCESS_VIEW_DESC&,UINT index=0);
    // Tier 3 only: the shader must guard every descriptor index against the
    // populated prefix. Used by DMA's GPU-visible active-bank count contract.
    void UnreachableUavTail(UINT reg,UINT space,UINT first);
    void Cbv(UINT reg,UINT space,ID3D12Resource*,UINT64 offset,UINT bytes,UINT index=0);
    void Sampler(UINT reg,UINT space,const D3D12_SAMPLER_DESC&,UINT index=0);
    void Bind(ID3D12GraphicsCommandList*,bool compute);
};
}
