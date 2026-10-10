// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_dma.h"
#include "compute_pipeline.h"
#include "shader_pipeline.h"
#include <cstring>
#include <unordered_map>
namespace NativeDX12 {
namespace {
namespace Dma=Shader::NativeDma;
constexpr u64 TableBytes=u64(Dma::TableEntries)*sizeof(u64);
constexpr u64 FaultBytes=u64(Dma::PageCount)/8;
D3D12_UNORDERED_ACCESS_VIEW_DESC RawView(u64 offset,u64 bytes){
    if(offset%16||bytes%4||!bytes||bytes/4>UINT_MAX)throw std::invalid_argument("Native DMA raw view extent");
    D3D12_UNORDERED_ACCESS_VIEW_DESC view{};view.Format=DXGI_FORMAT_R32_TYPELESS;
    view.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;view.Buffer.FirstElement=offset/4;
    view.Buffer.NumElements=UINT(bytes/4);view.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;return view;
}
Resource Upload(ID3D12Device* device,u64 bytes,const void* initial=nullptr){
    auto resource=Buffer(device,bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
    void* data{};D3D12_RANGE empty{};Check(DX12_API_CALL(resource.handle->Map(0,&empty,&data)));
    if(initial)memcpy(data,initial,size_t(bytes));else memset(data,0,size_t(bytes));
    DX12_API_CALL(resource.handle->Unmap(0,nullptr));return resource;
}
}
void GuestDmaMemory::Initialize(Rasterizer& rasterizer){
    if(table_.handle)return;
    auto table=UavBuffer(device_.Get(),TableBytes),faults=UavBuffer(device_.Get(),FaultBytes);
    auto zero_faults=Upload(device_.Get(),FaultBytes);
    auto exported=UavBuffer(device_.Get(),DmaFaultCompactor::Bytes);
    auto compactor=std::make_unique<DmaFaultCompactor>(device_.Get());
    auto zero=std::make_shared<Resource>(Upload(device_.Get(),TableBytes));
    // Retain before recording, including an exceptional exit from preparation.
    rasterizer.RetainUntilFinish([zero](u64){});
    table_=std::move(table);faults_=std::move(faults);zero_faults_=std::move(zero_faults);
    exported_=std::move(exported);compactor_=std::move(compactor);
    auto* list=rasterizer.RecordingList();table_.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
    DX12_API_CALL(list->CopyBufferRegion(table_.handle.Get(),0,zero->handle.Get(),0,TableBytes));
    table_.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    faults_.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
    DX12_API_CALL(list->CopyBufferRegion(faults_.handle.Get(),0,zero_faults_.handle.Get(),0,FaultBytes));
    faults_.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}
void GuestDmaMemory::AppendRequests(Rasterizer& rasterizer,std::vector<Rasterizer::ShaderBufferRequest>& requests){
    ConsumeFaults(rasterizer);ranges_.clear();translations_.clear();indirect_entries_.clear();
    const auto append=[&](u32 entry,VAddr address,u32 bytes){
        if(ranges_.empty()||ranges_.back().address+ranges_.back().size!=address||ranges_.back().size>Dma::BankSize-bytes)
            ranges_.push_back({address,0,false,true});
        translations_.push_back({entry,bytes,ranges_.size()-1,ranges_.back().size});ranges_.back().size+=bytes;
    };
    u32 partial_count{};
    for(auto it=pages_.begin();it!=pages_.end();){
        const VAddr address=u64(*it)<<Dma::PageShift;
        if(rasterizer.IsDmaReadable(address,Dma::PageSize))append(*it,address,Dma::PageSize);
        else{
            u32 valid{};for(u32 sub=0;sub<Dma::SubpagesPerPage;++sub)
                if(rasterizer.IsDmaReadable(address+u64(sub)*Dma::SubpageSize,Dma::SubpageSize))valid|=1u<<sub;
            if(!valid){it=pages_.erase(it);continue;}
            if(partial_count==Dma::PartialPageCount)throw std::runtime_error("Native DMA partial-page table exceeds buffer batch capacity");
            indirect_entries_.emplace(*it,Dma::PartialFlag|u64(partial_count+1));
            for(u32 sub=0;sub<Dma::SubpagesPerPage;++sub)if(valid&(1u<<sub))
                append(Dma::SecondaryBase+partial_count*Dma::SubpagesPerPage+sub,address+u64(sub)*Dma::SubpageSize,Dma::SubpageSize);
            ++partial_count;
        }
        ++it;
    }
    // Resolve image/CPU ownership before preparing any views. Buffer and DMA
    // aliases in this submission are then materialized together in one batch.
    for(const auto& range:ranges_)rasterizer.ReadMemory(range.address,range.size);
    requests.insert(requests.end(),ranges_.begin(),ranges_.end());
}
void GuestDmaMemory::Bind(Rasterizer& rasterizer,ResourceBindings& bindings,
    std::span<const Rasterizer::ShaderBufferView> views){
    if(views.size()!=ranges_.size())throw std::invalid_argument("Native DMA range/view mismatch");
    struct Bank {ComPtr<ID3D12Resource> resource;u64 base,bytes;};std::vector<Bank> banks;
    std::unordered_map<ID3D12Resource*,std::vector<size_t>> resource_banks;
    auto next=indirect_entries_;
    for(const auto& translation:translations_){
        const auto& view=views[translation.range];const auto width=DX12_API_CALL(view.resource->GetDesc()).Width;
        if(view.size!=ranges_[translation.range].size||view.offset>width||view.size>width-view.offset)
            throw std::out_of_range("Native DMA cache view extent");
            const auto absolute=view.offset+translation.offset;
            auto& candidates=resource_banks[view.resource.Get()];
            auto found=std::find_if(candidates.begin(),candidates.end(),[&](size_t index){const auto& b=banks[index];return
                absolute>=b.base&&absolute-b.base<=b.bytes&&translation.bytes<=b.bytes-(absolute-b.base);});
            size_t bank_index;
            if(found==candidates.end()){
                if(banks.size()==Dma::BankCount)throw std::runtime_error("Native DMA descriptor bank capacity exceeded; scattered cache needs more banks");
                // Cache slices can start mid guest page. Align the native raw
                // view to 16 bytes and keep the exact page offset in the table.
                const u64 base=absolute&~u64(15),bytes=std::min<u64>(Dma::BankSize,width-base)&~u64(3);
                if(bytes<absolute-base+translation.bytes)throw std::out_of_range("Native DMA page crosses cache resource");
                bank_index=banks.size();banks.push_back({view.resource,base,bytes});candidates.push_back(bank_index);
            }else bank_index=*found;
            next.emplace(translation.entry,u64(bank_index)*Dma::BankSize+(absolute-banks[bank_index].base)+1);
    }
    next.emplace(Dma::BankCountEntry,banks.size());
    // Validate every descriptor before changing the persistent page table.
    const bool banks_used=std::any_of(bindings.Layout().Bindings().begin(),bindings.Layout().Bindings().end(),
        [](const auto& binding){return binding.space==Dma::DescriptorSet&&binding.reg==0&&
            binding.type==D3D12_DESCRIPTOR_RANGE_TYPE_UAV&&binding.count==Dma::BankCount;});
    Initialize(rasterizer);
    if(banks_used){
        for(UINT i=0;i<banks.size();++i){const auto& bank=banks[i];
            bindings.Uav(0,Dma::DescriptorSet,bank.resource.Get(),RawView(bank.base,bank.bytes),i);}
        bindings.UnreachableUavTail(0,Dma::DescriptorSet,UINT(banks.size()));
    }
    std::map<u32,u64> patches;
    for(const auto& [page,value]:entries_)if(!next.contains(page))patches.emplace(page,0);
    for(const auto& [page,value]:next){const auto old=entries_.find(page);
        if(old==entries_.end()||old->second!=value)patches.emplace(page,value);}
    if(!patches.empty()){
        std::vector<u64> values;values.reserve(patches.size());for(const auto& [page,value]:patches)values.push_back(value);
        auto upload=std::make_shared<Resource>(Upload(device_.Get(),values.size()*sizeof(u64),values.data()));
        rasterizer.RetainUntilFinish([upload](u64){});
        auto* list=rasterizer.RecordingList();table_.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
        u64 index=0;
        for(auto it=patches.begin();it!=patches.end();){
            const auto first=it->first;u64 count=0;
            do{++count;++it;}while(it!=patches.end()&&it->first==u64(first)+count);
            DX12_API_CALL(list->CopyBufferRegion(table_.handle.Get(),u64(first)*8,upload->handle.Get(),index*8,count*8));
            index+=count;
        }
        table_.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }
    entries_=std::move(next);
}
void GuestDmaMemory::BindMetadata(ResourceBindings& bindings,UINT reg,bool fault){
    auto& resource=fault?faults_:table_;
    for(const auto& binding:bindings.Layout().Bindings())if(binding.type==D3D12_DESCRIPTOR_RANGE_TYPE_UAV&&
        binding.space==0&&reg>=binding.reg&&reg-binding.reg<binding.count){
        bindings.Uav(binding.reg,0,resource.handle.Get(),RawView(0,fault?FaultBytes:TableBytes),reg-binding.reg);return;}
    throw std::runtime_error("Native DMA metadata is not a UAV binding");
}
void GuestDmaMemory::RecordFaults(Rasterizer& rasterizer){
    ConsumeFaults(rasterizer);
    auto available=[&]{return std::find_if(feedback_.begin(),feedback_.end(),[](const auto& slot){return !slot||!slot->pending;});};
    auto found=available();
    if(found==feedback_.end()){
        // Bounded backpressure only when all readback areas are in flight.
        // Ordinary DMA commands must not drain the GPU to inspect fault bits.
        rasterizer.Finish();ConsumeFaults(rasterizer);found=available();
        if(found==feedback_.end())throw std::logic_error("Native DMA feedback did not retire");
    }
    if(!*found){
        auto slot=std::make_shared<Feedback>();
        slot->readback=Buffer(device_.Get(),DmaFaultCompactor::Bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        *found=std::move(slot);
    }
    const auto slot=*found;
    rasterizer.RetainUntilFinish([slot](u64){slot->ready=true;});
    auto* list=rasterizer.RecordingList();compactor_->Dispatch(list,faults_,exported_,zero_faults_);
    DX12_API_CALL(list->CopyBufferRegion(slot->readback.handle.Get(),0,exported_.handle.Get(),0,DmaFaultCompactor::Bytes));
    slot->pending=true;slot->ready=false;
}
void GuestDmaMemory::ConsumeFaults(Rasterizer& rasterizer){
    rasterizer.CollectRetirements();
    for(const auto& slot:feedback_){
        // Only the scheduler's completed-fence callback publishes ready. Map
        // itself does not wait, and unfinished areas must never be overwritten.
        if(!slot||!slot->pending||!slot->ready)continue;
        void* data{};D3D12_RANGE range{0,DmaFaultCompactor::Bytes};
        Check(DX12_API_CALL(slot->readback.handle->Map(0,&range,&data)));
        struct Unmap {ID3D12Resource* resource;~Unmap(){D3D12_RANGE empty{};DX12_API_CALL(resource->Unmap(0,&empty));}} unmap{slot->readback.handle.Get()};
        const auto* words=static_cast<const u32*>(data);
        const auto count=std::min(words[0],DmaFaultCompactor::Capacity);
        for(u32 i=1;i<=count;++i){
            if(words[i]>=Dma::PageCount)throw std::runtime_error("Native DMA feedback page out of range");
            pages_.insert(words[i]);
        }
        slot->pending=false;slot->ready=false;
    }
}
}
