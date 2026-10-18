// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_compute.h"
#include "nv_shader_clock.h"
#include "guest_sampler.h"
#include "guest_image_readback.h"
#include "guest_image_ownership.h"
#include "guest_stage_bindings.h"
#include "diagnostics.h"
#include "guest_dma.h"
#include "shader_recompiler/native_dma.h"
#ifdef SHADPS4_NATIVE_DX12_CORE
#include "core/memory.h"
#endif
#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <tuple>
namespace NativeDX12 {
namespace {
constexpr u64 MaxBytes=Shader::NativeDma::MaxBufferBytes;
std::optional<u32> FormattedMetadataValue(const GuestTextureCache::MetaDataInfo& meta){
    return GuestFormattedMetadataValue(meta);
}
struct TemporaryMemory {
    Rasterizer& rasterizer;
    explicit TemporaryMemory(Rasterizer& owner):rasterizer(owner){}
    std::vector<std::vector<u32>> storage;
    std::vector<std::shared_ptr<Rasterizer::TemporaryBuffer>> pooled;
    std::vector<std::pair<VAddr,u64>> mapped;
    void Release(){while(!mapped.empty()){auto [a,s]=mapped.back();rasterizer.UnmapMemory(a,s);mapped.pop_back();}pooled.clear();}
    ~TemporaryMemory(){try{Release();}catch(...) {/* preserve the original device/submission error */}}
    std::pair<VAddr,u64> Make(u64 bytes,const void* initial=nullptr) {
        if(!bytes||bytes>MaxBytes)throw std::invalid_argument("Native compute temporary buffer size");
        if(auto block=rasterizer.AcquireTemporaryBuffer(bytes,initial)){
            const auto address=reinterpret_cast<VAddr>(block->words.data());const auto size=u64(block->words.size())*4;
            pooled.push_back(std::move(block));return {address,size};
        }
        storage.emplace_back(size_t((bytes+3)/4),0);
        auto& data=storage.back();if(initial)memcpy(data.data(),initial,size_t(bytes));
        auto address=reinterpret_cast<VAddr>(data.data());auto size=u64(data.size()*4);
        rasterizer.MapMemory(address,size);mapped.emplace_back(address,size);return {address,size};
    }
};
}
bool ExecuteGuestCopyShader(Rasterizer& rasterizer,const Shader::Info& info,
    const ComputeInvocation& invocation) {
    if(info.pgm_hash!=0xfefebf9f)return false;
    std::lock_guard lock(rasterizer.ExecutionMutex());
    if(info.buffers.size()<3||invocation.groups[1]!=1||invocation.groups[2]!=1||
       invocation.base[0]||invocation.base[1]||invocation.base[2])
        throw std::runtime_error("Native guest copy shader has incompatible dispatch metadata");
    const auto control=info.buffers[0].GetSharp(info),source=info.buffers[1].GetSharp(info),dest=info.buffers[2].GetSharp(info);
    if(control.stride!=12||!source.stride||source.stride!=dest.stride||invocation.groups[0]>65535)
        throw std::runtime_error("Native guest copy shader has incompatible buffer strides/count");
    struct Command {u32 dest,source,end;};
    const auto control_bytes=u64(invocation.groups[0])*sizeof(Command);
    if(!control_bytes)return true;
    rasterizer.ValidateHostMemory(control.base_address,control_bytes);
    rasterizer.ReadMemory(control.base_address,control_bytes);
    std::vector<Command> commands(invocation.groups[0]);
    memcpy(commands.data(),reinterpret_cast<const void*>(control.base_address),size_t(control_bytes));
    std::vector<Rasterizer::BufferCopy> copies;
    std::vector<std::pair<VAddr,VAddr>> reads,writes;
    for(const auto& command:commands) {
        const u32 source_offset=command.source*u32(source.stride),dest_offset=command.dest*u32(dest.stride);
        const u32 bytes=(command.end+1)*u32(source.stride);
        if(!bytes)continue;
        const auto from=source.base_address+source_offset,to=dest.base_address+dest_offset;
        if(from>UINT64_MAX-bytes||to>UINT64_MAX-bytes)throw std::out_of_range("Native copy shader address overflow");
        reads.emplace_back(from,from+bytes);writes.emplace_back(to,to+bytes);
        copies.push_back({to,from,bytes});
    }
    const auto merge=[](auto& ranges){
        std::sort(ranges.begin(),ranges.end());size_t count{};
        for(const auto range:ranges){
            if(count&&range.first<=ranges[count-1].second)ranges[count-1].second=std::max(range.second,ranges[count-1].second);
            else ranges[count++]=range;
        }
        ranges.resize(count);
    };
    merge(reads);merge(writes);
    for(auto [begin,end]:reads)rasterizer.ValidateGpuBacking(begin,end-begin);
    for(auto [begin,end]:writes)rasterizer.ValidateGpuBacking(begin,end-begin,true);
    // Execute the recognized PS4 copy shader on the native copy path.  The
    // rasterizer keeps command order, uses scratch storage for overlapping
    // aliases, and reconciles only the exact destination ranges.  This avoids
    // serial CPU memmoves while preserving the shader's memmove semantics.
    Diagnostics::Count("copy-hle.dispatches");
    Diagnostics::Count("copy-hle.commands",copies.size());
    unsigned long long copied_bytes{};for(const auto& copy:copies)copied_bytes+=copy.bytes;
    Diagnostics::Count("copy-hle.bytes",copied_bytes);
    rasterizer.CopyBufferBatch(copies);
    return true;
}
static void ExecuteGuestResources(ID3D12Device* device,Rasterizer& rasterizer,
    const ShaderPipeline& pipeline,std::span<const GuestResourceStage> stages,
    const ComputeInvocation& invocation,Shader::PushData push,std::span<const std::byte> runtime_data,
    const std::function<void(ResourceBindings&)>& submit,GuestTextureCache* textures,VAddr border_colors,
    std::span<const Rasterizer::ShaderBufferRequest> read_buffers={}) {
    Diagnostics::Scope diagnostic(Diagnostics::Kind::Resources,"metadata-and-layout");
    std::lock_guard lock(rasterizer.ExecutionMutex());
    if(stages.empty()||!submit)throw std::invalid_argument("Empty native guest stage set/command");
    struct Pending {
        UINT reg;bool uav;u32 push_index,byte_adjust;bool formatted_read{};u64 original_bytes{};
        bool guest_read{};std::optional<size_t> stream_view;
        u32 access_alignment{1};
    };
    std::vector<Pending> pending;
    struct DmaPending {UINT reg;bool fault;};std::vector<DmaPending> dma_pending;
    const bool uses_dma=std::any_of(stages.begin(),stages.end(),[](const auto& stage){return stage.info&&stage.info->uses_dma;});
    auto* dma=uses_dma?&rasterizer.DmaMemory():nullptr;
    std::vector<Rasterizer::ShaderBufferRequest> requests;
    struct ImagePending {GuestTextureDesc desc;GuestTextureLayout layout;UINT reg,count,mip;bool array;bool null_resource;bool sample_storage;
        std::shared_ptr<const std::vector<std::byte>> guest_snapshot;
        std::shared_ptr<Resource> gpu_snapshot;bool buffer_writer_alias=false,current_buffer_snapshot=false;
        bool buffer_source_residency=false;
        std::optional<size_t> ordered_transfer;std::optional<size_t> storage_group;
        GuestTextureCache::Entry* ordered_entry{};};
    std::vector<ImagePending> images;
    struct ShaderImageStorage {GuestTextureDesc desc;GuestTextureLayout layout;
        std::shared_ptr<const GuestTextureCache::CachedImageBindingPlan> cached_plan;};
    std::vector<ShaderImageStorage> shader_image_storage;
    std::vector<std::shared_ptr<void>> shader_storage_pins;
    const auto storage_desc=[&](const ImagePending& image)->const GuestTextureDesc&{
        return image.storage_group?shader_image_storage[*image.storage_group].desc:image.desc;
    };
    const auto storage_layout=[&](const ImagePending& image)->const GuestTextureLayout&{
        return image.storage_group?shader_image_storage[*image.storage_group].layout:image.layout;
    };
    std::vector<std::shared_ptr<void>> writable_block_parent_pins;
    const auto find_block_parent=[textures](const GuestTextureDesc& desc,const GuestTextureLayout& layout){
        if(!textures)return static_cast<GuestTextureCache::Entry*>(nullptr);
        if(auto* parent=textures->FindBlockViewParent(desc,layout))return parent;
        return textures->FindReadonlyBlockParent(desc,layout);
    };
    const auto compatible_block_prefixes=[](GuestTextureDesc a,const GuestTextureLayout& first,
        GuestTextureDesc b,const GuestTextureLayout& second){
        // Only use this after both views have proved the same BC parent. A
        // longer transient UINT allocation can serve the shorter physical mip
        // prefix without creating another independently writable cache entry.
        if(first.block_extent!=1||second.block_extent!=1||
           first.bytes_per_pixel!=second.bytes_per_pixel||
           first.mips.size()!=a.levels||second.mips.size()!=b.levels)
            return false;
        const auto common_levels=std::min(a.levels,b.levels);
        const auto& prefix_layout=a.levels<=b.levels?first:second;
        const auto& large=a.levels<=b.levels?second:first;
        a.levels=b.levels=std::max(a.levels,b.levels);
        a.writable=b.writable=true;
        if(!CompatibleGuestWritableStorage(a,b))return false;
        if(prefix_layout.guest_bytes>large.guest_bytes)return false;
        for(UINT mip=0;mip<common_levels;++mip){
            const auto& x=prefix_layout.mips[mip];const auto& y=large.mips[mip];
            if(x.pitch!=y.pitch||x.height!=y.height||x.offset!=y.offset||x.slice_bytes!=y.slice_bytes||
               x.macro!=y.macro||x.thickness!=y.thickness)return false;
        }
        return true;
    };
    struct SamplerPending {UINT reg;D3D12_SAMPLER_DESC desc;};std::vector<SamplerPending> samplers;
    auto temporary_lease=std::make_shared<TemporaryMemory>(rasterizer);
    auto& temporary=*temporary_lease;
    std::set<std::pair<UINT,UINT>> covered;
    std::vector<GuestStageBindingSpan> binding_stages;
    for(const auto& stage:stages) {
    const auto first_buffer=requests.size(),first_image=images.size();
    if(!stage.info)throw std::invalid_argument("Null native guest stage metadata");
    const auto& info=*stage.info;const auto start=stage.bindings;
    if(pipeline.IsCompute()!=(info.sw_stage==Shader::SwStage::Compute))throw std::invalid_argument("Native guest command stage mismatch");
    // FMASK operations do not survive resource patching as descriptor-backed
    // image operations.  Reads/sample-raw are replaced with the hardware
    // identity mapping and dimension queries with specialization constants.
    // Keep the metadata in StageSpecialization (it selects the correct shader
    // variant), but deliberately allocate no D3D12 descriptor for it.
    if(!info.images.empty()&&!textures)throw std::runtime_error("Native guest image cache not supplied");
    if(start.buffer>Shader::NUM_BUFFERS||info.buffers.size()>Shader::NUM_BUFFERS-start.buffer)
        throw std::out_of_range("Native guest push buffer offset index");
    bool has_guest_srv=false,has_guest_uav=false;
    for(size_t index=0;index<info.buffers.size();++index)if(info.buffers[index].buffer_type==Shader::BufferType::Guest)
        for(const auto& binding:pipeline.Layout().Bindings())if(binding.space==0&&
            u64(start.unified)+index>=binding.reg&&u64(start.unified)+index-u64(binding.reg)<binding.count){
            has_guest_srv|=binding.type==D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            has_guest_uav|=binding.type==D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        }
    for(size_t index=0;index<info.buffers.size();++index) {
        if(index>UINT_MAX-start.unified)throw std::out_of_range("Native compute register overflow");
        const auto reg=start.unified+UINT(index);
        const ShaderBinding* reflected=nullptr;
        for(const auto& binding:pipeline.Layout().Bindings()) {
            if(binding.space==0&&reg>=binding.reg&&reg-binding.reg<binding.count&&
               (binding.type==D3D12_DESCRIPTOR_RANGE_TYPE_SRV||binding.type==D3D12_DESCRIPTOR_RANGE_TYPE_UAV)) {
                if(reflected)throw std::runtime_error("Ambiguous native guest buffer access class");
                reflected=&binding;
            }
        }
        const auto& source=info.buffers[index];
        // Pinned BindBuffers still prepares optimized-away guest sharps: their
        // invalidation, dirty-page and formatted-retile effects remain visible.
        if(!reflected&&source.buffer_type!=Shader::BufferType::Guest)continue;
        // Production SSBOs are unified UAVs. Keep SRV-only conversion callers'
        // readonly access class when a live guest binding establishes it.
        const bool uav=reflected?reflected->type==D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
            source.is_written||has_guest_uav||!has_guest_srv;
        if(source.is_written&&!uav)throw std::runtime_error("Native writable guest buffer reflected as SRV");
        VAddr address{};u64 size{},original_bytes{};u32 adjust{};
        switch(source.buffer_type) {
        case Shader::BufferType::Guest: {
            auto sharp=source.GetSharp(info);
            size=sharp.GetSize();
            if(!sharp.base_address||!size){std::tie(address,size)=temporary.Make(16);break;}
            size=rasterizer.ClampMemoryRange(sharp.base_address,size);
            if(size>MaxBytes)throw std::out_of_range("Native guest shader buffer exceeds slice budget: address="+
                std::to_string(sharp.base_address)+" bytes="+std::to_string(size)+
                " stride="+std::to_string(sharp.stride)+" records="+std::to_string(sharp.num_records)+
                " shader="+std::to_string(info.pgm_hash)+" binding="+std::to_string(index));
            original_bytes=size;adjust=u32(sharp.base_address&3);address=sharp.base_address&~u64(3);
            size=(size+adjust+3)&~u64(3);break;
        }
        case Shader::BufferType::Flatbuf:
            if(source.is_written)throw std::runtime_error("Native flat user data cannot be written");
            if(info.flattened_ud_buf.empty())std::tie(address,size)=temporary.Make(16);
            else std::tie(address,size)=temporary.Make(info.flattened_ud_buf.size()*4,info.flattened_ud_buf.data());
            break;
        case Shader::BufferType::GdsBuffer:address=rasterizer.GdsAddress();size=65536;break;
        case Shader::BufferType::BorderColors: {
            std::array<std::array<float,4>,Shader::NUM_SAMPLERS> colors{};
            for(size_t sampler_index=0;sampler_index<info.samplers.size();++sampler_index){
                const auto& sampler_source=info.samplers[sampler_index];
                const auto sharp=sampler_source.GetSharp(info);
                if(!sampler_source.native_custom_border)continue;
                const u64 offset=u64(sharp.border_color_ptr)*sizeof(colors[0]);
                if(!border_colors||border_colors>UINT64_MAX-offset-sizeof(colors[0]))
                    throw std::runtime_error("Native custom border color table is missing or overflows");
                const auto color_address=border_colors+offset;
                rasterizer.ReadMemory(color_address,sizeof(colors[0]));
                rasterizer.ValidateHostMemory(color_address,sizeof(colors[0]));
                std::memcpy(colors[sampler_index].data(),reinterpret_cast<const void*>(color_address),sizeof(colors[0]));
            }
            std::tie(address,size)=temporary.Make(sizeof(colors),colors.data());
            break;
        }
        case Shader::BufferType::SharedMemory:
            std::tie(address,size)=temporary.Make(std::max<u64>(16,invocation.shared_bytes));break;
        case Shader::BufferType::ClipPlanes:
            if(source.is_written)throw std::runtime_error("Native clip planes cannot be written");
            if(invocation.clip_planes.empty())std::tie(address,size)=temporary.Make(16);
            else std::tie(address,size)=temporary.Make(invocation.clip_planes.size_bytes(),invocation.clip_planes.data());
            break;
        case Shader::BufferType::BdaPagetable:
        case Shader::BufferType::FaultBuffer:
            if(!dma||!uav)throw std::runtime_error("Native DMA metadata requires logical UAV addressing");
            dma_pending.push_back({reg,source.buffer_type==Shader::BufferType::FaultBuffer});
            covered.emplace(reg,0);continue;
        default:throw std::runtime_error("Native guest direct-memory-addressing buffer is not connected");
        }
        const u32 access_alignment=True(source.used_types&(Shader::IR::Type::U64|Shader::IR::Type::F64))?8:
            True(source.used_types&(Shader::IR::Type::U32|Shader::IR::Type::F32))?4:
            True(source.used_types&Shader::IR::Type::U16)?2:1;
        requests.push_back({address,size,source.is_written,uav,false,u8(adjust),original_bytes});
        pending.push_back({reg,uav,start.buffer+u32(index),adjust,
            source.buffer_type==Shader::BufferType::Guest&&source.is_formatted&&!source.is_written,original_bytes,
            source.buffer_type==Shader::BufferType::Guest&&!source.is_written,{},access_alignment});covered.emplace(reg,0);
    }
    u64 image_reg=u64(start.unified)+info.buffers.size();
    for(const auto& source:info.images) {
        const auto count=source.NumBindings(info);
        if(!count||image_reg>UINT_MAX-count)throw std::out_of_range("Native image descriptor register span");
        const auto type=source.is_written?D3D12_DESCRIPTOR_RANGE_TYPE_UAV:D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        std::optional<D3D_SRV_DIMENSION> reflected_dimension;
        for(const auto& b:pipeline.Layout().Bindings())if(b.type==type&&b.space==0)
            for(UINT i=0;i<count;++i)if(image_reg+i>=b.reg&&image_reg+i-b.reg<b.count){
                covered.emplace(UINT(image_reg+i),0);
                if(reflected_dimension&&*reflected_dimension!=b.dimension)
                    throw std::runtime_error("Native image mip descriptors have different resource dimensions");
                reflected_dimension=b.dimension;
            }
        // Pinned BindTextures prepares every original image slot: refresh,
        // tracking and conservative storage ownership survive shader liveness.
        // Only descriptor writes below depend on actual native reflection.
        {
            const auto sharp=source.GetSharp(info);
            const bool array=source.is_array||sharp.GetType()==AmdGpu::ImageType::Cube;
            // Direct/imported shaders can retain Texture2D declarations.
            // Promote only when actual DXIL agrees; dead original slots still
            // use the native ABI for conservative ownership preparation.
            const bool slice_view=source.NeedsNativeSliceView(info)&&(!reflected_dimension||
                *reflected_dimension==D3D_SRV_DIMENSION_TEXTURE2DARRAY||
                *reflected_dimension==D3D_SRV_DIMENSION_TEXTURE2DMSARRAY);
            auto desc=DescribeGuestTexture(sharp);desc.writable=source.is_written;
            desc.native_packed10_raw=source.native_packed10_raw;
            desc.native_raw32=source.native_raw32;
            // Original GCN IR lowers PS4 1D image operations to 2D with Y=0.
            // The native view must match that emitted DXIL resource dimension.
            if(desc.one_dimensional)desc.one_dimensional=false;
            if(desc.layers>1&&!array){
                if(!slice_view)throw std::runtime_error("Unsupported native non-array layer view");
                desc.view_layers=1;
            }
            auto layout=LayoutGuestTexture(desc);
            if(sharp&&count==1)if(auto view=textures->RebaseBlockSubresource(desc,layout,array||slice_view)){
                desc=*view;layout=LayoutGuestTexture(desc);
            }
            if(desc.writable)textures->ValidateWritableFormat(layout.format);
            auto* block_parent=sharp?find_block_parent(desc,layout):nullptr;
            if(block_parent)writable_block_parent_pins.push_back(textures->PinBinding(*block_parent));
            UINT mip=desc.first_mip;
            if(source.mip_fallback_mode==Shader::MipStorageFallbackMode::ConstantIndex)mip+=source.constant_mip_index;
            if(mip>=desc.levels||(!source.native_msaa_storage&&count>desc.levels-mip)||
               (source.native_msaa_storage&&count!=desc.samples))throw std::out_of_range("Native image descriptor span");
            for(const auto& previous:images)if(sharp&&!previous.null_resource&&previous.desc.address<desc.address+layout.guest_bytes&&desc.address<previous.desc.address+previous.layout.guest_bytes){
                auto* depth_parent=!desc.writable&&(desc.data_format==2||desc.data_format==4)?
                    textures->FindDepthViewParent(desc,layout):nullptr;
                const bool depth_views=!previous.desc.writable&&!desc.writable&&depth_parent&&depth_parent->desc.depth_target&&
                    CompatibleGuestDepthSubview(depth_parent->desc,depth_parent->layout,previous.desc,previous.layout)&&
                    CompatibleGuestDepthSubview(depth_parent->desc,depth_parent->layout,desc,layout);
                const bool converted_views=(!previous.desc.writable||!desc.writable)&&
                    desc.data_format==9&&CompatibleGuestPackedStorage(previous.desc,desc);
                const bool block_views=block_parent&&
                    find_block_parent(previous.desc,previous.layout)==block_parent&&
                    ((layout.block_extent==4||previous.layout.block_extent==4)?
                     ((layout.block_extent==4&&previous.layout.block_extent==4)||
                      (pipeline.IsCompute()&&stages.size()==1&&!uses_dma)):
                     compatible_block_prefixes(previous.desc,previous.layout,desc,layout));
                if(previous.desc!=desc&&!CompatibleGuestShaderStorage(previous.desc,desc)&&
                    !textures->CanShareReadonlyWordStorage(previous.desc,desc)&&!converted_views&&!block_views&&!depth_views&&
                    !CompatibleGuestShaderMipPrefixes(previous.desc,previous.layout,desc,layout)&&
                    !CompatibleGuestBlockSampledViews(previous.desc,previous.layout,desc,layout))
                    throw std::runtime_error("Native image aliases with different storage layouts unsupported");
                if(previous.desc.writable!=desc.writable){
                    const auto previous_end=previous.desc.writable?previous.mip+previous.count:previous.desc.levels;
                    const auto current_end=desc.writable?mip+count:desc.levels;
                    if(previous.mip<current_end&&mip<previous_end&&
                       previous.desc.first_layer<desc.first_layer+desc.view_layers&&
                       desc.first_layer<previous.desc.first_layer+previous.desc.view_layers)
                        throw std::runtime_error("Native overlapping SRV/UAV image subresources require shader access unification");
                }
            }
            images.push_back({desc,std::move(layout),UINT(image_reg),count,mip,array||slice_view,!bool(sharp),source.native_msaa_storage,{},{}});
        }
        image_reg+=count;
    }
    for(const auto& source:info.samplers) {
        if(image_reg>UINT_MAX)throw std::out_of_range("Native sampler register overflow");
        for(const auto& b:pipeline.Layout().Bindings())if(b.type==D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER&&b.space==0&&image_reg>=b.reg&&image_reg-b.reg<b.count) {
            const auto sharp=source.GetSharp(info);const float* custom{};
            if(sharp.border_color_type.Value()==AmdGpu::BorderColor::Custom){
                if(!border_colors)throw std::runtime_error("Native guest custom border table base is null");
                const auto offset=u64(sharp.border_color_ptr.Value())*sizeof(float)*4;
                if(border_colors>UINT64_MAX-offset)throw std::out_of_range("Native guest border table address overflow");
                const auto address=border_colors+offset;rasterizer.ReadMemory(address,sizeof(float)*4);
                rasterizer.ValidateHostMemory(address,sizeof(float)*4);custom=reinterpret_cast<const float*>(address);
            }
            samplers.push_back({UINT(image_reg),DescribeGuestSampler(sharp,source.is_depth,custom)});
            covered.emplace(UINT(image_reg),0);
        }
        ++image_reg;
    }
    binding_stages.push_back({first_buffer,requests.size(),first_image,images.size()});
    }
    // Plan the maximum physical chain before validation, snapshots or uploads.
    // Original metadata/descriptors keep their own mip count and view window.
    // Include an already owned larger chain so a narrow writable bind does not
    // discard its untouched I tail or initialize it from unrelated B bytes.
    if(textures)for(auto& image:images){
        if(!image.null_resource&&image.desc.writable){
            const auto storage=textures->ResolveDepthViewStorage(image.desc);
            if(storage!=image.desc){
                image.storage_group=shader_image_storage.size();
                shader_image_storage.push_back({storage,LayoutGuestTexture(storage)});continue;
            }
        }
        if(!image.null_resource&&!image.desc.writable&&(image.desc.data_format==2||image.desc.data_format==4)){
            auto* parent=textures->FindDepthViewParent(image.desc,image.layout);
            if(parent&&CompatibleGuestDepthSubview(parent->desc,parent->layout,image.desc,image.layout)){
                // Keep original image.desc for shader dimension/range/swizzle.
                // Only the physical storage/envelope follows the grown parent.
                image.storage_group=shader_image_storage.size();
                shader_image_storage.push_back({parent->desc,parent->layout});
                shader_storage_pins.push_back(textures->PinBinding(*parent));continue;
            }
        }
        if(image.null_resource||!CompatibleGuestShaderMipPrefixes(image.desc,image.layout,image.desc,image.layout))continue;
        auto found=std::ranges::find_if(shader_image_storage,[&](const auto& group){
            return CompatibleGuestShaderMipPrefixes(group.desc,group.layout,image.desc,image.layout);
        });
        if(found==shader_image_storage.end()){
            image.storage_group=shader_image_storage.size();
            shader_image_storage.push_back({image.desc,image.layout});
        }else{
            image.storage_group=size_t(found-shader_image_storage.begin());
            const auto levels=std::max(found->desc.levels,image.desc.levels);
            if(image.desc.writable&&!found->desc.writable)found->desc=image.desc;
            found->desc.levels=levels;found->layout=LayoutGuestTexture(found->desc);
        }
    }
    if(textures)for(auto& group:shader_image_storage){
        for(const auto& cached:textures->Entries())if(cached->gpu_owned&&
            CompatibleGuestImageMipGeometry(group.desc,group.layout,cached->desc,cached->layout)){
            shader_storage_pins.push_back(textures->PinBinding(*cached));
            if(cached->desc.levels>group.desc.levels){
                group.desc.levels=cached->desc.levels;group.layout=LayoutGuestTexture(group.desc);
            }
        }
    }
    if(textures)for(const auto& image:images)if(!image.null_resource){
        const auto& desc=storage_desc(image);const auto& layout=storage_layout(image);
        const bool owned_prefix=std::ranges::any_of(textures->Entries(),[&](const auto& cached){
            return cached->gpu_owned&&CompatibleGuestImageMipGeometry(desc,layout,cached->desc,cached->layout);
        });
        if(!owned_prefix&&!textures->HasOwnedShaderStorage(desc)){
            if(find_block_parent(image.desc,image.layout)||
               textures->FindImageRecreationBindingSource(desc,layout))
                rasterizer.ValidateGpuBacking(desc.address,layout.guest_bytes,image.desc.writable);
            else rasterizer.ValidateHostMemory(desc.address,layout.guest_bytes,image.desc.writable);
        }
    }
    // Retain only the already published original I before classifying streams
    // or collecting formatted readers. No future target is allocated/bound.
    if(textures)for(auto& group:shader_image_storage)
        group.cached_plan=textures->PlanCachedImageBinding(group.desc,group.layout);
    const auto classify_cpu_stream=[&](size_t index){
        if(!pending[index].guest_read||!pending[index].original_bytes||pending[index].original_bytes>16384)
            return false;
        const auto source=requests[index].WriteRange();
        if(source.address>UINT64_MAX-source.size)
            throw std::out_of_range("Ordered buffer stream range overflows");
        if(rasterizer.IsGpuBufferModified(source.address,source.size))return false;
        const auto first_page=source.address>>12,last_page=(source.address+source.size-1)>>12;
        // Pinned ObtainBuffer marks each writer's original range before binding
        // the next descriptor. Even byte-disjoint writes can share query pages.
        for(size_t j=0;j<index;++j)if(requests[j].written){
            const auto write=requests[j].WriteRange();
            if(!write.size||write.address>UINT64_MAX-write.size)
                throw std::out_of_range("Ordered buffer writer range overflows");
            if((write.address>>12)<=last_page&&first_page<=((write.address+write.size-1)>>12))
                return false;
        }
        return true;
    };
    const auto ordinary_count=requests.size();
    std::vector<bool> cpu_streams;cpu_streams.reserve(ordinary_count);
    for(size_t i=0;i<ordinary_count;++i){
        cpu_streams.push_back(classify_cpu_stream(i));
        requests[i].publish_dma_residency=pending[i].original_bytes&&
            (requests[i].written||!cpu_streams.back());
    }
    const auto streams_before_binding=[&](size_t index){return bool(cpu_streams.at(index));};
    std::vector<Rasterizer::ShaderBufferRequest> dma_reads;
    if(dma){
        // Completed faults become actual B residency before the stage effects,
        // as in the original frame fault pass. Future sharp predictions below
        // must not publish or materialize an original small CPU-only stream.
        dma->MaterializeResidentBacking(rasterizer);
        std::vector<Rasterizer::ShaderBufferRequest> resident_sharps;
        for(size_t i=0;i<ordinary_count;++i)if(pending[i].original_bytes&&
            (requests[i].written||!streams_before_binding(i)))resident_sharps.push_back(requests[i]);
        // Raw ReadConst ranges are physical B, not original BindBuffers slots.
        // They must not resolve an independently owned I in the shader batch.
        dma->AppendRequests(rasterizer,dma_reads,resident_sharps);
    }
    for(const auto& buffer:read_buffers){
        if(buffer.written||buffer.unordered_access||!buffer.resident)
            throw std::invalid_argument("Graphics auxiliary buffer must be a resident read-only input");
        (void)buffer.WriteRange();
    }
    const auto validate_writable_block_alias=[&](const ImagePending& image,const GuestTextureCache::Entry& parent){
        // All UINT views of this parent bind one shared transient resource.
        // Readonly subresources must remain disjoint from every writer, even
        // when a conflicting original slot is absent from native reflection.
        if(!pipeline.IsCompute()||stages.size()!=1||uses_dma||
           image.count!=1||image.mip!=image.desc.first_mip||image.sample_storage)
            throw std::runtime_error("Writable BC block views require selected compute mips without DMA");
        const auto overlaps=[](VAddr a,u64 as,VAddr b,u64 bs){return as&&bs&&(a<=b?b-a<as:a-b<bs);};
        for(const auto& request:requests)if(overlaps(request.address,request.size,parent.desc.address,parent.layout.guest_bytes))
            throw std::runtime_error("Writable BC block view cannot overlap any buffer request across its full parent storage");
        for(const auto& other:images)if(!other.null_resource&&
            overlaps(other.desc.address,other.layout.guest_bytes,parent.desc.address,parent.layout.guest_bytes)){
            const bool readonly_bc=!other.desc.writable&&other.layout.block_extent==4;
            if(other.count!=1||other.mip!=other.desc.first_mip||other.sample_storage||
               find_block_parent(other.desc,other.layout)!=&parent||
               (!readonly_bc&&!compatible_block_prefixes(image.desc,image.layout,other.desc,other.layout)))
                throw std::runtime_error("Writable BC block views cannot mix parent storage with other image domains");
            else if(!other.desc.writable&&image.mip<other.desc.levels&&other.mip<image.mip+image.count&&
                image.desc.first_layer<other.desc.first_layer+other.desc.view_layers&&
                other.desc.first_layer<image.desc.first_layer+image.desc.view_layers)
                throw std::runtime_error("Overlapping readonly/writable BC views require shader access unification");
        }
    };
    // Pin the authoritative full BC image before buffer preparation or logical
    // invalidation can retire an unrelated overlap or reserve cache storage.
    if(textures)for(const auto& image:images)if(!image.null_resource&&image.desc.writable)
        if(auto* parent=textures->FindWritableBlockParent(image.desc,image.layout)){
            validate_writable_block_alias(image,*parent);
            writable_block_parent_pins.push_back(textures->PinBinding(*parent));
        }
    const auto stage_image_domain=[&](const ImagePending& image){
        if(image.null_resource)return false;
        if(storage_desc(image).writable)return true;
        return textures&&std::ranges::any_of(textures->Entries(),[&](const auto& cached){return cached->logical_gpu_modified&&
            CompatibleGuestImageMipGeometry(storage_desc(image),storage_layout(image),cached->desc,cached->layout);});
    };
    bool ordered_stages=stages.size()>1&&textures&&(
        std::ranges::any_of(images,[&](const auto& image){return stage_image_domain(image)&&
            std::ranges::any_of(std::span(requests).first(ordinary_count),[&](const auto& buffer){
                const auto index=size_t(&buffer-requests.data());
                return (buffer.written||(pending[index].formatted_read&&!streams_before_binding(index)))&&
                    buffer.address<storage_desc(image).address+storage_layout(image).guest_bytes&&
                    storage_desc(image).address<buffer.address+buffer.size;});})||
        std::ranges::any_of(textures->Entries(),[&](const auto& image){return image->gpu_owned&&image->HasValidLogicalImage()&&
            std::ranges::any_of(std::span(requests).first(ordinary_count),[&](const auto& buffer){
                const auto index=size_t(&buffer-requests.data());return pending[index].formatted_read&&!streams_before_binding(index)&&
                    buffer.address<image->desc.address+image->layout.guest_bytes&&image->desc.address<buffer.address+buffer.size;});}));
    // Keep original stage ordering for real shader resources. Raw DMA will
    // materialize/synchronize physical B after those semantic effects, without
    // requiring a format conversion or independent-image ownership transfer.
    ordered_stages|=uses_dma&&textures;
    // The original binds buffers and refreshes sampled images before execution.
    // Two writers likewise keep separate cache domains, when the backend can
    // retain both versions without selecting an arbitrary physical winner.
    for(auto& image:images)for(const auto& b:requests)
        if(!image.null_resource&&b.address<storage_desc(image).address+storage_layout(image).guest_bytes&&storage_desc(image).address<b.address+b.size&&
            storage_desc(image).writable&&b.written){
            if(!textures||!textures->CanRetainBufferAlias(storage_desc(image),storage_layout(image))||
               (b.WriteRange().address!=image.desc.address&&stages.size()!=1&&!ordered_stages))
                throw std::runtime_error("Native cross-stage writable buffer/image alias needs shared cache coherency: buffer="+
                std::to_string(b.address)+" bytes="+std::to_string(b.size)+" written="+std::to_string(b.written)+
                " image="+std::to_string(image.desc.address)+" bytes="+std::to_string(image.layout.guest_bytes)+" writable="+std::to_string(image.desc.writable));
            image.buffer_writer_alias=true;
        }
    // A buffer touching only a cached parent's extra mips still shares its
    // physical domain. Preserve that valid I before buffer preparation can
    // acquire the same pages. Exact-base invalidation below remains original.
    if(textures)for(const auto& group:shader_image_storage){
        const bool writer=std::ranges::any_of(requests,[&](const auto& buffer){return buffer.written&&
            buffer.address<group.desc.address+group.layout.guest_bytes&&group.desc.address<buffer.address+buffer.size;});
        if(!writer)continue;
        for(const auto& cached:textures->Entries())if(cached->gpu_owned&&!cached->buffer_alias_version&&
            cached->HasValidLogicalImage()&&CompatibleGuestImageMipGeometry(group.desc,group.layout,cached->desc,cached->layout)){
            if(!textures->RevalidateReadonly(*cached))
                throw std::runtime_error("Mip-prefix buffer/image parent cannot retain its independent version");
        }
    }
    const auto separate_overlap=[&](VAddr address,u64 bytes){
        if(textures)for(const auto& entry:textures->Entries())if(entry->gpu_owned&&entry->buffer_alias_version&&
            address<entry->desc.address+entry->layout.guest_bytes&&entry->desc.address<address+bytes)return true;
        return std::ranges::any_of(images,[&](const auto& image){return image.buffer_writer_alias&&
            address<storage_desc(image).address+storage_layout(image).guest_bytes&&storage_desc(image).address<address+bytes;});
    };
    std::vector<std::pair<size_t,GuestTextureCache::Entry*>> formatted_images;
    std::vector<std::shared_ptr<void>> formatted_image_pins;
    std::vector<std::pair<size_t,u32>> formatted_metadata;
    struct OrderedFormattedImage {
        VAddr address;u64 bytes;GuestTextureCache::Entry* image;Resource tiled;std::vector<size_t> readers;
    };
    std::vector<OrderedFormattedImage> ordered_formatted_images;
    bool ordered_formatted_writer=false;
    if(textures&&!ordered_stages)for(size_t i=0;i<ordinary_count;++i)if(pending[i].formatted_read&&
        !streams_before_binding(i)){
        if(const auto meta=textures->GetMetaInfo(requests[i].address+pending[i].byte_adjust))
            if(const auto value=FormattedMetadataValue(*meta)){
                if(pending[i].byte_adjust)throw std::runtime_error("Formatted metadata synthesis requires DWORD-aligned original sharp");
                requests[i].resident=true;formatted_metadata.emplace_back(i,*value);
            }
    }
    for(size_t i=0;i<ordinary_count;++i)if(!ordered_stages&&separate_overlap(requests[i].address,requests[i].size)){
        if(pending[i].guest_read&&std::ranges::none_of(formatted_metadata,[i](const auto& item){return item.first==i;})){
            const bool needs_sync=pending[i].formatted_read&&!streams_before_binding(i);
            const auto request_writer=std::ranges::find_if(requests,[&](const auto& buffer){
                return buffer.written&&buffer.address<requests[i].address+requests[i].size&&
                    requests[i].address<buffer.address+buffer.size;});
            const bool overlaps_writer=request_writer!=requests.end();
            // Raw-only commands need the same early clean CPU stream, even when
            // no formatted descriptor triggers image synchronization afterward.
            if(!pending[i].formatted_read&&(!overlaps_writer||i>=size_t(request_writer-requests.begin())||
               !streams_before_binding(i)))continue;
            if(!needs_sync&&!overlaps_writer)continue;
            GuestTextureCache::Entry* version{};size_t candidates{};
            if(textures)for(const auto& entry:textures->Entries())if(entry->gpu_owned&&entry->buffer_alias_version&&
                entry->desc.address==requests[i].address){
                ++candidates;
                if(!version||(version->layout.guest_bytes!=pending[i].original_bytes&&entry->layout.guest_bytes==pending[i].original_bytes))
                    version=entry.get();
            }
            if(candidates>1&&version->layout.guest_bytes!=pending[i].original_bytes)
                throw std::runtime_error("Formatted buffer has ambiguous independent image versions");
            if(!version||pending[i].byte_adjust)
                throw std::runtime_error("Independent formatted image/buffer partial or simultaneous-write ordering is not adapted");
            if(version->logical_dirty)
                throw std::runtime_error("Formatted buffer cannot read an invalidated independent image version");
            const auto first_writer=std::ranges::find_if(requests,[&](const auto& buffer){
                return buffer.written&&buffer.address<version->desc.address+version->layout.guest_bytes&&
                    version->desc.address<buffer.address+buffer.size;});
            if(first_writer!=requests.end()){
                // Pinned InvalidateMemoryFromGPU only dirties an image at the
                // writer's original sharp base. An interior writer marks buffer
                // pages but leaves I valid, even when bound before this reader.
                const auto invalidating_writer=std::ranges::find_if(requests,[&](const auto& buffer){
                    return buffer.written&&buffer.WriteRange().address==version->desc.address;
                });
                const bool reads_image=pending[i].formatted_read&&
                    (invalidating_writer==requests.end()||i<size_t(invalidating_writer-requests.begin()));
                const auto ownership=DescribeGuestImageOwnership(*version);
                const auto& layout=version->layout;const auto& desc=version->desc;
                // Earlier raw reads also stream a small clean CPU snapshot;
                // a later formatted read must not turn them into arena aliases.
                const bool clean_small=streams_before_binding(i);
                const auto native=version->texture.handle?version->texture.handle->GetDesc():D3D12_RESOURCE_DESC{};
                const bool exact_shader_bounds=std::ranges::any_of(pipeline.Layout().Bindings(),[](const auto& binding){
                    return binding.type==D3D12_DESCRIPTOR_RANGE_TYPE_CBV&&binding.space==Shader::NATIVE_BUFFER_BOUNDS_SET;
                });
                const bool compatible_uavs=std::ranges::all_of(requests,[&](const auto& buffer){
                    if(buffer.address>=desc.address+layout.guest_bytes||desc.address>=buffer.address+buffer.size)return true;
                    const auto original=buffer.WriteRange();
                    if(!buffer.unordered_access||!original.size)return false;
                    if(!buffer.written)return buffer.address==desc.address&&
                        original.address==buffer.address&&original.size==buffer.size;
                    if(original.address!=buffer.address||original.size!=buffer.size){
                        const auto index=size_t(&buffer-requests.data());
                        // The pinned emitter independently floors base/address
                        // for each scalar width. Do not admit a new unaligned
                        // wider alias whose stores precede its logical dirty range.
                        if(!exact_shader_bounds||index>=pending.size()||
                           (original.address&(pending[index].access_alignment-1)))return false;
                    }
                    if(buffer.address==desc.address&&original.address==buffer.address)return true;
                    // The DWORD envelope shares B; native shader bounds retain
                    // the exact byte/halfword sharp, and narrow-store lowering
                    // preserves the other bytes. Invalidation uses the original
                    // base, including a sub-DWORD base in the image's first word.
                    // Offset readers still retain the pinned base assertion.
                    return buffer.address>=desc.address&&original.address>desc.address&&
                        buffer.size<=desc.address+layout.guest_bytes-buffer.address;
                });
                // GetArena in the pinned backend guarantees whole 4-GiB VA
                // pages only through this reader's end. A later full writer
                // cannot retroactively extend an earlier image-to-buffer copy.
                const bool image_fits_reader_arena=layout.guest_bytes&&pending[i].original_bytes&&
                    layout.guest_bytes<=UINT64_MAX-desc.address&&
                    pending[i].original_bytes<=UINT64_MAX-desc.address&&
                    ((desc.address+layout.guest_bytes-1)>>32)<=
                        ((desc.address+pending[i].original_bytes-1)>>32);
                // Without a full-image writer, the reader must already select
                // every image sparse block. Actual blocks are powers of two at
                // least 16 KiB, so this minimum-block bound is sufficient.
                const bool image_fits_reader_blocks=image_fits_reader_arena&&
                    ((desc.address+layout.guest_bytes-1)>>14)<=
                        ((desc.address+pending[i].original_bytes-1)>>14);
                // Preserve the existing same-submit full-writer case. A longer
                // writer may cover the image too, but must not require another
                // arena page beyond the earlier reader's guaranteed capacity.
                const bool covering_writer=image_fits_reader_arena&&std::ranges::any_of(requests,[&](const auto& buffer){
                    return buffer.written&&buffer.address==desc.address&&buffer.size>=layout.guest_bytes&&
                        buffer.size<=UINT64_MAX-desc.address&&
                        ((desc.address+buffer.size-1)>>32)<=
                            ((desc.address+pending[i].original_bytes-1)>>32);
                });
                const bool unique=std::ranges::none_of(textures->Entries(),[&](const auto& image){
                    return image.get()!=version&&image->gpu_owned&&image->buffer_alias_version&&
                        image->desc.address<desc.address+layout.guest_bytes&&
                        desc.address<image->desc.address+image->layout.guest_bytes;
                });
                // BindTextures follows BindBuffers in the pinned stage. Matching
                // image descriptors consume I (or the refreshed prewrite B),
                // while buffer descriptors keep their own arena/stream version.
                // Distinct physical interpretations still need their transfer
                // plan; do not infer coherence merely from overlapping addresses.
                const bool compatible_images=std::ranges::all_of(images,[&](const auto& image){
                    if(image.null_resource||image.desc.address>=desc.address+layout.guest_bytes||
                       desc.address>=image.desc.address+image.layout.guest_bytes)return true;
                    return (CompatibleGuestShaderStorage(desc,storage_desc(image))&&
                        MatchesGuestMipLayout(layout,storage_layout(image))||
                        (desc.levels<=storage_desc(image).levels&&CompatibleGuestShaderMipPrefixes(
                            desc,layout,storage_desc(image),storage_layout(image))))&&
                        (!storage_desc(image).writable||textures->CanRetainBufferAlias(storage_desc(image),storage_layout(image)));
                });
                if(!pipeline.IsCompute()||stages.size()!=1||uses_dma||!read_buffers.empty()||
                   !compatible_images||candidates!=1||!unique||!compatible_uavs||
                   !pending[i].original_bytes||pending[i].original_bytes!=requests[i].size||
                   (reads_image&&!clean_small&&(!image_fits_reader_arena||
                    (!image_fits_reader_blocks&&!covering_writer)))||
                   textures->GetMetaInfo(desc.address)||!ownership||
                   !CanRetainGuestImageOwnership(*textures,*ownership)||
                   native.SampleDesc.Count!=desc.samples||
                   (desc.samples>1&&!GuestTextureCache::CanTransferIndependentMultisample(desc,layout))||
                   (layout.expanded_bytes_per_pixel&&(!HasExactPackedExpansion(desc,layout)||
                    !version->expanded_guest_backing.handle||!version->expanded_written_mask.handle||
                    !version->expanded_backing_bytes)))
                    throw std::runtime_error("Ordered buffer/image aliases require compatible UAV ranges and proven transfer capacity/residency");
                ordered_formatted_writer=true;
                requests[i].resident=true;
                if(reads_image&&!clean_small){
                    auto transfer=std::ranges::find_if(ordered_formatted_images,
                        [version](const auto& item){return item.image==version;});
                    if(transfer==ordered_formatted_images.end()){
                        formatted_image_pins.push_back(textures->PinBinding(*version));
                        ordered_formatted_images.push_back({desc.address,layout.guest_bytes,version,{},{i}});
                    }else transfer->readers.push_back(i);
                }
                continue;
            }
            if(!needs_sync)continue;
            requests[i].resident=true;
            formatted_image_pins.push_back(textures->PinBinding(*version));
            formatted_images.emplace_back(i,version);
            // Original arenas may fit the whole image beyond a shorter sharp.
            // Prepare that transfer range without enlarging the shader view.
            if(version->layout.guest_bytes>requests[i].size&&std::ranges::none_of(requests,[&](const auto& buffer){
                return !buffer.written&&buffer.resident&&buffer.address==version->desc.address&&
                    buffer.size>=version->layout.guest_bytes&&buffer.unordered_access==requests[i].unordered_access;}))
            {
                requests.push_back({version->desc.address,version->layout.guest_bytes,false,requests[i].unordered_access,true});
                requests.back().publish_dma_residency=false;
            }
        }
    }
    // A short writer does not prepare the whole retile destination by itself.
    // Add only a readonly residency envelope after descriptor-order planning;
    // original sharp bounds, write ranges and pending indices remain unchanged.
    for(const auto& transfer:ordered_formatted_images)
        if(std::ranges::none_of(requests,[&](const auto& buffer){
            return buffer.unordered_access&&buffer.address==transfer.address&&buffer.size>=transfer.bytes;}))
        {
            requests.push_back({transfer.address,transfer.bytes,false,true,true});
            requests.back().publish_dma_residency=false;
        }
    for(const auto& binding:pipeline.Layout().Bindings()) {
        if(binding.nv_clock||binding.amd_clock)continue;
        // Native capture/replay storage is supplied by the graphics callback,
        // never interpreted as a guest sharp or mapped into PageWatch.
        if(!pipeline.IsCompute()&&binding.space==33&&binding.reg==0&&binding.count==1&&
           (binding.type==D3D12_DESCRIPTOR_RANGE_TYPE_UAV||binding.type==D3D12_DESCRIPTOR_RANGE_TYPE_SRV))continue;
        if(dma&&binding.type==D3D12_DESCRIPTOR_RANGE_TYPE_UAV&&binding.space==Shader::NativeDma::DescriptorSet&&
           binding.reg==0&&binding.count==Shader::NativeDma::BankCount)continue;
        if(binding.type==D3D12_DESCRIPTOR_RANGE_TYPE_CBV&&binding.reg==0&&binding.count==1&&
            (binding.space==30||binding.space==31||binding.space==Shader::NATIVE_BUFFER_BOUNDS_SET))continue;
        if(binding.space!=0||(binding.type!=D3D12_DESCRIPTOR_RANGE_TYPE_SRV&&binding.type!=D3D12_DESCRIPTOR_RANGE_TYPE_UAV&&binding.type!=D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER))
            throw std::runtime_error("Unexpected native guest compute descriptor class/space");
        for(UINT i=0;i<binding.count;++i)if(!covered.contains({binding.reg+i,binding.space}))
            throw std::runtime_error("Native guest compute DXIL resource has no guest metadata binding");
    }
    diagnostic.Detail("compute=%u buffers=%zu images=%zu samplers=%zu",unsigned(pipeline.IsCompute()),requests.size(),images.size(),samplers.size());
    if(!ordered_stages){
        std::map<std::tuple<VAddr,u64,bool>,size_t> stream_views;
        for(size_t index=0;index<ordinary_count;++index)if(streams_before_binding(index)){
            // Keep the guest requests unchanged for invalidation, write ranges
            // and ordering. Only this descriptor's native binding is private.
            const auto source=requests[index];
            const auto key=std::tuple{source.address,source.size,source.unordered_access};
            if(const auto existing=stream_views.find(key);existing!=stream_views.end()){
                pending[index].stream_view=existing->second;continue;
            }
            if(!rasterizer.ReadMemory(source.address,source.size))
                throw std::out_of_range("Ordered CPU stream has no readable guest backing");
            rasterizer.ValidateHostMemory(source.address,source.size);
            const auto [address,capacity]=temporary.Make(source.size,reinterpret_cast<const void*>(source.address));
            if(capacity<source.size)throw std::logic_error("Ordered CPU stream temporary is smaller than its sharp");
            const auto view_index=requests.size();
            // Reflected guest readonly buffers use UAV descriptors. Stage CPU
            // bytes through the normal default-resource path, never an UPLOAD UAV.
            requests.push_back({address,source.size,false,requests[index].unordered_access,true});
            requests.back().publish_dma_residency=false;
            stream_views.emplace(key,view_index);pending[index].stream_view=view_index;
        }
    }
    if(ordered_formatted_writer){
        // Complete legality validation precedes captures and logical invalidation.
        // Capture may rebuild native envelopes: no final shader views exist yet.
        for(const auto& request:requests){
            (void)request.WriteRange();
            rasterizer.ValidateGpuBacking(request.address,request.size,request.written);
        }
        for(auto& transfer:ordered_formatted_images){
            // Capture/ReserveMemory may Finish and consume a one-shot callback.
            // Cover earlier work first, then publish the new retile submission
            // even if later buffer preparation throws before descriptor creation.
            rasterizer.NotifyOnSubmission([textures](u64 fence){textures->Submitted(fence);});
            auto initial=rasterizer.CaptureShaderBuffer(transfer.address,transfer.bytes,false);
            try{
                transfer.tiled=textures->RetileBufferVersion(*transfer.image,initial,
                    rasterizer.RecordingList(),rasterizer.CompletedFence());
            }catch(...){
                // Retile publishes its leases before recording; a failing API
                // call can still leave those commands pending for later Flush.
                rasterizer.NotifyOnSubmission([textures](u64 fence){textures->Submitted(fence);});
                throw;
            }
            rasterizer.NotifyOnSubmission([textures](u64 fence){textures->Submitted(fence);});
        }
    }
    // Invalidation uses the original logical sharp address even when a cached
    // native envelope is already protected or starts before that sharp.
    for(size_t i=0;i<ordinary_count;++i)if(!ordered_stages&&requests[i].written){const auto write=requests[i].WriteRange();
        rasterizer.NotifyGpuBufferWrite(write.address,write.size);}
    const bool retained_image_buffer_writer=std::ranges::any_of(requests,[&](const auto& buffer){
        return buffer.written&&separate_overlap(buffer.address,buffer.size);
    });
    std::map<std::pair<VAddr,u64>,std::shared_ptr<const std::vector<std::byte>>> image_snapshots;
    std::map<std::pair<VAddr,u64>,std::shared_ptr<Resource>> gpu_image_snapshots;
    u64 gpu_snapshot_bytes{};
    std::vector<std::shared_ptr<void>> preserved_image_pins;
    // Grow/reinterpret an independent cached prefix before final buffer views
    // exist. Upload's private I/B transfer can extend native buffer envelopes;
    // doing that later would invalidate already selected shader descriptors.
    // Exact-base writers above have already retired I and therefore refresh
    // from current B through the ordinary prewrite snapshot path instead.
    if(!ordered_stages)for(const auto& group:shader_image_storage){
        if(group.cached_plan)continue;
        const bool needs_transfer=std::ranges::any_of(textures->Entries(),[&](const auto& cached){
            return cached->gpu_owned&&cached->buffer_alias_version&&!cached->logical_dirty&&
                CompatibleGuestImageMipGeometry(group.desc,group.layout,cached->desc,cached->layout)&&
                (cached->desc.levels<group.desc.levels||
                 !CompatibleGuestShaderStorage(cached->desc,group.desc));
        });
        if(!needs_transfer)continue;
        rasterizer.NotifyOnSubmission([textures](u64 fence){textures->Submitted(fence);});
        const auto bytes=std::span(reinterpret_cast<const std::byte*>(group.desc.address),size_t(group.layout.guest_bytes));
        auto& expanded=textures->Upload(group.desc,bytes,rasterizer.RecordingList(),rasterizer.CompletedFence(),
            {},false,nullptr,false,false,GuestTextureCache::UploadPurpose::Preparation);
        preserved_image_pins.push_back(textures->PinBinding(expanded));
        rasterizer.NotifyOnSubmission([textures](u64 fence){textures->Submitted(fence);});
    }
    for(auto& image:images)if(!ordered_stages||!stage_image_domain(image)){
        const auto& input=storage_desc(image);const auto& input_layout=storage_layout(image);
        if(!image.null_resource&&(!input.writable||image.buffer_writer_alias)&&
        std::ranges::any_of(requests,[&](const auto& buffer){return buffer.written&&
            buffer.address<input.address+input_layout.guest_bytes&&input.address<buffer.address+buffer.size;})){
        // An interior buffer writer does not invalidate the pinned same-base
        // independent image. Keep I here instead of capturing unrelated B0.
        // Capturing another image can reserve memory, so pin this witness until
        // Upload has selected and retained its descriptor resource.
        if(auto* retained=textures->FindIndependentUploadSource(input,input_layout)){
            preserved_image_pins.push_back(textures->PinBinding(*retained));continue;
        }
        image.current_buffer_snapshot=!image.desc.writable&&std::ranges::any_of(
            std::span(requests).first(ordinary_count),[&](const auto& buffer){
                const auto write=buffer.WriteRange();
                return buffer.written&&write.address==input.address&&write.size;
            });
        const auto key=std::pair{input.address,input_layout.guest_bytes};
        // An earlier formatted buffer bind may already have retiled I into B.
        // A later exact-base writer invalidates the logical I before images are
        // bound, but it must not erase that recorded prewrite transfer. Refresh
        // images from B1, not the older B0 captured before PrepareShaderBuffers.
        // The immutable tiled result already includes the original B padding.
        const auto retiled=std::ranges::find_if(ordered_formatted_images,[&](const auto& transfer){
            return transfer.address==key.first&&transfer.bytes==key.second&&transfer.tiled.handle;
        });
        if(retiled!=ordered_formatted_images.end()&&
           GuestTextureCache::CanUploadGpuSnapshot(input,input_layout,image.buffer_writer_alias)){
            image.ordered_transfer=size_t(retiled-ordered_formatted_images.begin());
            continue;
        }
        const auto prefix=std::ranges::find_if(ordered_formatted_images,[&](const auto& transfer){
            return transfer.address==key.first&&transfer.bytes<key.second&&transfer.tiled.handle&&
                CompatibleGuestShaderMipPrefixes(transfer.image->desc,transfer.image->layout,input,input_layout);
        });
        if(prefix!=ordered_formatted_images.end()&&
           GuestTextureCache::CanUploadGpuSnapshot(input,input_layout,image.buffer_writer_alias)){
            // BindBuffers retiled only the old image prefix. BindTextures then
            // grows I from that B1 prefix plus current B0 tail/padding. Capture
            // before final views: native arena growth must not replace them.
            auto owner=input;owner.writable=true;
            if(!textures->CanRetainIndependentImage(owner,LayoutGuestTexture(owner)))
                throw std::runtime_error("Ordered image mip growth needs independent range ownership");
            // Reserve the grown B before final native views, but publish its
            // semantic image-source residency only after all buffer binds.
            (void)rasterizer.CaptureShaderBuffer(key.first,key.second,false);
            image.ordered_transfer=size_t(prefix-ordered_formatted_images.begin());
            continue;
        }
        image.buffer_source_residency=rasterizer.IsGpuBufferModified(key.first,key.second);
        if(GuestTextureCache::CanUploadGpuSnapshot(input,input_layout,image.buffer_writer_alias)&&
           image.buffer_source_residency){
            auto existing=gpu_image_snapshots.find(key);
            const auto capacity=(key.second+D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT-1)&
                ~u64(D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT-1);
            if(existing!=gpu_image_snapshots.end()){
                image.gpu_snapshot=existing->second;continue;
            }
            if(capacity<=64*1024*1024-gpu_snapshot_bytes){
                diagnostic.Stage("buffer-image-gpu-prewrite-snapshot");
                image.gpu_snapshot=std::make_shared<Resource>(rasterizer.CaptureShaderBuffer(key.first,key.second,false));
                gpu_image_snapshots.emplace(key,image.gpu_snapshot);gpu_snapshot_bytes+=capacity;
                Diagnostics::Count("buffer-image.gpu-snapshot.bytes",key.second);
                continue;
            }
        }
        auto& snapshot=image_snapshots[key];
        if(!snapshot){
            diagnostic.Stage("buffer-image-prewrite-snapshot");
            if(!rasterizer.ReadMemory(key.first,key.second))
                throw std::out_of_range("Native sampled buffer/image alias has no readable guest backing");
            rasterizer.ValidateHostMemory(key.first,key.second);
            auto bytes=std::make_shared<std::vector<std::byte>>(size_t(key.second));
            memcpy(bytes->data(),reinterpret_cast<const void*>(key.first),bytes->size());
            Diagnostics::Count("buffer-image.snapshot.bytes",key.second);
            snapshot=std::move(bytes);
        }
        image.guest_snapshot=snapshot;
    }
    }
    // CPU fallback can have resolved only narrow dirty sharps while capturing
    // a larger image. Reserve its complete B source before selecting any views;
    // the actual image bind below still owns semantic publication.
    for(const auto& image:images)if(image.buffer_source_residency){
        const auto& desc=storage_desc(image);const auto& layout=storage_layout(image);
        if(std::ranges::any_of(requests,[&](const auto& request){return request.resident&&
            request.address<=desc.address&&desc.address-request.address<=request.size&&
            layout.guest_bytes<=request.size-(desc.address-request.address);}))continue;
        const bool uav=std::ranges::any_of(requests,[&](const auto& request){return request.unordered_access&&
            request.address<desc.address+layout.guest_bytes&&desc.address<request.address+request.size;});
        requests.push_back({desc.address,layout.guest_bytes,false,uav,true});
        requests.back().publish_dma_residency=false;
    }
    diagnostic.Stage("buffer-views");
    std::optional<OrderedGuestBindings> stage_bindings;
    if(ordered_stages){
        std::vector<OrderedGuestBuffer> ordered_buffers;ordered_buffers.reserve(ordinary_count);
        for(size_t i=0;i<ordinary_count;++i)ordered_buffers.push_back({pending[i].guest_read,
            pending[i].formatted_read,streams_before_binding(i),pending[i].byte_adjust,
            pending[i].access_alignment,pending[i].original_bytes});
        std::vector<OrderedGuestImage> ordered_images;ordered_images.reserve(images.size());
        for(const auto& image:images)ordered_images.push_back({storage_desc(image),storage_layout(image),
            image.desc.writable,image.null_resource,
            image.storage_group?shader_image_storage[*image.storage_group].cached_plan:nullptr});
        const bool exact_bounds=std::ranges::any_of(pipeline.Layout().Bindings(),[](const auto& binding){
            return binding.type==D3D12_DESCRIPTOR_RANGE_TYPE_CBV&&binding.space==Shader::NATIVE_BUFFER_BOUNDS_SET;});
        stage_bindings=PrepareOrderedGuestStages(device,rasterizer,*textures,binding_stages,requests,
            ordered_buffers,ordered_images,exact_bounds,[&](VAddr address,u64 bytes){
                if(!rasterizer.ReadMemory(address,bytes))throw std::out_of_range("Ordered stage CPU stream has no readable backing");
                rasterizer.ValidateHostMemory(address,bytes);
                const auto [copy,capacity]=temporary.Make(bytes,reinterpret_cast<const void*>(address));
                if(capacity<bytes)throw std::logic_error("Ordered stage CPU stream is shorter than its original sharp");
                return copy;
            });
        for(size_t i=0;i<ordinary_count;++i)pending[i].stream_view=stage_bindings->stream_views[i];
        for(size_t i=0;i<images.size();++i)images[i].ordered_entry=stage_bindings->images[i];
    }
    std::vector<bool> publish;
    if(!stage_bindings){
        for(const auto& request:requests)publish.push_back(request.publish_dma_residency);
        for(auto& request:requests)request.publish_dma_residency=false;
    }
    auto views=stage_bindings?std::move(stage_bindings->buffers):rasterizer.PrepareShaderBuffers(requests);
    if(!stage_bindings){
        rasterizer.DmaMemory().PublishPreparedFaults(rasterizer);
        for(size_t i=ordinary_count;i<publish.size();++i)if(publish[i]){
            auto resident=requests[i];resident.publish_dma_residency=true;
            rasterizer.DmaMemory().ObserveResidentSharps(std::span(&resident,1));
        }
        if(!formatted_images.empty())rasterizer.NotifyOnSubmission([textures](u64 fence){textures->Submitted(fence);});
        for(size_t i=0;i<ordinary_count;++i){
            const auto& request=requests[i];
            if(publish[i]&&!pending[i].stream_view){
                auto resident=request;resident.publish_dma_residency=true;
                rasterizer.DmaMemory().ObserveResidentSharps(std::span(&resident,1));
            }
            for(auto& transfer:ordered_formatted_images)if(std::ranges::find(transfer.readers,i)!=transfer.readers.end())
                rasterizer.CopyResidentImageToPreparedShaderBuffer(transfer.address,transfer.bytes,transfer.tiled,
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            const auto state=request.unordered_access?D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            for(const auto& [index,value]:formatted_metadata)if(index==i)
                rasterizer.FillPreparedShaderBuffer(request.address,request.size,value,state);
            for(const auto& [index,image]:formatted_images)if(index==i){
                auto initial=rasterizer.CapturePreparedShaderBuffer(request.address,image->layout.guest_bytes);
                auto tiled=textures->RetileBufferVersion(*image,initial,rasterizer.RecordingList(),rasterizer.CompletedFence());
                rasterizer.CopyResidentImageToPreparedShaderBuffer(request.address,image->layout.guest_bytes,tiled,state);
            }
        }
    }
    for(auto& image:images)if(image.ordered_transfer){
        const auto key=std::pair{storage_desc(image).address,storage_layout(image).guest_bytes};
        image.buffer_source_residency=true;
        auto [existing,inserted]=gpu_image_snapshots.try_emplace(key);
        if(inserted)existing->second=std::make_shared<Resource>(rasterizer.CapturePreparedShaderBuffer(key.first,key.second));
        image.gpu_snapshot=existing->second;
    }
    if(!ordered_formatted_images.empty()){
        // Capturing the final masked B for image upload leaves that arena in
        // COPY_SOURCE. Restore one combined state per selected native buffer.
        std::map<ID3D12Resource*,std::pair<size_t,bool>> states;
        for(size_t i=0;i<views.size();++i){
            auto [it,inserted]=states.try_emplace(views[i].resource.Get(),i,false);
            it->second.second|=requests[i].unordered_access;
        }
        for(const auto& [resource,state]:states){
            const auto& request=requests[state.first];
            rasterizer.TransitionPreparedShaderBuffer(request.address,request.size,state.second?
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }
    }
    diagnostic.Stage("descriptor-heap-allocation");
    auto bindings_lease=std::make_shared<ResourceBindings>(device,pipeline.Layout(),&rasterizer.BindingHeapPool());
    auto& bindings=*bindings_lease;
    if(stage_bindings)bindings.RetainAllocation(stage_bindings->pins);
    diagnostic.Stage("buffer-descriptors-and-constants");
    // Original arena bindings remain logical through BindTextures and
    // BeginRendering. Attachment uploads can grow a preserving B envelope or
    // Finish/reset constants. Pick native descriptors only at the final bind.
    bindings.BeforeBind([&rasterizer,push,ordinary_count,
        pending,dma,dma_pending,prepared_views=views,
        auxiliary=std::vector<Rasterizer::ShaderBufferRequest>(read_buffers.begin(),read_buffers.end()),
        runtime=std::vector<std::byte>(runtime_data.begin(),runtime_data.end()),
        buffer_requests=std::vector<Rasterizer::ShaderBufferRequest>(requests.begin(),requests.end())]
        (ResourceBindings& table) mutable {
        // This can restore protection/Finish. Complete it before selecting any
        // final views; actual original stage invalidation is not replayed here.
        rasterizer.MarkShaderBufferWrites(buffer_requests);
        std::vector<Rasterizer::ShaderBufferRequest> resident_auxiliary;
        for(auto request:auxiliary){const auto source=request.WriteRange();
            if(source.size<=16384&&!rasterizer.IsGpuBufferModified(source.address,source.size))continue;
            request.unordered_access=true;request.publish_dma_residency=false;
            resident_auxiliary.push_back(request);
        }
        if(!resident_auxiliary.empty()){
            rasterizer.PreparePhysicalDmaBuffers(resident_auxiliary);
            for(auto request:resident_auxiliary){request.publish_dma_residency=true;
                rasterizer.DmaMemory().ObserveResidentSharps(std::span(&request,1));}
        }
        if(dma)dma->MaterializeResidentBacking(rasterizer);
        // Physical growth can retire a preceding write reservation. Restore
        // the current command's exact writes before final native selection.
        rasterizer.MarkShaderBufferWrites(buffer_requests);
        const auto current=rasterizer.RefreshPreparedShaderBuffers(buffer_requests,prepared_views);
        for(const auto& view:current){
            table.RetainAllocation(view.allocation);table.RetainAllocation(view.lease);
        }
        if(dma){
            std::vector<Rasterizer::ShaderBufferView> shader_srvs;
            for(size_t i=0;i<ordinary_count;++i)if(!pending[i].uav)
                shader_srvs.push_back(current[pending[i].stream_view.value_or(i)]);
            // Only reflected shader SRVs conflict with raw UAV banks. IA is
            // selected after this phase and uses its existing GPU snapshot
            // when the current arena is a UAV. There are no earlier DMA views:
            // all real shader generations were validated above, and BindResident
            // selects/validates fully initialized DMA views without allocation.
            dma->BindResident(rasterizer,table,{}, {},shader_srvs);
            for(const auto& target:dma_pending)dma->BindMetadata(table,target.reg,target.fault);
        }
        std::array<u64,Shader::NATIVE_BUFFER_BOUNDS_VECTORS*2> buffer_limits{};
        for(size_t i=0;i<ordinary_count;++i){
            const auto& target=pending[i];const auto& view=current[target.stream_view.value_or(i)];
            const auto base=view.offset&~u64(15),delta=view.offset-base;
            if(view.size>Shader::NativeDma::MaxRawViewBytes-delta)
                throw std::out_of_range("Native raw buffer descriptor element count overflow");
            push.buf_offsets[target.push_index]=u8(delta+target.byte_adjust);
            if(target.byte_adjust>view.size)
                throw std::out_of_range("Native buffer prefix exceeds prepared view");
            const u64 exact_bytes=target.original_bytes?target.original_bytes:view.size-target.byte_adjust;
            if(!exact_bytes||exact_bytes>view.size-target.byte_adjust)
                throw std::out_of_range("Native buffer original sharp exceeds prepared view");
            buffer_limits.at(target.push_index)=delta+target.byte_adjust+exact_bytes-1;
            const auto elements=UINT((view.size+delta)/4);
            if(target.uav){
                D3D12_UNORDERED_ACCESS_VIEW_DESC d{};d.Format=DXGI_FORMAT_R32_TYPELESS;
                d.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;d.Buffer.FirstElement=base/4;
                d.Buffer.NumElements=elements;d.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
                for(const auto& b:table.Layout().Bindings())if(b.type==D3D12_DESCRIPTOR_RANGE_TYPE_UAV&&
                    b.space==0&&target.reg>=b.reg&&target.reg-b.reg<b.count)
                    table.Uav(b.reg,0,view.resource.Get(),d,target.reg-b.reg);
            }else{
                D3D12_SHADER_RESOURCE_VIEW_DESC d{};d.Format=DXGI_FORMAT_R32_TYPELESS;
                d.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;d.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                d.Buffer.FirstElement=base/4;d.Buffer.NumElements=elements;d.Buffer.Flags=D3D12_BUFFER_SRV_FLAG_RAW;
                for(const auto& b:table.Layout().Bindings())if(b.type==D3D12_DESCRIPTOR_RANGE_TYPE_SRV&&
                    b.space==0&&target.reg>=b.reg&&target.reg-b.reg<b.count)
                    table.Srv(b.reg,0,view.resource.Get(),d,target.reg-b.reg);
            }
        }
        auto push_buffer=rasterizer.UploadConstants(std::as_bytes(std::span(&push,1)));
        auto runtime_buffer=rasterizer.UploadConstants(runtime);
        std::optional<UploadArena::Slice> limits_buffer;
        for(const auto& b:table.Layout().Bindings())if(b.type==D3D12_DESCRIPTOR_RANGE_TYPE_CBV){
            if(b.space==Shader::NATIVE_BUFFER_BOUNDS_SET){
                if(!limits_buffer)limits_buffer=rasterizer.UploadConstants(std::as_bytes(std::span(buffer_limits)));
                table.Cbv(0,b.space,limits_buffer->resource,limits_buffer->offset,
                    UINT((sizeof(buffer_limits)+255)&~size_t(255)));
            }else table.Cbv(0,b.space,b.space==30?push_buffer.resource:runtime_buffer.resource,
                    b.space==30?push_buffer.offset:runtime_buffer.offset,
                    b.space==30?256:UINT((runtime.size()+255)&~size_t(255)));
        }
    });
    std::vector<GuestTextureCache::Entry*> uploaded,uploaded_writers;
    const auto staged_image=[&](const GuestTextureCache::Entry* entry){return stage_bindings&&
        std::ranges::find(stage_bindings->images,entry)!=stage_bindings->images.end();};
    const auto needs_readback=[&](const GuestTextureCache::Entry* entry){return !entry->gpu_owned&&!staged_image(entry);};
    std::vector<GuestTextureCache::Entry*> bc_readers;
    std::vector<GuestTextureCache::WritableBlockAlias> alias_writers;
    std::vector<std::pair<GuestTextureDesc,GuestTextureCache::Entry*>> uploaded_views;
    // Keep all recorded resources alive on exceptions as well as normal GPU
    // completion. Cache entries own texture uploads; readbacks own their buffers.
    std::vector<std::unique_ptr<GuestImageReadback>> readbacks;
    std::vector<std::pair<GuestTextureCache::Entry*,std::shared_ptr<GuestMultisample::StorageLease>>> storage_transfers;
    try {
        diagnostic.Stage("image-upload-and-descriptors");
        // Converted sampled views retain their pre-dispatch resources. Record
        // them first so the final authoritative cache backing is the resource
        // used by all compatible writable views during the guest operation.
        std::stable_partition(images.begin(),images.end(),[](const auto& image){return !image.desc.writable;});
        struct BlockViewGroup {
            GuestTextureCache::Entry* parent;uint64_t parent_uid;size_t storage;
            bool has_writer;
            std::optional<size_t> alias;
        };
        std::vector<BlockViewGroup> block_groups;
        std::vector<std::optional<size_t>> image_block_group(images.size());
        // Plan every group and pin every parent before any Upload can reserve
        // memory. Descriptor order must not choose a smaller allocation or
        // create separate copies whose commits overwrite one another.
        for(size_t index=0;index<images.size();++index){const auto& image=images[index];
            if(image.null_resource)continue;
            if(auto* parent=textures->FindBlockViewParent(image.desc,image.layout)){
                if(image.desc.writable)validate_writable_block_alias(image,*parent);
                const auto group=std::ranges::find_if(block_groups,[parent](const auto& item){return item.parent==parent;});
                if(group==block_groups.end()){
                    image_block_group[index]=block_groups.size();
                    block_groups.push_back({parent,parent->image_uid,index,image.desc.writable,{}});
                    writable_block_parent_pins.push_back(textures->PinBinding(*parent));
                }else{
                    image_block_group[index]=size_t(group-block_groups.begin());
                    if(!compatible_block_prefixes(images[group->storage].desc,images[group->storage].layout,
                        image.desc,image.layout))
                        throw std::runtime_error("Writable BC group has incompatible physical mip prefixes");
                    if(image.desc.levels>images[group->storage].desc.levels)group->storage=index;
                    group->has_writer|=image.desc.writable;
                }
            }
        }
        for(size_t image_index=0;image_index<images.size();++image_index) {
            const auto& image=images[image_index];
            // Native null views match the guest null T# without reading address
            // zero or allocating/uploading fake guest backing. Reads return 0;
            // writes are discarded by D3D12 (also used by Mesa/TranslationLayer).
            GuestTextureCache::Entry descriptor_entry;
            descriptor_entry.desc=image.desc;descriptor_entry.layout=image.layout;
            GuestTextureCache::Entry* entry=image.null_resource?&descriptor_entry:image.ordered_entry;
            if(image.ordered_entry)entry->pending=true;
            if(image.ordered_entry&&std::ranges::find(uploaded,entry)==uploaded.end()){
                bindings.RetainAllocation(textures->PinBinding(*entry));uploaded.push_back(entry);
            }
            bool block_alias_view=false;
            if(image_block_group[image_index]&&block_groups[*image_block_group[image_index]].has_writer){
                auto& group=block_groups[*image_block_group[image_index]];
                if(textures->FindBlockViewParent(image.desc,image.layout)!=group.parent||
                   group.parent->image_uid!=group.parent_uid)
                    throw std::runtime_error("Writable BC parent changed after group planning");
                if(!group.alias){
                    rasterizer.NotifyOnSubmission([textures](u64 fence){textures->Submitted(fence);});
                    auto storage=images[group.storage].desc;storage.writable=true;
                    auto alias=textures->BeginWritableBlockAlias(storage,
                        rasterizer.RecordingList(),rasterizer.CompletedFence());
                    if(!alias||!alias->image||alias->parent!=group.parent||alias->parent_uid!=group.parent_uid)
                        throw std::runtime_error("Writable BC block parent changed before transient image binding");
                    group.alias=alias_writers.size();
                    bindings.RetainAllocation(alias->image_pin);uploaded.push_back(alias->image);
                    alias_writers.push_back(std::move(*alias));
                }
                entry=alias_writers[*group.alias].image;block_alias_view=true;
                if(entry->gpu_owned||entry->buffer_alias_version||!entry->logical_dirty)
                    throw std::runtime_error("Writable BC block transient acquired independent authority before dispatch");
            }else if(!entry&&image.desc.writable&&textures->FindWritableBlockParent(image.desc,image.layout))
                throw std::runtime_error("Writable BC parent appeared after group planning");
            bool separate_input=image.buffer_writer_alias;
            for(const auto& candidate:images)if(candidate.buffer_writer_alias&&
                (CompatibleGuestShaderStorage(candidate.desc,image.desc)||
                 (candidate.storage_group&&candidate.storage_group==image.storage_group)))separate_input=true;
            if(!entry)for(const auto& [description,previous]:uploaded_views)
                if((description==image.desc||CompatibleGuestShaderStorage(description,image.desc)||
                    (description.levels>=image.desc.levels&&CompatibleGuestShaderMipPrefixes(description,
                        LayoutGuestTexture(description),image.desc,image.layout)))&&
                   (previous->desc==image.desc||CompatibleGuestShaderStorage(previous->desc,image.desc)||
                    (previous->desc.levels>=image.desc.levels&&CompatibleGuestShaderMipPrefixes(previous->desc,
                        previous->layout,image.desc,image.layout)))){entry=previous;break;}
            if(!entry) {
                // Allocate with UAV capability even if a sampled descriptor is
                // encountered first. All group members bind this same storage.
                auto storage=storage_desc(image);
                if(!image.storage_group)for(const auto& candidate:images)if(!candidate.null_resource&&candidate.desc.writable&&
                    CompatibleGuestShaderStorage(candidate.desc,image.desc)){storage=candidate.desc;break;}
                const auto guest=image.guest_snapshot?std::span<const std::byte>(*image.guest_snapshot):
                    std::span(reinterpret_cast<const std::byte*>(storage.address),size_t(storage_layout(image).guest_bytes));
                const auto plan=image.storage_group?shader_image_storage[*image.storage_group].cached_plan:nullptr;
                entry=plan?&textures->BindPlannedImage(*plan,rasterizer.RecordingList(),rasterizer.CompletedFence()):
                    &textures->Upload(storage,image.gpu_snapshot?std::span<const std::byte>{}:guest,
                    rasterizer.RecordingList(),rasterizer.CompletedFence(),{},
                    bool(image.guest_snapshot)||bool(image.gpu_snapshot),image.gpu_snapshot.get(),
                    separate_input&&(bool(image.guest_snapshot)||bool(image.gpu_snapshot)),
                    image.current_buffer_snapshot);
                if(std::ranges::find(uploaded,entry)==uploaded.end()){
                    bindings.RetainAllocation(textures->PinBinding(*entry));uploaded.push_back(entry);
                }
                uploaded_views.emplace_back(storage,entry);
            }
            if(!image.null_resource&&image.desc.writable&&!block_alias_view&&
               std::ranges::find(uploaded_writers,entry)==uploaded_writers.end())uploaded_writers.push_back(entry);
            if(!image.null_resource){
                if(image.buffer_source_residency){
                    const Rasterizer::ShaderBufferRequest resident{storage_desc(image).address,
                        storage_layout(image).guest_bytes,false,true,true};
                    rasterizer.DmaMemory().ObserveResidentSharps(std::span(&resident,1));
                }
                textures->PublishBufferResidency(*entry);
            }
            if(!image.null_resource&&separate_input&&!block_alias_view&&!image.ordered_entry)entry->buffer_alias_version=true;
            const bool direct_bc_view=!image.null_resource&&!image.desc.writable&&image.layout.block_extent==4&&
                textures->FindReadonlyBlockParent(image.desc,image.layout)==entry;
            if(!image.null_resource){
                if(direct_bc_view&&
                   std::ranges::find(bc_readers,entry)==bc_readers.end())bc_readers.push_back(entry);
                textures->SetBackingSamples(*entry,image.desc.samples,rasterizer.RecordingList());
                if(image.sample_storage&&image.desc.samples>1){
                    if(std::ranges::none_of(storage_transfers,[entry](const auto& transfer){return transfer.first==entry;}))
                        storage_transfers.emplace_back(entry,textures->PrepareMultisampleStorage(*entry));
                }else{
                const auto state=image.desc.writable?D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
                const auto last_mip=image.desc.writable?image.mip+image.count:image.desc.levels;
                for(UINT layer=image.desc.first_layer;layer<image.desc.first_layer+image.desc.view_layers;++layer)
                    for(UINT mip=image.mip;mip<last_mip;++mip)
                        entry->texture.Transition(rasterizer.RecordingList(),state,mip+layer*entry->desc.levels);
                }
            }
            for(UINT i=0;i<image.count;++i)for(const auto& b:pipeline.Layout().Bindings()) {
                const auto reg=image.reg+i;if(b.space!=0||reg<b.reg||reg-b.reg>=b.count)continue;
                if(image.desc.writable&&b.type==D3D12_DESCRIPTOR_RANGE_TYPE_UAV) {
                    const auto mip=image.sample_storage?0:image.mip+i;
                    auto writable_view=image.desc;
                    if(block_alias_view)writable_view.levels=entry->desc.levels;
                    auto view=entry->Uav(writable_view,mip);
                    if(image.array&&image.desc.layers==1){
                        if(image.desc.one_dimensional){view.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE1DARRAY;view.Texture1DArray={mip,0,1};}
                        else{view.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2DARRAY;view.Texture2DArray={mip,0,1,0};}
                    }
                    auto* storage=&entry->texture;
                    if(!image.null_resource&&image.sample_storage&&image.desc.samples>1)
                        storage=&entry->storage_samples.at(i);
                    bindings.RetainAllocation(storage->allocation);
                    bindings.Uav(b.reg,0,storage->handle.Get(),view,reg-b.reg);
                }else if(!image.desc.writable&&b.type==D3D12_DESCRIPTOR_RANGE_TYPE_SRV) {
                    // Group admission proves the backing prefix. Build the
                    // original SRV range/LOD clamp, rather than expanding its
                    // mip span to the larger shared child allocation.
                    auto view=block_alias_view||direct_bc_view?descriptor_entry.Srv():entry->Srv(image.desc);
                    if(image.array&&image.desc.layers==1){
                        if(image.desc.one_dimensional){const auto source=view.Texture1D;
                            view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE1DARRAY;view.Texture1DArray={source.MostDetailedMip,source.MipLevels,0,1,source.ResourceMinLODClamp};}
                        else if(image.desc.samples>1){view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;view.Texture2DMSArray={0,1};}
                        else{const auto source=view.Texture2D;
                            view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DARRAY;view.Texture2DArray={source.MostDetailedMip,source.MipLevels,0,1,source.PlaneSlice,source.ResourceMinLODClamp};}
                    }
                    bindings.RetainAllocation(entry->texture.allocation);
                    bindings.Srv(b.reg,0,entry->texture.handle.Get(),view,reg-b.reg);
                }
            }
        }
        for(const auto& [entry,transfer]:storage_transfers)
            transfer->Record(rasterizer.RecordingList(),entry->texture,entry->storage_samples,entry->storage_linear,false);
        diagnostic.Stage("sampler-descriptors");
        for(const auto& sampler:samplers)for(const auto& b:pipeline.Layout().Bindings())
            if(b.type==D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER&&b.space==0&&sampler.reg>=b.reg&&sampler.reg-b.reg<b.count)
                bindings.Sampler(b.reg,0,sampler.desc,sampler.reg-b.reg);
        diagnostic.Stage("draw-or-dispatch-recording");
        bool owned_images=false;
        if(retained_image_buffer_writer||std::ranges::any_of(uploaded,[](const auto* entry){return entry->buffer_alias_version;})){
            rasterizer.MarkShaderBufferWrites(requests);
            if(!rasterizer.BuffersMayDefer(requests))
                throw std::runtime_error("Independent image version lost its writable buffer ownership before execution");
        }
        // A reused allocation can still have a writable cache description even
        // when all original slots here are readonly. Revalidation is not a new
        // guest image write and must not enqueue a linear-image download.
        for(auto* entry:uploaded)if(!staged_image(entry)&&std::ranges::find(uploaded_writers,entry)==uploaded_writers.end())
            if(std::ranges::none_of(alias_writers,[entry](const auto& alias){return alias.image==entry;}))
                textures->RevalidateReadonly(*entry);
        for(auto* entry:uploaded_writers)owned_images|=staged_image(entry)?entry->gpu_owned:textures->Own(*entry);
        for(auto* entry:uploaded)if(!staged_image(entry)&&entry->buffer_alias_version&&!entry->gpu_owned)
            throw std::runtime_error("Independent buffer/image cache ownership was not acquired");
        // A later UINT view can privately retile this same BC parent after its
        // SRV was bound, leaving it in COPY_SOURCE. All such snapshots precede
        // execution; restore the parent reads after final image preparation.
        for(auto* entry:bc_readers)entry->texture.Transition(rasterizer.RecordingList(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        submit(bindings);
        for(auto& alias:alias_writers){
            // Dispatch writes only transient UINT storage. Merge it into a
            // parent-I-seeded replacement BC before recording later commands;
            // the child never acquires guest authority or CPU readback fallback.
            rasterizer.NotifyOnSubmission([textures](u64 fence){textures->Submitted(fence);});
            textures->CommitWritableBlockAlias(alias,rasterizer.RecordingList(),rasterizer.CompletedFence());
            owned_images=true;
        }
        for(const auto& [entry,transfer]:storage_transfers)
            transfer->Record(rasterizer.RecordingList(),entry->texture,entry->storage_samples,entry->storage_linear,true);
        if(dma)dma->RecordFaults(rasterizer);
        diagnostic.Stage("image-readback-recording");
        // The graphics callback can upload color/depth attachments even when
        // the shader has no sampled images. Retire those uploads as well.
        for(auto* entry:uploaded_writers)if(needs_readback(entry))
            readbacks.push_back(std::make_unique<GuestImageReadback>(device,*entry,rasterizer.RecordingList()));
        static const bool async_enabled=[] {
            const auto* value=std::getenv("SHADPS4_DX12_ASYNC");
            // Ownership and readback checks below make the deferred path safe.
            // Keep an explicit diagnostic opt-out, rather than requiring a
            // hidden marker file for normal queue overlap.
            if(value)return value[0]!='0';
            return true;
        }();
        if(async_enabled&&rasterizer.BuffersMayDefer(requests)&&readbacks.empty()) {
            Diagnostics::Scope deferred(Diagnostics::Kind::Deferred,"retain-resources");
            diagnostic.Stage("deferred-gpu-retirement");
            if(textures)rasterizer.NotifyOnSubmission([textures](u64 fence){textures->Submitted(fence);});
            rasterizer.RetainUntilFinish([bindings_lease,temporary_lease,textures,stream_views=views](u64 fence){
                if(textures&&fence)textures->Collect(fence);
                bindings_lease->RecycleRetiredHeaps();
                temporary_lease->Release();
            });
            // Publish the DMA feedback copy's fence without waiting for it.
            // The ring consumes only completed areas on subsequent commands.
            if(dma||owned_images)rasterizer.Flush();
            return;
        }
        // Draw/dispatch and all image readbacks share one submission. Keep the
        // descriptor heaps and constants alive until this single fence retires.
        diagnostic.Stage("submit-and-retire");
        const auto fence=rasterizer.Flush();
        if(textures)textures->Submitted(fence);
        for(auto& rb:readbacks)rb->Submitted(fence);
        rasterizer.Finish();rasterizer.ValidateDebugMessages();
        if(dma)dma->ConsumeFaults(rasterizer);
        bindings.RecycleRetiredHeaps();
        diagnostic.Stage("image-cpu-coherence");
        if(!readbacks.empty()) {
            size_t index=0;for(auto* entry:uploaded_writers)if(needs_readback(entry)) {
                rasterizer.ValidateHostMemory(entry->desc.address,entry->layout.guest_bytes,true);
                readbacks[index++]->Resolve(rasterizer.CompletedFence(),
                    std::span(reinterpret_cast<std::byte*>(entry->desc.address),size_t(entry->layout.guest_bytes)));
            }
        }
        diagnostic.Stage("texture-retirement");
        if(textures)textures->Collect(rasterizer.CompletedFence());
    }catch(...) {
        try{rasterizer.Finish();if(textures){textures->Submitted(rasterizer.Flush());textures->Collect(rasterizer.CompletedFence());}}catch(...){}
        throw;
    }
    diagnostic.Stage("temporary-buffer-retirement");
    temporary.Release();
}
void ExecuteGuestBufferCompute(ID3D12Device* device,Rasterizer& rasterizer,
    const ShaderPipeline& pipeline,const Shader::Info& info,
    Shader::Backend::Bindings start,const ComputeInvocation& invocation,GuestTextureCache* textures,VAddr border_colors) {
    if(!pipeline.IsCompute()||info.sw_stage!=Shader::SwStage::Compute)throw std::invalid_argument("Native compute command stage mismatch");
    for(auto dim:invocation.groups)if(!dim)throw std::invalid_argument("Native compute group count");
    if(std::any_of(std::begin(invocation.groups),std::end(invocation.groups),[](u32 dim){return dim>65535;})){
        // D3D12 caps each Dispatch dimension at 65535. Preserve guest
        // NumWorkgroups while adding each chunk's offset to BaseWorkgroup.
        for(unsigned i=0;i<3;++i){
            if(u64(invocation.base[i])+invocation.groups[i]-1>UINT32_MAX)
                throw std::out_of_range("Native split dispatch base overflow");
        }
        for(u64 z=0;z<invocation.groups[2];z+=65535)
        for(u64 y=0;y<invocation.groups[1];y+=65535)
        for(u64 x=0;x<invocation.groups[0];x+=65535){
            auto part=invocation;const u64 offset[]{x,y,z};
            for(unsigned i=0;i<3;++i){
                part.groups[i]=u32(std::min<u64>(65535,u64(invocation.groups[i])-offset[i]));
                part.base[i]=invocation.base[i]+u32(offset[i]);
                part.logical_groups[i]=invocation.logical_groups[i]?invocation.logical_groups[i]:invocation.groups[i];
            }
            ExecuteGuestBufferCompute(device,rasterizer,pipeline,info,start,part,textures,border_colors);
        }
        return;
    }
    struct Runtime {u32 groups[3];u32 padding;u32 base[3];} runtime{};
    for(unsigned i=0;i<3;++i)runtime.groups[i]=invocation.logical_groups[i]?invocation.logical_groups[i]:invocation.groups[i];
    memcpy(runtime.base,invocation.base,sizeof(runtime.base));
    Shader::PushData push{};push.xscale=push.yscale=1;const GuestResourceStage stages[]{{&info,start}};
    ExecuteGuestResources(device,rasterizer,pipeline,stages,invocation,push,
        std::as_bytes(std::span(&runtime,1)),[&](ResourceBindings& b){rasterizer.ExecuteCompute(pipeline,b,
            invocation.groups[0],invocation.groups[1],invocation.groups[2],false);},textures,border_colors);
}
void ExecuteGuestGraphicsResources(ID3D12Device* device,Rasterizer& rasterizer,
    const ShaderPipeline& pipeline,std::span<const GuestResourceStage> stages,
    const Shader::PushData& push,const GraphicsRuntime& runtime,
    const std::function<void(ResourceBindings&)>& submit,GuestTextureCache& textures,VAddr border_colors,
    std::span<const Rasterizer::ShaderBufferRequest> read_buffers) {
    if(pipeline.IsCompute())throw std::invalid_argument("Native graphics command has compute PSO");
    ExecuteGuestResources(device,rasterizer,pipeline,stages,{},push,std::as_bytes(std::span(&runtime,1)),submit,&textures,border_colors,read_buffers);
}
}
