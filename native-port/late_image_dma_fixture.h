// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_stage_bindings.h"
#include "video_core/renderer_d3d12/guest_dma.h"
#include "video_core/renderer_d3d12/compute_pipeline.h"
#include "common/scope_exit.h"
#include <cstring>

namespace NativeDX12 {
// Uses the fixture's production ReadConst pipeline. No test-built BDA table.
inline void RunLateImageDmaFixture(ID3D12Device* device,const ShaderPipeline& pipeline,
    VAddr guest_base,VAddr private_base,Resource& flat,Resource& output,u32 fallback){
    namespace Dma=Shader::NativeDma;
    const auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    for(const bool partial:{false,true}){
        const auto page=guest_base+2*Dma::PageSize;
        const auto address=page+2*Dma::SubpageSize;
        Rasterizer rasterizer(device);
        rasterizer.MapMemory(partial?address:page,partial?Dma::SubpageSize:Dma::PageSize);
        rasterizer.MapMemory(private_base,Dma::SubpageSize);
        GuestTextureCache textures(device);
        SCOPE_EXIT {rasterizer.Finish();};
        // This control isolates contents/table/view stability. Real PageWatch
        // ownership is covered by native_gpu_page_ownership independently.
        textures.SetIndependentImageRetention([](const auto&,const auto&){return true;});
        textures.SetBufferAliasRetention([](const auto&,const auto&){return true;});
        textures.SetOwnershipAcquirer([](auto& image){image.SetGpuOwned(true);return true;});
        textures.SetIndependentVersionTransfer({},
            [](auto& image){image.SetGpuOwned(true);return true;},{});
        const u32 old_cpu=*reinterpret_cast<const u32*>(address);
        Rasterizer::ShaderBufferRequest tiny{address,4,false,true,true};tiny.publish_dma_residency=false;
        rasterizer.PrepareShaderBuffers(std::span(&tiny,1));
        rasterizer.FillPreparedShaderBuffer(address,4,0x192a3b4c,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        require(!rasterizer.PreparedDmaBuffer(address,Dma::SubpageSize),"Tiny B unexpectedly covers a whole subpage");
        require(rasterizer.DmaMemory().ResidentIntersections(page,Dma::PageSize).empty(),
            "Private tiny B published a semantic page");
        GuestTextureDesc desc;desc.address=address;desc.width=desc.height=desc.pitch=4;
        desc.data_format=4;desc.number_format=7;desc.tile_mode=31;desc.writable=true;
        const auto layout=LayoutGuestTexture(desc);
        std::vector<Rasterizer::ShaderBufferRequest> requests{tiny};
        const OrderedGuestBuffer buffers[]{{true,false,true,0,4,4}};
        const OrderedGuestImage images[]{{desc,layout,true,false}};
        const GuestStageBindingSpan stages[]{{0,1,0,1}};
        auto prepared=PrepareOrderedGuestStages(device,rasterizer,textures,stages,requests,buffers,images,true,
            [&](VAddr source,u64 bytes){
                require(bytes==4,"Private CPU stream lost its original size");
                memcpy(reinterpret_cast<void*>(private_base),reinterpret_cast<const void*>(source),size_t(bytes));
                return private_base;
            });
        require(prepared.images[0]&&prepared.stream_views[0],"Late image DMA lost its original bound resources");
        rasterizer.DmaMemory().MaterializeResidentBacking(rasterizer);
        prepared.buffers=rasterizer.RefreshPreparedShaderBuffers(requests,prepared.buffers);
        const auto full=rasterizer.PreparedDmaBuffer(page,Dma::PageSize);
        require(bool(full)==!partial&&bool(rasterizer.PreparedDmaBuffer(address,Dma::SubpageSize)),
            "Late image source lacks complete legal pre-view backing");
        require(!partial||!rasterizer.PreparedDmaBuffer(address+Dma::SubpageSize,Dma::SubpageSize),
            "Late image source exposed an inaccessible neighboring subpage");
        require(*reinterpret_cast<const u32*>(address)==old_cpu&&
            *reinterpret_cast<const u32*>(private_base)==old_cpu,"Physical/private CPU B0 changed during image prediction");
        require(rasterizer.DmaMemory().ResidentIntersections(private_base,4).empty(),
            "Private CPU stream became a semantic DMA arena");
        ResourceBindings bindings(device,pipeline.Layout());
        const auto ordinary=[&](UINT reg,Resource& resource){
            resource.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            D3D12_UNORDERED_ACCESS_VIEW_DESC view{};view.Format=DXGI_FORMAT_R32_TYPELESS;
            view.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;view.Buffer.NumElements=UINT(resource.handle->GetDesc().Width/4);
            view.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;bindings.Uav(reg,0,resource.handle.Get(),view);
        };
        ordinary(0,flat);ordinary(3,output);
        bindings.BeforeBind([&](ResourceBindings& table){
            rasterizer.DmaMemory().BindResident(rasterizer,table,{},{});
            for(size_t i=0;i<requests.size();++i){const auto current=rasterizer.PreparedDmaBuffer(requests[i].address,requests[i].size);
                require(current&&current->resource.Get()==prepared.buffers[i].resource.Get()&&
                    current->offset==prepared.buffers[i].offset,"Late image DMA replaced a selected buffer view");}
            rasterizer.DmaMemory().BindMetadata(table,1,false);rasterizer.DmaMemory().BindMetadata(table,2,true);
        });
        rasterizer.ExecuteCompute(pipeline,bindings,1,1,1);
        auto snapshot=rasterizer.CapturePreparedShaderBuffer(address,Dma::SubpageSize);
        auto rb=Buffer(device,1024+Dma::SubpageSize,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        output.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        snapshot.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        rasterizer.RecordingList()->CopyBufferRegion(rb.handle.Get(),0,output.handle.Get(),0,1024);
        rasterizer.RecordingList()->CopyBufferRegion(rb.handle.Get(),1024,snapshot.handle.Get(),0,Dma::SubpageSize);
        rasterizer.Finish();textures.Submitted(rasterizer.CompletedFence());
        void* mapped{};D3D12_RANGE all{0,1024+Dma::SubpageSize};Check(rb.handle->Map(0,&all,&mapped));
        const auto* words=static_cast<const u32*>(mapped);
        bool correct=true;for(u32 lane=0;lane<64;++lane){
            const auto value=!partial&&lane==2?0xface0002U:0U;
            correct&=words[lane]==value&&words[lane+64]==(value?value:fallback)&&
                words[lane+128]==0&&words[lane+192]==0;
        }
        correct&=words[256]==0x192a3b4c;
        correct&=!memcmp(words+257,reinterpret_cast<const void*>(address+4),Dma::SubpageSize-4);
        D3D12_RANGE empty{};rb.handle->Unmap(0,&empty);
        require(correct,"Late image DMA full/partial contents, old GPU B1 or physical tail mismatch");
        rasterizer.ValidateDebugMessages();
    }
}
}
