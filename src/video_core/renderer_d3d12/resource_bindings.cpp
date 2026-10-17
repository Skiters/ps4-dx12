#include "common/api_trace.h"
#include "common/runtime_module_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "resource_bindings.h"
#include "nv_shader_clock.h"
#include "ags_shader_clock.h"
#include "diagnostics.h"
#include <dxcapi.h>
#include <d3d12shader.h>
#include <algorithm>
#include <map>
#include <tuple>
#include <bit>
namespace NativeDX12 {
DescriptorHeapPool::Slice DescriptorHeapPool::Acquire(UINT count,D3D12_DESCRIPTOR_HEAP_TYPE type) {
    if(!count)return {};
    const UINT maximum=type==D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER?D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE:1000000;
    if(count>maximum)throw std::invalid_argument("Descriptor table exceeds shader-visible heap limit");
    for(auto& page:pages_)if(page->type==type&&count<=page->capacity&&page->cursor<=page->capacity-count){
        Diagnostics::Count("descriptor-page.reused");Diagnostics::Count("descriptor-count",count);
        const auto offset=page->cursor;page->cursor+=count;++page->active;++reused_;return {page,offset,count};
    }
    // One resource page normally covers several PS4 frames; a sampler page uses
    // the hardware maximum. This keeps SetDescriptorHeaps stable across draws.
    const UINT preferred=type==D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER?maximum:65536;
    const auto capacity=std::min(maximum,std::max(preferred,std::bit_ceil(count)));
    D3D12_DESCRIPTOR_HEAP_DESC desc{type,capacity,D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,0};
    auto page=std::make_shared<Page>();page->type=type;page->capacity=capacity;page->cursor=count;page->active=1;
    Check(DX12_API_CALL(device_->CreateDescriptorHeap(&desc,IID_PPV_ARGS(&page->heap))));
    Diagnostics::Count("descriptor-page.created");Diagnostics::Count("descriptor-count",count);
    ++created_;pages_.push_back(page);return {std::move(page),0,count};
}
void DescriptorHeapPool::Release(Slice& slice){
    if(!slice)return;
    ++released_;auto page=std::move(slice.page);slice={};
    if(!page->active)throw std::logic_error("Descriptor page released without active slice");
    if(!--page->active){
        // Every table on this page has crossed its retirement fence. Reset the
        // linear allocator; descriptors are renamed instead of overwritten in flight.
        page->cursor=0;
        // Keep a bounded number of completely idle overflow pages per type.
        size_t idle{};for(const auto& candidate:pages_)if(candidate->type==page->type&&!candidate->active)++idle;
        if(idle>8){
            auto it=std::find(pages_.begin(),pages_.end(),page);
            if(it!=pages_.end()){pages_.erase(it);++discarded_;}
        }
    }
}
namespace {
struct Module {
    HMODULE handle{};
    explicit Module(const std::wstring& path) {
        handle=Common::RuntimeModuleTrace::Load(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if(!handle)throw std::runtime_error("Cannot load DXC reflection library");
    }
    ~Module(){if(handle)FreeLibrary(handle);}
};
D3D12_DESCRIPTOR_RANGE_TYPE RangeType(D3D_SHADER_INPUT_TYPE type) {
    switch(type) {
    case D3D_SIT_CBUFFER:return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    case D3D_SIT_TBUFFER:case D3D_SIT_TEXTURE:case D3D_SIT_STRUCTURED:case D3D_SIT_BYTEADDRESS:
        return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    case D3D_SIT_UAV_RWTYPED:case D3D_SIT_UAV_RWSTRUCTURED:case D3D_SIT_UAV_RWBYTEADDRESS:
        return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    case D3D_SIT_UAV_APPEND_STRUCTURED:case D3D_SIT_UAV_CONSUME_STRUCTURED:case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
        throw std::runtime_error("DXIL counter UAV requires explicit counter resource support");
    case D3D_SIT_SAMPLER:return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    default:throw std::runtime_error("Unsupported DXIL resource binding kind");
    }
}
}
ResourceLayout::ResourceLayout(ID3D12Device* device,const std::wstring& dxc_dll,
    std::span<const std::span<const uint8_t>> shaders) {
    if(!device||shaders.empty())throw std::invalid_argument("Resource layout needs device and shader bytecode");
    D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
    Check(DX12_API_CALL(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,&options,sizeof(options))));
    binding_tier_=options.ResourceBindingTier;
    Module module(dxc_dll);
    auto create=reinterpret_cast<DxcCreateInstanceProc>(GetProcAddress(module.handle,"DxcCreateInstance"));
    if(!create)throw std::runtime_error("DXC reflection entry point unavailable");
    ComPtr<IDxcUtils> utils;Check(DX12_API_CALL(create(CLSID_DxcUtils,IID_PPV_ARGS(&utils))));
    using Key=std::tuple<UINT,UINT,UINT>;
    std::map<Key,ShaderBinding> unique;
    for(auto code:shaders) {
        if(code.empty())continue;
        DxcBuffer input{code.data(),code.size(),DXC_CP_ACP};
        ComPtr<ID3D12ShaderReflection> reflection;
        Check(DX12_API_CALL(utils->CreateReflection(&input,IID_PPV_ARGS(&reflection))));
        D3D12_SHADER_DESC desc{};Check(DX12_API_CALL(reflection->GetDesc(&desc)));
        for(UINT i=0;i<desc.BoundResources;++i) {
            D3D12_SHADER_INPUT_BIND_DESC binding{};Check(DX12_API_CALL(reflection->GetResourceBindingDesc(i,&binding)));
            if(!binding.BindCount||binding.BindCount>1000000||binding.BindPoint>UINT_MAX-binding.BindCount)
                throw std::runtime_error("Unbounded/oversized DXIL descriptor range is not supported yet");
            const bool nv_clock=binding.Type==D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER&&
                IsNvShaderClockBinding(D3D12_DESCRIPTOR_RANGE_TYPE_UAV,binding.BindPoint,binding.Space,binding.BindCount)&&
                binding.Name&&std::string_view(binding.Name)=="__nv_shader_extensions";
            const bool amd_clock=binding.Type==D3D_SIT_UAV_RWBYTEADDRESS&&binding.Space==AmdClockSpace&&
                binding.BindPoint==AmdClockRegister&&binding.BindCount==1&&binding.Name&&
                std::string_view(binding.Name)=="__ags_shader_clock";
            auto type=nv_clock?D3D12_DESCRIPTOR_RANGE_TYPE_UAV:RangeType(binding.Type);Key key{UINT(type),binding.Space,binding.BindPoint};
            auto [it,added]=unique.emplace(key,ShaderBinding{type,binding.BindPoint,binding.Space,binding.BindCount,0,nv_clock,amd_clock});
            if(added)it->second.dimension=binding.Dimension;
            else if(it->second.dimension!=binding.Dimension)
                throw std::runtime_error("Conflicting DXIL resource dimensions at one register");
            if(!added&&(it->second.count!=binding.BindCount||it->second.nv_clock!=nv_clock||it->second.amd_clock!=amd_clock))
                throw std::runtime_error("Conflicting DXIL descriptor array sizes or driver extension binding");
        }
    }
    std::vector<D3D12_DESCRIPTOR_RANGE> resource_ranges,sampler_ranges;
    for(const auto& [key,value]:unique) {
        auto binding=value;
        for(const auto& previous:bindings_)if(previous.type==binding.type&&previous.space==binding.space&&
            binding.reg<previous.reg+previous.count&&previous.reg<binding.reg+binding.count)
            throw std::runtime_error("Overlapping DXIL resource register ranges");
        const bool sampler=binding.type==D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        auto& count=sampler?sampler_count_:resource_count_;
        const UINT budget=sampler?D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE:1000000;
        if(binding.count>budget||count>budget-binding.count)throw std::runtime_error("DXIL descriptor layout exceeds supported budget");
        binding.offset=count;count+=binding.count;
        auto& ranges=sampler?sampler_ranges:resource_ranges;
        ranges.push_back({binding.type,binding.count,binding.reg,binding.space,binding.offset});
        bindings_.push_back(binding);
    }
    D3D12_ROOT_PARAMETER parameters[2]{};UINT parameter_count=0;
    auto table=[&](const std::vector<D3D12_DESCRIPTOR_RANGE>& ranges,UINT& index) {
        if(ranges.empty())return;
        index=parameter_count++;auto& parameter=parameters[index];
        parameter.ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameter.DescriptorTable={UINT(ranges.size()),ranges.data()};
        parameter.ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
    };
    table(resource_ranges,resource_parameter_);table(sampler_ranges,sampler_parameter_);
    D3D12_ROOT_SIGNATURE_DESC desc{parameter_count,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAGS(
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT|D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT)};
    ComPtr<ID3DBlob> blob,errors;auto hr=DX12_API_CALL(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors));
    if(FAILED(hr))throw std::runtime_error(errors?std::string(static_cast<const char*>(DX12_API_CALL(errors->GetBufferPointer())),DX12_API_CALL(errors->GetBufferSize())):"DX12 root signature serialization failed");
    Check(DX12_API_CALL(device->CreateRootSignature(0,DX12_API_CALL(blob->GetBufferPointer()),DX12_API_CALL(blob->GetBufferSize()),IID_PPV_ARGS(&root_))));
}
ResourceBindings::ResourceBindings(ID3D12Device* device,const ResourceLayout& layout,DescriptorHeapPool* pool):device_(device),layout_(layout),
    written_resources_(layout.resource_count_),written_samplers_(layout.sampler_count_),pool_(pool) {
    if(!device)throw std::invalid_argument("Null resource binding device");
    ComPtr<ID3D12Device> owner;Check(DX12_API_CALL(layout.Root()->GetDevice(IID_PPV_ARGS(&owner))));
    if(owner.Get()!=device)throw std::invalid_argument("Resource layout belongs to another D3D12 device");
    if(pool&&pool->Device()!=device)throw std::invalid_argument("Descriptor pool belongs to another D3D12 device");
    auto heap=[&](UINT count,D3D12_DESCRIPTOR_HEAP_TYPE type,DescriptorHeapPool::Slice& target,ComPtr<ID3D12DescriptorHeap>& owned) {
        if(!count)return;D3D12_DESCRIPTOR_HEAP_DESC desc{type,count,D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,0};
        if(pool_){target=pool_->Acquire(count,type);return;}
        Check(DX12_API_CALL(device->CreateDescriptorHeap(&desc,IID_PPV_ARGS(&owned))));
    };
    heap(layout.resource_count_,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,resources_,owned_resources_);
    heap(layout.sampler_count_,D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,samplers_,owned_samplers_);
    for(const auto& binding:layout.bindings_)if(binding.nv_clock){
        D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};desc.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;
        desc.Buffer.NumElements=1;desc.Buffer.StructureByteStride=256;
        Uav(binding.reg,binding.space,nullptr,desc);
    }else if(binding.amd_clock){
        D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};desc.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;
        desc.Format=DXGI_FORMAT_R32_TYPELESS;desc.Buffer.NumElements=1;desc.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
        Uav(binding.reg,binding.space,nullptr,desc);
    }
}
void ResourceBindings::RecycleRetiredHeaps(){
    if(!pool_)return;
    if(recycled_)throw std::logic_error("Descriptor heaps already recycled");
    pool_->Release(resources_);pool_->Release(samplers_);
    keep_alive_.clear();sealed_=recycled_=true;
}
D3D12_CPU_DESCRIPTOR_HANDLE ResourceBindings::Slot(D3D12_DESCRIPTOR_RANGE_TYPE type,UINT reg,UINT space,UINT index) {
    if(sealed_)throw std::logic_error("Cannot mutate descriptors after recording their use");
    for(const auto& binding:layout_.bindings_)if(binding.type==type&&binding.reg==reg&&binding.space==space) {
        if(index>=binding.count)throw std::out_of_range("Descriptor array index");
        const bool sampler=type==D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        const auto& slice=sampler?samplers_:resources_;auto* heap=slice?slice.page->heap.Get():(sampler?owned_samplers_:owned_resources_).Get();
        auto handle=DX12_API_CALL(heap->GetCPUDescriptorHandleForHeapStart());
        handle.ptr+=SIZE_T((slice?slice.offset:0)+binding.offset+index)*DX12_API_CALL(device_->GetDescriptorHandleIncrementSize(sampler?D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
        (sampler?written_samplers_:written_resources_)[binding.offset+index]=true;
        return handle;
    }
    throw std::out_of_range("Register absent from reflected DXIL resource layout");
}
void ResourceBindings::Retain(ID3D12Resource* resource){if(resource)keep_alive_.emplace_back(resource);}
void ResourceBindings::TrackShaderTextureRange(ID3D12Resource* resource,ShaderTextureRange range,SIZE_T descriptor){
    for(auto& [_,ranges]:shader_texture_ranges_)
        std::erase_if(ranges,[descriptor](const auto& previous){return previous.descriptor==descriptor;});
    if(resource&&range.planes){range.descriptor=descriptor;shader_texture_ranges_[resource].push_back(range);}
}
void ResourceBindings::Srv(UINT reg,UINT space,ID3D12Resource* resource,const D3D12_SHADER_RESOURCE_VIEW_DESC& desc,UINT index) {
    // Ordinary buffers/color images do not need depth attachment range data.
    // Unknown DS views retain conservative whole-resource access.
    ShaderTextureRange range{};
    if(resource){
        const auto texture=DX12_API_CALL(resource->GetDesc());
        if(texture.Flags&D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL){
            range={0,UINT_MAX,0,UINT_MAX,3};bool known=true;
            switch(desc.ViewDimension){
            case D3D12_SRV_DIMENSION_TEXTURE2D:
                range={desc.Texture2D.MostDetailedMip,desc.Texture2D.MipLevels,0,1,3};break;
            case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
                range={desc.Texture2DArray.MostDetailedMip,desc.Texture2DArray.MipLevels,
                    desc.Texture2DArray.FirstArraySlice,desc.Texture2DArray.ArraySize,3};break;
            case D3D12_SRV_DIMENSION_TEXTURE2DMS:range={0,1,0,1,3};break;
            case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
                range={0,1,desc.Texture2DMSArray.FirstArraySlice,desc.Texture2DMSArray.ArraySize,3};break;
            case D3D12_SRV_DIMENSION_TEXTURECUBE:range={desc.TextureCube.MostDetailedMip,desc.TextureCube.MipLevels,0,6,3};break;
            case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
                if(desc.TextureCubeArray.NumCubes>UINT_MAX/6)throw std::out_of_range("Native cube SRV range overflow");
                range={desc.TextureCubeArray.MostDetailedMip,desc.TextureCubeArray.MipLevels,
                    desc.TextureCubeArray.First2DArrayFace,desc.TextureCubeArray.NumCubes*6,3};break;
            default:known=false;break;
            }
            if(known){
                if(range.mip>=texture.MipLevels)throw std::out_of_range("Native depth SRV first mip");
                if(range.mips==UINT_MAX)range.mips=texture.MipLevels-range.mip;
                if(!range.mips||range.mips>texture.MipLevels-range.mip||range.layer>=texture.DepthOrArraySize||
                    !range.layers||range.layers>texture.DepthOrArraySize-range.layer)
                    throw std::out_of_range("Native depth SRV range exceeds resource");
            }
            if(desc.Format==DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS||desc.Format==DXGI_FORMAT_R24_UNORM_X8_TYPELESS)range.planes=1;
            else if(desc.Format==DXGI_FORMAT_X32_TYPELESS_G8X24_UINT||desc.Format==DXGI_FORMAT_X24_TYPELESS_G8_UINT)range.planes=2;
        }
    }
    auto slot=Slot(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,reg,space,index);DX12_API_CALL(device_->CreateShaderResourceView(resource,&desc,slot));Retain(resource);
    TrackShaderTextureRange(resource,range,slot.ptr);
    if(resource){
        UINT planes=3; // Unknown views conservatively reference both DS planes.
        if(desc.Format==DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS||desc.Format==DXGI_FORMAT_R24_UNORM_X8_TYPELESS)planes=1;
        else if(desc.Format==DXGI_FORMAT_X32_TYPELESS_G8X24_UINT||desc.Format==DXGI_FORMAT_X24_TYPELESS_G8_UINT)planes=2;
        shader_planes_[resource]|=planes;
    }
}
void ResourceBindings::Uav(UINT reg,UINT space,ID3D12Resource* resource,const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc,UINT index) {
    auto slot=Slot(D3D12_DESCRIPTOR_RANGE_TYPE_UAV,reg,space,index);DX12_API_CALL(device_->CreateUnorderedAccessView(resource,nullptr,&desc,slot));Retain(resource);
    if(resource)shader_planes_[resource]|=3;
    TrackShaderTextureRange(resource,resource&&(DX12_API_CALL(resource->GetDesc()).Flags&D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)?
        ShaderTextureRange{0,UINT_MAX,0,UINT_MAX,3}:ShaderTextureRange{},slot.ptr);
}
void ResourceBindings::UnreachableUavTail(UINT reg,UINT space,UINT first){
    if(sealed_||recycled_)throw std::logic_error("Cannot alter recorded descriptor coverage");
    if(layout_.binding_tier_<D3D12_RESOURCE_BINDING_TIER_3)throw std::runtime_error("Sparse descriptor coverage requires resource binding tier 3");
    for(const auto& binding:layout_.bindings_)if(binding.type==D3D12_DESCRIPTOR_RANGE_TYPE_UAV&&binding.reg==reg&&binding.space==space){
        if(first>binding.count)throw std::out_of_range("Unreachable UAV tail extent");
        if(std::find(written_resources_.begin()+binding.offset,written_resources_.begin()+binding.offset+first,false)!=
            written_resources_.begin()+binding.offset+first)throw std::logic_error("Sparse UAV prefix contains unbound descriptors");
        std::fill(written_resources_.begin()+binding.offset+first,written_resources_.begin()+binding.offset+binding.count,true);
        return;
    }
    throw std::out_of_range("Unreachable UAV range absent from layout");
}
void ResourceBindings::Cbv(UINT reg,UINT space,ID3D12Resource* resource,UINT64 offset,UINT bytes,UINT index) {
    if(!resource||!bytes||bytes>65536||bytes%256||offset%256||DX12_API_CALL(resource->GetDesc()).Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||
        offset>DX12_API_CALL(resource->GetDesc()).Width||bytes>DX12_API_CALL(resource->GetDesc()).Width-offset)
        throw std::invalid_argument("Invalid constant buffer range/alignment");
    D3D12_CONSTANT_BUFFER_VIEW_DESC desc{DX12_API_CALL(resource->GetGPUVirtualAddress())+offset,bytes};
    DX12_API_CALL(device_->CreateConstantBufferView(&desc,Slot(D3D12_DESCRIPTOR_RANGE_TYPE_CBV,reg,space,index)));Retain(resource);
}
void ResourceBindings::Sampler(UINT reg,UINT space,const D3D12_SAMPLER_DESC& desc,UINT index) {
    DX12_API_CALL(device_->CreateSampler(&desc,Slot(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,reg,space,index)));
}
void ResourceBindings::Prepare(){
    if(recycled_||preparation_failed_||preparing_)throw std::logic_error("Invalid or failed native resource preparation");
    if(!before_bind_)return;
    auto prepare=std::move(before_bind_);before_bind_={};preparing_=true;
    try{prepare(*this);preparing_=false;}
    catch(...){preparing_=false;preparation_failed_=true;throw;}
}
void ResourceBindings::Bind(ID3D12GraphicsCommandList* list,bool compute) {
    if(recycled_)throw std::logic_error("Cannot bind retired descriptor heaps");
    if(!list)throw std::invalid_argument("Null command list");
    Prepare();
    if(std::find(written_resources_.begin(),written_resources_.end(),false)!=written_resources_.end()||
        std::find(written_samplers_.begin(),written_samplers_.end(),false)!=written_samplers_.end())
        throw std::logic_error("Missing reflected shader resource descriptors");
    ID3D12DescriptorHeap* heaps[2]{};UINT count=0;
    auto* resource_heap=resources_?resources_.page->heap.Get():owned_resources_.Get();
    auto* sampler_heap=samplers_?samplers_.page->heap.Get():owned_samplers_.Get();
    if(resource_heap)heaps[count++]=resource_heap;if(sampler_heap)heaps[count++]=sampler_heap;
    if(count)DX12_API_CALL(list->SetDescriptorHeaps(count,heaps));
    if(compute)DX12_API_CALL(list->SetComputeRootSignature(layout_.Root()));else DX12_API_CALL(list->SetGraphicsRootSignature(layout_.Root()));
    auto bind=[&](ID3D12DescriptorHeap* heap,UINT parameter,UINT offset,D3D12_DESCRIPTOR_HEAP_TYPE type) {
        if(!heap)return;auto handle=DX12_API_CALL(heap->GetGPUDescriptorHandleForHeapStart());
        handle.ptr+=UINT64(offset)*DX12_API_CALL(device_->GetDescriptorHandleIncrementSize(type));
        if(compute)DX12_API_CALL(list->SetComputeRootDescriptorTable(parameter,handle));else DX12_API_CALL(list->SetGraphicsRootDescriptorTable(parameter,handle));
    };
    bind(resource_heap,layout_.resource_parameter_,resources_?resources_.offset:0,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    bind(sampler_heap,layout_.sampler_parameter_,samplers_?samplers_.offset:0,D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);sealed_=true;
}
}
