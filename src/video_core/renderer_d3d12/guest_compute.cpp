// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_compute.h"
#include "guest_sampler.h"
#include "guest_image_readback.h"
#include "diagnostics.h"
#include "guest_dma.h"
#include <algorithm>
#include <cstring>
#include <set>
#include <tuple>
namespace NativeDX12 {
namespace {
constexpr u64 MaxBytes=256*1024*1024;
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
    const std::function<void(ResourceBindings&)>& submit,GuestTextureCache* textures,VAddr border_colors) {
    Diagnostics::Scope diagnostic(Diagnostics::Kind::Resources,"metadata-and-layout");
    std::lock_guard lock(rasterizer.ExecutionMutex());
    if(stages.empty()||!submit)throw std::invalid_argument("Empty native guest stage set/command");
    struct Pending {UINT reg;bool uav;u32 push_index,byte_adjust;};
    std::vector<Pending> pending;
    struct DmaPending {UINT reg;bool fault;};std::vector<DmaPending> dma_pending;
    const bool uses_dma=std::any_of(stages.begin(),stages.end(),[](const auto& stage){return stage.info&&stage.info->uses_dma;});
    auto* dma=uses_dma?&rasterizer.DmaMemory():nullptr;
    std::vector<Rasterizer::ShaderBufferRequest> requests;
    struct ImagePending {GuestTextureDesc desc;GuestTextureLayout layout;UINT reg,count,mip;bool array;bool null_resource;};
    std::vector<ImagePending> images;
    struct SamplerPending {UINT reg;D3D12_SAMPLER_DESC desc;};std::vector<SamplerPending> samplers;
    auto temporary_lease=std::make_shared<TemporaryMemory>(rasterizer);
    auto& temporary=*temporary_lease;
    std::set<std::pair<UINT,UINT>> covered;
    for(const auto& stage:stages) {
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
        if(!reflected)continue; // DXIL optimization removed this guest descriptor.
        const auto& source=info.buffers[index];const bool uav=reflected->type==D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        if(source.is_written&&!uav)throw std::runtime_error("Native writable guest buffer reflected as SRV");
        VAddr address{};u64 size{};u32 adjust{};
        switch(source.buffer_type) {
        case Shader::BufferType::Guest: {
            auto sharp=source.GetSharp(info);
            size=sharp.GetSize();
            if(!sharp.base_address||!size){std::tie(address,size)=temporary.Make(16);break;}
            if(size>MaxBytes)throw std::out_of_range("Native guest shader buffer exceeds slice budget: address="+
                std::to_string(sharp.base_address)+" bytes="+std::to_string(size)+
                " stride="+std::to_string(sharp.stride)+" records="+std::to_string(sharp.num_records)+
                " shader="+std::to_string(info.pgm_hash)+" binding="+std::to_string(index));
            adjust=u32(sharp.base_address&3);address=sharp.base_address&~u64(3);
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
        requests.push_back({address,size,source.is_written,uav});
        pending.push_back({reg,uav,start.buffer+u32(index),adjust});covered.emplace(reg,0);
    }
    u64 image_reg=u64(start.unified)+info.buffers.size();
    for(const auto& source:info.images) {
        const auto count=source.NumBindings(info);
        if(!count||image_reg>UINT_MAX-count)throw std::out_of_range("Native image descriptor register span");
        const auto type=source.is_written?D3D12_DESCRIPTOR_RANGE_TYPE_UAV:D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        bool used=false;
        for(const auto& b:pipeline.Layout().Bindings())if(b.type==type&&b.space==0)
            for(UINT i=0;i<count;++i)if(image_reg+i>=b.reg&&image_reg+i-b.reg<b.count){covered.emplace(UINT(image_reg+i),0);used=true;}
        if(used) {
            const auto sharp=source.GetSharp(info);
            const bool array=source.is_array||sharp.GetType()==AmdGpu::ImageType::Cube;
            auto desc=DescribeGuestTexture(sharp);desc.writable=source.is_written;
            // Original GCN IR lowers PS4 1D image operations to 2D with Y=0.
            // The native view must match that emitted DXIL resource dimension.
            if(desc.one_dimensional)desc.one_dimensional=false;
            if(desc.layers>1&&!array){
                if(!source.NeedsNativeSliceView(info))throw std::runtime_error("Unsupported native non-array layer view");
                desc.view_layers=1;
            }
            auto layout=LayoutGuestTexture(desc);
            if(desc.writable)textures->ValidateWritableFormat(layout.format);
            if(sharp&&!textures->HasOwnedShaderStorage(desc))
                rasterizer.ValidateHostMemory(desc.address,layout.guest_bytes,source.is_written);
            UINT mip=desc.first_mip;
            if(source.mip_fallback_mode==Shader::MipStorageFallbackMode::ConstantIndex)mip+=source.constant_mip_index;
            if(mip>=desc.levels||count>desc.levels-mip)throw std::out_of_range("Native image mip descriptor span");
            for(const auto& b:requests)if(sharp&&b.address<desc.address+layout.guest_bytes&&desc.address<b.address+b.size&&
                (b.written||desc.writable))
                throw std::runtime_error("Native simultaneous writable buffer/image alias needs shared cache coherency: buffer="+
                    std::to_string(b.address)+" bytes="+std::to_string(b.size)+" written="+std::to_string(b.written)+
                    " image="+std::to_string(desc.address)+" bytes="+std::to_string(layout.guest_bytes)+" writable="+std::to_string(desc.writable));
            for(const auto& previous:images)if(sharp&&!previous.null_resource&&previous.desc.address<desc.address+layout.guest_bytes&&desc.address<previous.desc.address+previous.layout.guest_bytes){
                if(previous.desc!=desc&&!CompatibleGuestShaderStorage(previous.desc,desc))
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
            images.push_back({desc,std::move(layout),UINT(image_reg),count,mip,array,!bool(sharp)});
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
    }
    const auto ordinary_count=requests.size();
    if(dma)dma->AppendRequests(rasterizer,requests);
    // Separate native buffer/texture views are valid for read-only aliases: both
    // are materialized from the same retired CPU-visible guest contents. Writes
    // require shared ownership/feedback semantics and remain explicit failures.
    for(const auto& image:images)for(const auto& b:requests)
        if(!image.null_resource&&b.address<image.desc.address+image.layout.guest_bytes&&image.desc.address<b.address+b.size&&
            (b.written||image.desc.writable))
            throw std::runtime_error("Native cross-stage writable buffer/image alias needs shared cache coherency: buffer="+
                std::to_string(b.address)+" bytes="+std::to_string(b.size)+" written="+std::to_string(b.written)+
                " image="+std::to_string(image.desc.address)+" bytes="+std::to_string(image.layout.guest_bytes)+" writable="+std::to_string(image.desc.writable));
    for(const auto& binding:pipeline.Layout().Bindings()) {
        if(dma&&binding.type==D3D12_DESCRIPTOR_RANGE_TYPE_UAV&&binding.space==Shader::NativeDma::DescriptorSet&&
           binding.reg==0&&binding.count==Shader::NativeDma::BankCount)continue;
        if(binding.type==D3D12_DESCRIPTOR_RANGE_TYPE_CBV&&binding.reg==0&&binding.count==1&&
           (binding.space==30||binding.space==31))continue;
        if(binding.space!=0||(binding.type!=D3D12_DESCRIPTOR_RANGE_TYPE_SRV&&binding.type!=D3D12_DESCRIPTOR_RANGE_TYPE_UAV&&binding.type!=D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER))
            throw std::runtime_error("Unexpected native guest compute descriptor class/space");
        for(UINT i=0;i<binding.count;++i)if(!covered.contains({binding.reg+i,binding.space}))
            throw std::runtime_error("Native guest compute DXIL resource has no guest metadata binding");
    }
    diagnostic.Detail("compute=%u buffers=%zu images=%zu samplers=%zu",unsigned(pipeline.IsCompute()),requests.size(),images.size(),samplers.size());
    diagnostic.Stage("buffer-views");
    auto views=rasterizer.PrepareShaderBuffers(requests);
    diagnostic.Stage("descriptor-heap-allocation");
    auto bindings_lease=std::make_shared<ResourceBindings>(device,pipeline.Layout(),&rasterizer.BindingHeapPool());
    auto& bindings=*bindings_lease;
    diagnostic.Stage("buffer-descriptors-and-constants");
    for(size_t i=0;i<ordinary_count;++i) {
        const auto& view=views[i];const auto& target=pending[i];
        // Raw SRV/UAV base is aligned to 16 bytes; PS4 sub-dword offsets go
        // through the original compiler's PushData adjustment, never truncation.
        const auto base=view.offset&~u64(15);const auto delta=view.offset-base;
        push.buf_offsets[target.push_index]=u8(delta+target.byte_adjust);
        const auto elements=UINT((view.size+delta)/4);
        if(target.uav) {
            D3D12_UNORDERED_ACCESS_VIEW_DESC d{};d.Format=DXGI_FORMAT_R32_TYPELESS;
            d.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;d.Buffer.FirstElement=base/4;
            d.Buffer.NumElements=elements;d.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
            // Arrays share the reflected range's base register and element index.
            for(const auto& b:pipeline.Layout().Bindings())if(b.type==D3D12_DESCRIPTOR_RANGE_TYPE_UAV&&b.space==0&&target.reg>=b.reg&&target.reg-b.reg<b.count)
                bindings.Uav(b.reg,0,view.resource.Get(),d,target.reg-b.reg);
        }else {
            D3D12_SHADER_RESOURCE_VIEW_DESC d{};d.Format=DXGI_FORMAT_R32_TYPELESS;
            d.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;d.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            d.Buffer.FirstElement=base/4;d.Buffer.NumElements=elements;d.Buffer.Flags=D3D12_BUFFER_SRV_FLAG_RAW;
            for(const auto& b:pipeline.Layout().Bindings())if(b.type==D3D12_DESCRIPTOR_RANGE_TYPE_SRV&&b.space==0&&target.reg>=b.reg&&target.reg-b.reg<b.count)
                bindings.Srv(b.reg,0,view.resource.Get(),d,target.reg-b.reg);
        }
    }
    if(dma){dma->Bind(rasterizer,bindings,std::span(views).subspan(ordinary_count));
        for(const auto& target:dma_pending)dma->BindMetadata(bindings,target.reg,target.fault);}
    // Image/attachment uploads below may Finish and reset the constant arena.
    // Allocate only once their ownership resolution has ended, immediately
    // before the command binds its descriptors and executes on the GPU.
    bindings.BeforeBind([&rasterizer,push,runtime=std::vector<std::byte>(runtime_data.begin(),runtime_data.end()),
        buffer_requests=std::vector<Rasterizer::ShaderBufferRequest>(requests.begin(),requests.end())](ResourceBindings& table){
        // Attachment uploads may Finish after buffer preparation. Mark actual
        // shader writes at bind time so such retirement cannot clear this draw.
        rasterizer.MarkShaderBufferWrites(buffer_requests);
        auto push_buffer=rasterizer.UploadConstants(std::as_bytes(std::span(&push,1)));
        auto runtime_buffer=rasterizer.UploadConstants(runtime);
        for(const auto& b:table.Layout().Bindings())if(b.type==D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
            table.Cbv(0,b.space,b.space==30?push_buffer.resource:runtime_buffer.resource,
                b.space==30?push_buffer.offset:runtime_buffer.offset,256);
    });
    std::vector<GuestTextureCache::Entry*> uploaded;
    std::vector<std::pair<GuestTextureDesc,GuestTextureCache::Entry*>> uploaded_views;
    // Keep all recorded resources alive on exceptions as well as normal GPU
    // completion. Cache entries own texture uploads; readbacks own their buffers.
    std::vector<std::unique_ptr<GuestImageReadback>> readbacks;
    try {
        diagnostic.Stage("image-upload-and-descriptors");
        for(const auto& image:images) {
            // Native null views match the guest null T# without reading address
            // zero or allocating/uploading fake guest backing. Reads return 0;
            // writes are discarded by D3D12 (also used by Mesa/TranslationLayer).
            GuestTextureCache::Entry null_entry;
            null_entry.desc=image.desc;null_entry.layout=image.layout;
            GuestTextureCache::Entry* entry=image.null_resource?&null_entry:nullptr;
            if(!entry)for(const auto& [description,previous]:uploaded_views)
                if(description==image.desc||CompatibleGuestShaderStorage(description,image.desc)){entry=previous;break;}
            if(!entry) {
                // Allocate with UAV capability even if a sampled descriptor is
                // encountered first. All group members bind this same storage.
                auto storage=image.desc;
                for(const auto& candidate:images)if(!candidate.null_resource&&candidate.desc.writable&&
                    CompatibleGuestShaderStorage(candidate.desc,image.desc)){storage=candidate.desc;break;}
                const auto guest=std::span(reinterpret_cast<const std::byte*>(image.desc.address),size_t(image.layout.guest_bytes));
                entry=&textures->Upload(storage,guest,rasterizer.RecordingList(),rasterizer.CompletedFence());uploaded.push_back(entry);
                uploaded_views.emplace_back(storage,entry);
            }
            if(!image.null_resource){
                const auto state=image.desc.writable?D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
                const auto last_mip=image.desc.writable?image.mip+image.count:image.desc.levels;
                for(UINT layer=image.desc.first_layer;layer<image.desc.first_layer+image.desc.view_layers;++layer)
                    for(UINT mip=image.mip;mip<last_mip;++mip)
                        entry->texture.Transition(rasterizer.RecordingList(),state,mip+layer*image.desc.levels);
            }
            for(UINT i=0;i<image.count;++i)for(const auto& b:pipeline.Layout().Bindings()) {
                const auto reg=image.reg+i;if(b.space!=0||reg<b.reg||reg-b.reg>=b.count)continue;
                if(image.desc.writable&&b.type==D3D12_DESCRIPTOR_RANGE_TYPE_UAV) {
                    auto view=entry->Uav(image.desc,image.mip+i);
                    if(image.array&&image.desc.layers==1){
                        if(image.desc.one_dimensional){view.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE1DARRAY;view.Texture1DArray={image.mip+i,0,1};}
                        else{view.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2DARRAY;view.Texture2DArray={image.mip+i,0,1,0};}
                    }
                    bindings.Uav(b.reg,0,entry->texture.handle.Get(),view,reg-b.reg);
                }else if(!image.desc.writable&&b.type==D3D12_DESCRIPTOR_RANGE_TYPE_SRV) {
                    auto view=entry->Srv(image.desc);
                    if(image.array&&image.desc.layers==1){
                        if(image.desc.one_dimensional){view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE1DARRAY;view.Texture1DArray={image.desc.first_mip,image.desc.levels-image.desc.first_mip,0,1,0};}
                        else{view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DARRAY;view.Texture2DArray={image.desc.first_mip,image.desc.levels-image.desc.first_mip,0,1,0,0};}
                    }
                    bindings.Srv(b.reg,0,entry->texture.handle.Get(),view,reg-b.reg);
                }
            }
        }
        diagnostic.Stage("sampler-descriptors");
        for(const auto& sampler:samplers)for(const auto& b:pipeline.Layout().Bindings())
            if(b.type==D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER&&b.space==0&&sampler.reg>=b.reg&&sampler.reg-b.reg<b.count)
                bindings.Sampler(b.reg,0,sampler.desc,sampler.reg-b.reg);
        diagnostic.Stage("draw-or-dispatch-recording");
        bool owned_images=false;
        for(auto* entry:uploaded)if(entry->desc.writable)owned_images|=textures->Own(*entry);
        submit(bindings);
        if(dma)dma->RecordFaults(rasterizer);
        diagnostic.Stage("image-readback-recording");
        // The graphics callback can upload color/depth attachments even when
        // the shader has no sampled images. Retire those uploads as well.
        for(auto* entry:uploaded)if(entry->desc.writable&&!entry->gpu_owned)
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
            size_t index=0;for(auto* entry:uploaded)if(entry->desc.writable&&!entry->gpu_owned) {
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
        u64 chunks=1;
        for(unsigned i=0;i<3;++i){
            if(u64(invocation.base[i])+invocation.groups[i]-1>UINT32_MAX)
                throw std::out_of_range("Native split dispatch base overflow");
            chunks*=((u64(invocation.groups[i])+65534)/65535);
            if(chunks>4096)throw std::out_of_range("Native split dispatch submission budget");
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
    const std::function<void(ResourceBindings&)>& submit,GuestTextureCache& textures,VAddr border_colors) {
    if(pipeline.IsCompute())throw std::invalid_argument("Native graphics command has compute PSO");
    ExecuteGuestResources(device,rasterizer,pipeline,stages,{},push,std::as_bytes(std::span(&runtime,1)),submit,&textures,border_colors);
}
}
