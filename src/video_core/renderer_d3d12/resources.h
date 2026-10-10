#include "common/api_trace.h"
#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <mutex>
#include <limits>
#include <string>
#include <cstddef>
#include <algorithm>
#include <memory>
#include <unordered_map>
#include "diagnostics.h"
namespace NativeDX12 {
using Microsoft::WRL::ComPtr;
inline void Check(HRESULT hr) { if (FAILED(hr)) throw std::runtime_error("D3D12 operation failed: " + std::to_string(static_cast<unsigned long>(hr))); }
// All resource-state recording is confined to one command-list owner. This prevents
// CPU state from running ahead of the actual command submission order.
struct Resource {
    ComPtr<ID3D12Resource> handle;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    // Empty means every subresource is in `state`.  The vector is materialized
    // only when a caller starts transitioning individual mip/array/plane
    // subresources, keeping the common buffer path allocation-free.
    std::vector<D3D12_RESOURCE_STATES> subresource_states;
    // Keeps a placed-resource arena allocation alive across lightweight
    // Resource copies. Committed resources leave this empty.
    std::shared_ptr<void> allocation;
    // D3D12 forbids mixing legacy and enhanced state tracking for a resource.
    // BarrierBatch migrates through COMMON once, then every later transition
    // stays on the enhanced model.
    bool enhanced_barriers = false;
    static D3D12_BARRIER_ACCESS EnhancedAccess(D3D12_RESOURCE_STATES value) {
        D3D12_BARRIER_ACCESS access=D3D12_BARRIER_ACCESS(0);
        if(value&D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)access|=D3D12_BARRIER_ACCESS_VERTEX_BUFFER|D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
        if(value&D3D12_RESOURCE_STATE_INDEX_BUFFER)access|=D3D12_BARRIER_ACCESS_INDEX_BUFFER;
        if(value&D3D12_RESOURCE_STATE_RENDER_TARGET)access|=D3D12_BARRIER_ACCESS_RENDER_TARGET;
        if(value&D3D12_RESOURCE_STATE_UNORDERED_ACCESS)access|=D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
        if(value&D3D12_RESOURCE_STATE_DEPTH_WRITE)access|=D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
        if(value&D3D12_RESOURCE_STATE_DEPTH_READ)access|=D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
        if(value&(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE))access|=D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
        if(value&D3D12_RESOURCE_STATE_STREAM_OUT)access|=D3D12_BARRIER_ACCESS_STREAM_OUTPUT;
        if(value&D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)access|=D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;
        if(value&D3D12_RESOURCE_STATE_COPY_DEST)access|=D3D12_BARRIER_ACCESS_COPY_DEST;
        if(value&D3D12_RESOURCE_STATE_COPY_SOURCE)access|=D3D12_BARRIER_ACCESS_COPY_SOURCE;
        if(value&D3D12_RESOURCE_STATE_RESOLVE_DEST)access|=D3D12_BARRIER_ACCESS_RESOLVE_DEST;
        if(value&D3D12_RESOURCE_STATE_RESOLVE_SOURCE)access|=D3D12_BARRIER_ACCESS_RESOLVE_SOURCE;
        return access==D3D12_BARRIER_ACCESS(0)?D3D12_BARRIER_ACCESS_COMMON:access;
    }
    static D3D12_BARRIER_LAYOUT EnhancedLayout(D3D12_RESOURCE_STATES value) {
        if(value==D3D12_RESOURCE_STATE_COMMON)return D3D12_BARRIER_LAYOUT_COMMON;
        if(value&D3D12_RESOURCE_STATE_RENDER_TARGET)return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
        if(value&D3D12_RESOURCE_STATE_DEPTH_WRITE)return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
        if(value&D3D12_RESOURCE_STATE_DEPTH_READ)return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
        if(value&D3D12_RESOURCE_STATE_UNORDERED_ACCESS)return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
        if(value&D3D12_RESOURCE_STATE_COPY_DEST)return D3D12_BARRIER_LAYOUT_COPY_DEST;
        if(value&D3D12_RESOURCE_STATE_COPY_SOURCE)return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
        if(value&D3D12_RESOURCE_STATE_RESOLVE_DEST)return D3D12_BARRIER_LAYOUT_RESOLVE_DEST;
        if(value&D3D12_RESOURCE_STATE_RESOLVE_SOURCE)return D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE;
        if(value&D3D12_RESOURCE_STATE_PRESENT)return D3D12_BARRIER_LAYOUT_PRESENT;
        return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
    }
    UINT SubresourceCount() const {
        if (!handle) return 0;
        const auto desc = handle->GetDesc();
        if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) return 1;
        const UINT planes=desc.Format==DXGI_FORMAT_R32G8X24_TYPELESS||desc.Format==DXGI_FORMAT_D32_FLOAT_S8X24_UINT||
            desc.Format==DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS||desc.Format==DXGI_FORMAT_X32_TYPELESS_G8X24_UINT||
            desc.Format==DXGI_FORMAT_R24G8_TYPELESS||desc.Format==DXGI_FORMAT_D24_UNORM_S8_UINT||
            desc.Format==DXGI_FORMAT_R24_UNORM_X8_TYPELESS||desc.Format==DXGI_FORMAT_X24_TYPELESS_G8_UINT?2:1;
        const UINT arrays=desc.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE3D?1:UINT(desc.DepthOrArraySize);
        return std::max<UINT>(1, UINT(desc.MipLevels) * arrays*planes);
    }
    D3D12_RESOURCE_STATES State(UINT subresource) const {
        return subresource_states.empty() ? state : subresource_states.at(subresource);
    }
    void SetState(UINT subresource, D3D12_RESOURCE_STATES next) {
        if (subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
            state = next;
            subresource_states.clear();
            return;
        }
        const auto count = SubresourceCount();
        if (subresource >= count) throw std::out_of_range("D3D12 subresource state index");
        if (subresource_states.empty()) subresource_states.assign(count, state);
        subresource_states[subresource] = next;
        if (std::all_of(subresource_states.begin(), subresource_states.end(),
                        [next](auto value) { return value == next; })) {
            state = next;
            subresource_states.clear();
        }
    }
    void Transition(ID3D12GraphicsCommandList* list, D3D12_RESOURCE_STATES next) {
        Transition(list, next, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
    }
    void Transition(ID3D12GraphicsCommandList* list, D3D12_RESOURCE_STATES next,
                    UINT subresource) {
        if (!list || !handle) throw std::invalid_argument("null D3D12 transition object");
        if (subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES &&
            subresource_states.empty() && next == state) return;
        if (subresource != D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES &&
            State(subresource) == next) return;
        if(enhanced_barriers){
            if(subresource==D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES&&!subresource_states.empty()){
                const auto count=SubresourceCount();
                for(UINT index=0;index<count;++index)Transition(list,next,index);
                return;
            }
            ComPtr<ID3D12GraphicsCommandList7> list7;
            Check(DX12_API_CALL(list->QueryInterface(IID_PPV_ARGS(&list7))));
            const auto before=State(subresource);const auto desc=handle->GetDesc();
            D3D12_BARRIER_GROUP group{};
            D3D12_BUFFER_BARRIER buffer{};D3D12_TEXTURE_BARRIER texture{};
            if(desc.Dimension==D3D12_RESOURCE_DIMENSION_BUFFER){
                buffer.SyncBefore=D3D12_BARRIER_SYNC_ALL;buffer.SyncAfter=D3D12_BARRIER_SYNC_ALL;
                buffer.AccessBefore=EnhancedAccess(before);buffer.AccessAfter=EnhancedAccess(next);
                buffer.pResource=handle.Get();buffer.Offset=0;buffer.Size=UINT64_MAX;
                group={D3D12_BARRIER_TYPE_BUFFER,1,{.pBufferBarriers=&buffer}};
            }else{
                texture.SyncBefore=D3D12_BARRIER_SYNC_ALL;texture.SyncAfter=D3D12_BARRIER_SYNC_ALL;
                texture.AccessBefore=EnhancedAccess(before);texture.AccessAfter=EnhancedAccess(next);
                texture.LayoutBefore=EnhancedLayout(before);texture.LayoutAfter=EnhancedLayout(next);
                texture.pResource=handle.Get();texture.Flags=D3D12_TEXTURE_BARRIER_FLAG_NONE;
                const UINT arrays=desc.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE3D?1:std::max<UINT>(1,desc.DepthOrArraySize);
                const UINT planes=SubresourceCount()/(std::max<UINT>(1,desc.MipLevels)*arrays);
                if(subresource==D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
                    texture.Subresources={0,std::max<UINT>(1,desc.MipLevels),0,arrays,0,planes};
                else {const UINT mips=std::max<UINT>(1,desc.MipLevels);
                    texture.Subresources={subresource%mips,1,(subresource/mips)%arrays,1,subresource/(mips*arrays),1};}
                group={D3D12_BARRIER_TYPE_TEXTURE,1,{.pTextureBarriers=&texture}};
            }
            DX12_API_CALL(list7->Barrier(1,&group));SetState(subresource,next);return;
        }
        if (subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES &&
            !subresource_states.empty()) {
            std::vector<D3D12_RESOURCE_BARRIER> barriers;
            barriers.reserve(subresource_states.size());
            for (UINT index = 0; index < subresource_states.size(); ++index) {
                if (subresource_states[index] == next) continue;
                D3D12_RESOURCE_BARRIER barrier{};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Transition = {handle.Get(), index, subresource_states[index], next};
                barriers.push_back(barrier);
            }
            if (!barriers.empty())
                DX12_API_CALL(list->ResourceBarrier(UINT(barriers.size()), barriers.data()));
            SetState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, next);
            return;
        }
        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition = {handle.Get(), subresource, State(subresource), next};
        DX12_API_CALL(list->ResourceBarrier(1, &b));
        SetState(subresource, next);
    }
    void UavBarrier(ID3D12GraphicsCommandList* list) {
        if(enhanced_barriers){
            ComPtr<ID3D12GraphicsCommandList7> list7;Check(DX12_API_CALL(list->QueryInterface(IID_PPV_ARGS(&list7))));
            D3D12_GLOBAL_BARRIER barrier{D3D12_BARRIER_SYNC_ALL_SHADING,D3D12_BARRIER_SYNC_ALL_SHADING,
                D3D12_BARRIER_ACCESS_UNORDERED_ACCESS,D3D12_BARRIER_ACCESS_UNORDERED_ACCESS};
            D3D12_BARRIER_GROUP group{D3D12_BARRIER_TYPE_GLOBAL,1,{.pGlobalBarriers=&barrier}};
            DX12_API_CALL(list7->Barrier(1,&group));return;
        }
        D3D12_RESOURCE_BARRIER b{}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        b.UAV.pResource = handle.Get(); DX12_API_CALL(list->ResourceBarrier(1, &b));
    }
};
// Accumulates narrowly-scoped legacy barriers and applies resource state only
// when the batch is emitted.  This is the compatibility path for adapters or
// SDKs without Enhanced Barriers and mirrors Vulkan's deferred dependency
// collection without issuing one API call per transition.
class BarrierBatch {
    struct PendingState { Resource* resource; UINT subresource; D3D12_RESOURCE_STATES next; };
    std::vector<D3D12_RESOURCE_BARRIER> barriers_;
    std::vector<PendingState> states_;
    std::vector<Resource*> uavs_;
    std::vector<std::pair<Resource*,Resource*>> aliases_;
    std::unordered_map<Resource*,Resource> planned_;
    bool ordered_{};
    bool has_aliasing_{};
    static D3D12_BARRIER_ACCESS Access(D3D12_RESOURCE_STATES state) {
        D3D12_BARRIER_ACCESS access=D3D12_BARRIER_ACCESS(0);
        if(state&D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)access|=D3D12_BARRIER_ACCESS_VERTEX_BUFFER|D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
        if(state&D3D12_RESOURCE_STATE_INDEX_BUFFER)access|=D3D12_BARRIER_ACCESS_INDEX_BUFFER;
        if(state&D3D12_RESOURCE_STATE_RENDER_TARGET)access|=D3D12_BARRIER_ACCESS_RENDER_TARGET;
        if(state&D3D12_RESOURCE_STATE_UNORDERED_ACCESS)access|=D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
        if(state&D3D12_RESOURCE_STATE_DEPTH_WRITE)access|=D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
        if(state&D3D12_RESOURCE_STATE_DEPTH_READ)access|=D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
        if(state&(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE))access|=D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
        if(state&D3D12_RESOURCE_STATE_STREAM_OUT)access|=D3D12_BARRIER_ACCESS_STREAM_OUTPUT;
        if(state&D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)access|=D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;
        if(state&D3D12_RESOURCE_STATE_COPY_DEST)access|=D3D12_BARRIER_ACCESS_COPY_DEST;
        if(state&D3D12_RESOURCE_STATE_COPY_SOURCE)access|=D3D12_BARRIER_ACCESS_COPY_SOURCE;
        if(state&D3D12_RESOURCE_STATE_RESOLVE_DEST)access|=D3D12_BARRIER_ACCESS_RESOLVE_DEST;
        if(state&D3D12_RESOURCE_STATE_RESOLVE_SOURCE)access|=D3D12_BARRIER_ACCESS_RESOLVE_SOURCE;
        return access==D3D12_BARRIER_ACCESS(0)?D3D12_BARRIER_ACCESS_COMMON:access;
    }
    static D3D12_BARRIER_LAYOUT Layout(D3D12_RESOURCE_STATES state) {
        if(state==D3D12_RESOURCE_STATE_COMMON)return D3D12_BARRIER_LAYOUT_COMMON;
        if(state&D3D12_RESOURCE_STATE_RENDER_TARGET)return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
        if(state&D3D12_RESOURCE_STATE_DEPTH_WRITE)return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
        if(state&D3D12_RESOURCE_STATE_DEPTH_READ)return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
        if(state&D3D12_RESOURCE_STATE_UNORDERED_ACCESS)return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
        if(state&D3D12_RESOURCE_STATE_COPY_DEST)return D3D12_BARRIER_LAYOUT_COPY_DEST;
        if(state&D3D12_RESOURCE_STATE_COPY_SOURCE)return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
        if(state&D3D12_RESOURCE_STATE_RESOLVE_DEST)return D3D12_BARRIER_LAYOUT_RESOLVE_DEST;
        if(state&D3D12_RESOURCE_STATE_RESOLVE_SOURCE)return D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE;
        if(state&D3D12_RESOURCE_STATE_PRESENT)return D3D12_BARRIER_LAYOUT_PRESENT;
        return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
    }
public:
    void Transition(Resource& resource, D3D12_RESOURCE_STATES next,
                    UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
        if (!resource.handle) throw std::invalid_argument("null D3D12 barrier resource");
        auto& planned=planned_.try_emplace(&resource,resource).first->second;
        if(subresource!=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES&&subresource>=resource.SubresourceCount())
            throw std::out_of_range("D3D12 planned subresource state index");
        if (subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES &&
            !planned.subresource_states.empty()) {
            const auto count=resource.SubresourceCount();
            for (UINT index = 0; index < count; ++index)
                Transition(resource, next, index);
            return;
        }
        const auto before = planned.State(subresource);
        if (before == next) return;
        // Enhanced barriers on the same memory are not implicitly ordered
        // within one call. Keep dependent transitions as separate operations.
        ordered_|=std::any_of(states_.begin(),states_.end(),[&](const auto& s){return s.resource==&resource;});
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition = {resource.handle.Get(), subresource, before, next};
        barriers_.push_back(barrier);
        states_.push_back({&resource, subresource, next});
        planned.SetState(subresource,next);
    }
    void Uav(Resource* resource = nullptr) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = resource ? resource->handle.Get() : nullptr;
        barriers_.push_back(barrier);
        uavs_.push_back(resource);
    }
    void Aliasing(Resource* before = nullptr, Resource* after = nullptr) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
        barrier.Aliasing.pResourceBefore = before ? before->handle.Get() : nullptr;
        barrier.Aliasing.pResourceAfter = after ? after->handle.Get() : nullptr;
        barriers_.push_back(barrier);
        aliases_.emplace_back(before,after);
        for(auto* resource:{before,after})if(resource){
            auto& planned=planned_.try_emplace(resource,*resource).first->second;
            if(planned.enhanced_barriers){
                planned.SetState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COMMON);
                planned.enhanced_barriers=false;
            }
        }
        has_aliasing_=true;
    }
    bool Empty() const { return barriers_.empty(); }
    void Flush(ID3D12GraphicsCommandList* list) {
        if (!list) throw std::invalid_argument("null D3D12 barrier list");
        if (barriers_.empty()) {planned_.clear();return;}
        bool enhanced=!has_aliasing_&&uavs_.empty()&&!ordered_;
        for(const auto& pending:states_)if(pending.subresource!=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
            enhanced=false;
        ComPtr<ID3D12GraphicsCommandList7> list7;
        ComPtr<ID3D12Device> device;
        D3D12_FEATURE_DATA_D3D12_OPTIONS12 options{};
        const bool can_enhanced=SUCCEEDED(list->QueryInterface(IID_PPV_ARGS(&list7)))&&
            SUCCEEDED(list->GetDevice(IID_PPV_ARGS(&device)))&&
            SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12,&options,sizeof(options)))&&
            options.EnhancedBarriersSupported;
        enhanced=enhanced&&can_enhanced;
        bool states_applied=false;
        if(enhanced){
            // A resource created or previously transitioned with legacy states
            // must cross COMMON once before the enhanced model may own it.
            std::vector<D3D12_RESOURCE_BARRIER> migrations;
            for(const auto& pending:states_)if(!pending.resource->enhanced_barriers){
                bool seen=false;for(const auto& migration:migrations)
                    if(migration.Transition.pResource==pending.resource->handle.Get()){seen=true;break;}
                if(seen)continue;
                const auto before=pending.resource->State(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
                if(before!=D3D12_RESOURCE_STATE_COMMON){D3D12_RESOURCE_BARRIER migration{};
                    migration.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                    migration.Transition={pending.resource->handle.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,before,D3D12_RESOURCE_STATE_COMMON};
                    migrations.push_back(migration);}
            }
            if(!migrations.empty())DX12_API_CALL(list->ResourceBarrier(UINT(migrations.size()),migrations.data()));
            for(const auto& pending:states_)if(!pending.resource->enhanced_barriers){
                pending.resource->SetState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COMMON);
                pending.resource->enhanced_barriers=true;}
            std::vector<D3D12_BUFFER_BARRIER> buffers;
            std::vector<D3D12_TEXTURE_BARRIER> textures;
            std::vector<D3D12_GLOBAL_BARRIER> globals;
            for(const auto& pending:states_){
                const auto before=pending.resource->State(pending.subresource);
                const auto desc=pending.resource->handle->GetDesc();
                if(desc.Dimension==D3D12_RESOURCE_DIMENSION_BUFFER){
                    D3D12_BUFFER_BARRIER barrier{};
                    barrier.SyncBefore=D3D12_BARRIER_SYNC_ALL;barrier.SyncAfter=D3D12_BARRIER_SYNC_ALL;
                    barrier.AccessBefore=Access(before);barrier.AccessAfter=Access(pending.next);
                    barrier.pResource=pending.resource->handle.Get();barrier.Offset=0;barrier.Size=UINT64_MAX;
                    buffers.push_back(barrier);
                }else{
                    D3D12_TEXTURE_BARRIER barrier{};
                    barrier.SyncBefore=D3D12_BARRIER_SYNC_ALL;barrier.SyncAfter=D3D12_BARRIER_SYNC_ALL;
                    barrier.AccessBefore=Access(before);barrier.AccessAfter=Access(pending.next);
                    barrier.LayoutBefore=Layout(before);barrier.LayoutAfter=Layout(pending.next);
                    barrier.pResource=pending.resource->handle.Get();
                    const UINT mips=std::max<UINT>(1,desc.MipLevels),arrays=desc.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE3D?1:std::max<UINT>(1,desc.DepthOrArraySize);
                    const UINT planes=pending.resource->SubresourceCount()/(mips*arrays);
                    barrier.Subresources={0,mips,0,arrays,0,planes};
                    barrier.Flags=D3D12_TEXTURE_BARRIER_FLAG_NONE;textures.push_back(barrier);
                }
            }
            for([[maybe_unused]] auto* resource:uavs_){
                D3D12_GLOBAL_BARRIER barrier{};
                barrier.SyncBefore=D3D12_BARRIER_SYNC_ALL_SHADING;barrier.SyncAfter=D3D12_BARRIER_SYNC_ALL_SHADING;
                barrier.AccessBefore=D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
                barrier.AccessAfter=D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;globals.push_back(barrier);
            }
            std::vector<D3D12_BARRIER_GROUP> groups;
            if(!globals.empty())groups.push_back({D3D12_BARRIER_TYPE_GLOBAL,UINT(globals.size()),{.pGlobalBarriers=globals.data()}});
            if(!buffers.empty())groups.push_back({D3D12_BARRIER_TYPE_BUFFER,UINT(buffers.size()),{.pBufferBarriers=buffers.data()}});
            if(!textures.empty())groups.push_back({D3D12_BARRIER_TYPE_TEXTURE,UINT(textures.size()),{.pTextureBarriers=textures.data()}});
            if(!groups.empty())DX12_API_CALL(list7->Barrier(UINT(groups.size()),groups.data()));
        }else if(has_aliasing_||
            (ordered_||std::any_of(states_.begin(),states_.end(),[](const auto& s){return s.resource->enhanced_barriers;})||
             std::any_of(uavs_.begin(),uavs_.end(),[](const auto* r){return r&&r->enhanced_barriers;}))){
            // A batch containing individual subresources cannot use the whole-
            // resource enhanced fast path above. Keep each resource on its
            // existing barrier model instead of feeding enhanced layouts to
            // legacy ResourceBarrier without the required COMMON interop step.
            size_t state_index{},uav_index{},alias_index{};
            for(const auto& barrier:barriers_){
                if(barrier.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION){
                    const auto& pending=states_.at(state_index++);
                    pending.resource->Transition(list,pending.next,pending.subresource);
                }else if(barrier.Type==D3D12_RESOURCE_BARRIER_TYPE_ALIASING){
                    const auto [before,after]=aliases_.at(alias_index++);
                    // Legacy aliasing may only reference enhanced resources
                    // after finishing/flushing their accesses in COMMON.
                    // Leave deactivated resources there for later reuse.
                    if((before&&before->enhanced_barriers)||(after&&after->enhanced_barriers)){
                        // COMMON itself permits accesses. Flush them as well,
                        // even when Transition(COMMON) would be a state no-op.
                        D3D12_GLOBAL_BARRIER global{D3D12_BARRIER_SYNC_ALL,D3D12_BARRIER_SYNC_ALL,
                            D3D12_BARRIER_ACCESS_COMMON,D3D12_BARRIER_ACCESS_COMMON};
                        D3D12_BARRIER_GROUP group{D3D12_BARRIER_TYPE_GLOBAL,1,{.pGlobalBarriers=&global}};
                        DX12_API_CALL(list7->Barrier(1,&group));
                    }
                    for(auto* resource:{before,after})if(resource&&resource->enhanced_barriers){
                        resource->Transition(list,D3D12_RESOURCE_STATE_COMMON);
                        resource->enhanced_barriers=false;
                    }
                    if(!before&&!after&&can_enhanced){
                        D3D12_GLOBAL_BARRIER global{D3D12_BARRIER_SYNC_ALL,D3D12_BARRIER_SYNC_ALL,
                            D3D12_BARRIER_ACCESS_COMMON,D3D12_BARRIER_ACCESS_COMMON};
                        D3D12_BARRIER_GROUP group{D3D12_BARRIER_TYPE_GLOBAL,1,{.pGlobalBarriers=&global}};
                        DX12_API_CALL(list7->Barrier(1,&group));
                    }else DX12_API_CALL(list->ResourceBarrier(1,&barrier));
                }else if(auto* resource=uavs_.at(uav_index++))resource->UavBarrier(list);
                else if(can_enhanced){
                    D3D12_GLOBAL_BARRIER global{D3D12_BARRIER_SYNC_ALL_SHADING,D3D12_BARRIER_SYNC_ALL_SHADING,
                        D3D12_BARRIER_ACCESS_UNORDERED_ACCESS,D3D12_BARRIER_ACCESS_UNORDERED_ACCESS};
                    D3D12_BARRIER_GROUP group{D3D12_BARRIER_TYPE_GLOBAL,1,{.pGlobalBarriers=&global}};
                    DX12_API_CALL(list7->Barrier(1,&group));
                }else DX12_API_CALL(list->ResourceBarrier(1,&barrier));
            }
            states_applied=true;
        }else DX12_API_CALL(list->ResourceBarrier(UINT(barriers_.size()), barriers_.data()));
        if(!states_applied)for (const auto& pending : states_)
            pending.resource->SetState(pending.subresource, pending.next);
        barriers_.clear();
        states_.clear();
        uavs_.clear();aliases_.clear();planned_.clear();has_aliasing_=false;ordered_=false;
    }
};
inline Resource Buffer(ID3D12Device* device, uint64_t size, D3D12_HEAP_TYPE heap,
                       D3D12_RESOURCE_STATES state) {
    if (!size) throw std::invalid_argument("zero-sized buffer");
    D3D12_HEAP_PROPERTIES props{}; props.Type=heap;
    D3D12_RESOURCE_DESC desc{}; desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width=size; desc.Height=1; desc.DepthOrArraySize=1; desc.MipLevels=1;
    desc.SampleDesc.Count=1; desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    Resource r; r.state=state;
    Check(DX12_API_CALL(device->CreateCommittedResource(&props,D3D12_HEAP_FLAG_NONE,&desc,state,nullptr,IID_PPV_ARGS(&r.handle))));
    return r;
}
class Timeline {
    ComPtr<ID3D12Fence> fence_;
    HANDLE event_ = nullptr;
    uint64_t next_ = 1;
    std::mutex mutex_;
    std::mutex wait_mutex_;
public:
    explicit Timeline(ID3D12Device* device) {
        if(!device)throw std::invalid_argument("Null timeline device");
        Check(DX12_API_CALL(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence_))));
        event_=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        if (!event_) throw std::runtime_error("CreateEvent failed");
    }
    ~Timeline() { if(event_) CloseHandle(event_); }
    Timeline(const Timeline&)=delete;
    uint64_t Completed() const {
        auto value=DX12_API_CALL(fence_->GetCompletedValue());
        if(value==UINT64_MAX) throw std::runtime_error("D3D12 device removed");
        return value;
    }
    ID3D12Fence* Fence() const { return fence_.Get(); }
    // Queue submit and signal must share this lock, otherwise another thread can
    // interleave work between the list and its retirement fence.
    uint64_t Submit(ID3D12CommandQueue* queue, ID3D12CommandList* list) {
        std::lock_guard lock(mutex_);
        DX12_API_CALL(queue->ExecuteCommandLists(1,&list));
        auto value=next_++; Check(DX12_API_CALL(queue->Signal(fence_.Get(),value))); return value;
    }
    uint64_t Signal(ID3D12CommandQueue* queue) {
        std::lock_guard lock(mutex_);
        const auto value=next_++;
        Check(DX12_API_CALL(queue->Signal(fence_.Get(),value)));
        return value;
    }
    void Wait(uint64_t value) {
        Diagnostics::Scope diagnostic(Diagnostics::Kind::FenceWait,"wait-lock");
        if(Diagnostics::Enabled())diagnostic.Detail("target=%llu completed=%llu",(unsigned long long)value,
            (unsigned long long)Completed());
        // Serialize shared-event users without blocking concurrent queue submits.
        std::lock_guard lock(wait_mutex_);
        if(Completed()>=value) return;
        Check(DX12_API_CALL(fence_->SetEventOnCompletion(value,event_)));
        diagnostic.Stage("gpu-completion");
        const auto result=WaitForSingleObject(event_,30000);
        if(result!=WAIT_OBJECT_0) throw std::runtime_error("GPU fence wait failed: result="+
            std::to_string(result)+" target="+std::to_string(value)+" completed="+std::to_string(Completed()));
        if(Completed()<value) throw std::runtime_error("GPU fence incomplete");
    }
};
// Descriptor handles remain reserved until their last GPU submission completes.
// Each allocation is a single slot; callers build descriptor tables separately.
class Descriptors {
    enum class Phase { Free, Live, Retired };
    struct Slot { Phase phase=Phase::Free; uint64_t fence=0; };
    ComPtr<ID3D12DescriptorHeap> heap_;
    std::vector<Slot> slots_;
    UINT stride_;
    bool visible_;
public:
    Descriptors(ID3D12Device* device,D3D12_DESCRIPTOR_HEAP_TYPE type,UINT count,bool visible)
      : slots_(count),stride_(DX12_API_CALL(device->GetDescriptorHandleIncrementSize(type))),visible_(visible) {
        if(!count) throw std::invalid_argument("empty descriptor heap");
        D3D12_DESCRIPTOR_HEAP_DESC d{type,count,visible?D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE:D3D12_DESCRIPTOR_HEAP_FLAG_NONE,0};
        Check(DX12_API_CALL(device->CreateDescriptorHeap(&d,IID_PPV_ARGS(&heap_))));
    }
    UINT Allocate(uint64_t completed) {
        for(UINT i=0;i<slots_.size();++i) {
            auto& s=slots_[i];
            if(s.phase==Phase::Retired && s.fence<=completed) s.phase=Phase::Free;
            if(s.phase==Phase::Free) {s.phase=Phase::Live;return i;}
        }
        throw std::runtime_error("descriptor heap exhausted");
    }
    void Retire(UINT slot,uint64_t fence) {
        if(slot>=slots_.size() || slots_[slot].phase!=Phase::Live || !fence)
            throw std::invalid_argument("invalid descriptor retirement");
        slots_[slot]={Phase::Retired,fence};
    }
    D3D12_CPU_DESCRIPTOR_HANDLE Cpu(UINT i) const {
        if(i>=slots_.size()) throw std::out_of_range("descriptor index");
        auto h=DX12_API_CALL(heap_->GetCPUDescriptorHandleForHeapStart());h.ptr+=SIZE_T(i)*stride_;return h;
    }
    D3D12_GPU_DESCRIPTOR_HANDLE Gpu(UINT i) const {
        if(!visible_ || i>=slots_.size()) throw std::out_of_range("GPU descriptor unavailable");
        auto h=DX12_API_CALL(heap_->GetGPUDescriptorHandleForHeapStart());h.ptr+=UINT64(i)*stride_;return h;
    }
    ID3D12DescriptorHeap* Heap() const {return heap_.Get();}
};
// One persistent upload buffer per in-flight command context. Reset only after
// its submitted fence. No Map/Unmap or committed-resource allocation per draw.
class UploadArena {
    Resource buffer_;
    std::byte* data_=nullptr;
    uint64_t capacity_,offset_=0,retirement_=0;
public:
    struct Slice { ID3D12Resource* resource; uint64_t offset; void* cpu; D3D12_GPU_VIRTUAL_ADDRESS gpu; };
    UploadArena(ID3D12Device* device,uint64_t capacity)
      :buffer_(Buffer(device,capacity,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ)),capacity_(capacity) {
        D3D12_RANGE no_read{0,0}; Check(DX12_API_CALL(buffer_.handle->Map(0,&no_read,reinterpret_cast<void**>(&data_))));
    }
    ~UploadArena() { if(data_) DX12_API_CALL(buffer_.handle->Unmap(0,nullptr)); }
    UploadArena(const UploadArena&)=delete;
    Slice Allocate(uint64_t size,uint64_t alignment=256) {
        if(retirement_) throw std::logic_error("upload arena still submitted");
        if(!size || !alignment || (alignment&(alignment-1))) throw std::invalid_argument("invalid upload allocation");
        if(offset_>UINT64_MAX-(alignment-1)) throw std::overflow_error("upload offset overflow");
        auto start=(offset_+alignment-1)&~(alignment-1);
        if(start>capacity_ || size>capacity_-start) throw std::runtime_error("upload arena exhausted");
        offset_=start+size;
        return {buffer_.handle.Get(),start,data_+start,DX12_API_CALL(buffer_.handle->GetGPUVirtualAddress())+start};
    }
    void Submitted(uint64_t fence) {
        if(!fence || retirement_) throw std::logic_error("invalid upload retirement");
        retirement_=fence;
    }
    void Reset(uint64_t completed) {
        if(retirement_>completed) throw std::logic_error("upload buffer still in flight");
        offset_=0;retirement_=0;
    }
};
}
