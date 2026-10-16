// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "video_core/renderer_d3d12/command_scheduler.h"
#include <d3d12sdklayers.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

namespace NativeDX12 {
// Authored, unrun real-queue submission ownership and publication-failure probe.
inline void RunSchedulerSubmissionFixture(ID3D12Device* device) {
    const auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    require(device!=nullptr,"Submission fixture needs a D3D12 device");
    ComPtr<ID3D12InfoQueue> info;Check(device->QueryInterface(IID_PPV_ARGS(&info)));
    UINT64 first_message=info->GetNumStoredMessages();
    const auto initial_discarded=info->GetNumMessagesDiscardedByMessageCountLimit();
    const auto validate=[&]{bool severe=false;
        require(info->GetNumMessagesDiscardedByMessageCountLimit()==initial_discarded,
            "Submission fixture lost D3D12 validation messages");
        const auto end=info->GetNumStoredMessages();
        require(end>=first_message,"Submission fixture validation queue was cleared externally");
        for(UINT64 i=first_message;i<end;++i){SIZE_T size{};Check(info->GetMessage(i,nullptr,&size));
            std::vector<std::byte> bytes(size);auto* message=reinterpret_cast<D3D12_MESSAGE*>(bytes.data());
            Check(info->GetMessage(i,message,&size));if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){
                std::fprintf(stderr,"Submission fixture validation: %s\n",message->pDescription);severe=true;}}
        first_message=end;require(!severe,"D3D12 rejected scheduler submission fixture commands");};
    struct PublicationFailure final:std::exception {
        const char* what()const noexcept override{return "injected submission publication failure";}
    };
    struct State {Resource upload,readback;std::array<u32,4> expected;};
    for(const bool fail:{false,true}){
        unsigned retired=0,published_b=0,published_c=0,replayed_copy=0;
        u64 retirement_fence=0,b_fence=0,c_fence=0;
        CommandScheduler scheduler(device);
        ComPtr<ID3D12Fence> gate;Check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&gate)));
        std::array<std::shared_ptr<State>,3> states;
        std::array<std::weak_ptr<State>,3> weak;
        std::vector<std::function<void(u64)>> retirements,publications;
        // A latched submission failure prevents Finish from submitting/waiting.
        // Release the external queue wait AND drain its known submitted fence
        // before scheduler destruction, including assertion/callback failures.
        struct GateRelease {
            CommandScheduler& scheduler;ComPtr<ID3D12Fence> gate;bool done=false;
            ~GateRelease(){if(!done)try{Check(gate->Signal(1));auto fence=scheduler.Submitted();
                if(!fence&&scheduler.Recording())fence=scheduler.Submit();
                if(fence)scheduler.Wait(fence);scheduler.Collect();}catch(...) {}}
        } release{scheduler,gate};
        Check(scheduler.Queue()->Wait(gate.Get(),1));
        const auto check_bytes=[&](const State& state){void* mapped{};D3D12_RANGE range{0,sizeof(state.expected)};
            Check(state.readback.handle->Map(0,&range,&mapped));
            const bool valid=std::memcmp(mapped,state.expected.data(),sizeof(state.expected))==0;
            D3D12_RANGE empty{};state.readback.handle->Unmap(0,&empty);
            require(valid,"Submission-owned upload/readback lease lost its exact bytes");};
        auto* list=scheduler.Begin();
        for(u32 i=0;i<states.size();++i){auto& state=states[i];state=std::make_shared<State>();
            state->expected={0x12345678u+i,0xabcdef01u^i,fail?0xdeadbeefu:0x0badc0deu,0x87654321u-i};
            state->upload=Buffer(device,sizeof(state->expected),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
            state->readback=Buffer(device,sizeof(state->expected),D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            void* mapped{};D3D12_RANGE empty{};Check(state->upload.handle->Map(0,&empty,&mapped));
            std::memcpy(mapped,state->expected.data(),sizeof(state->expected));state->upload.handle->Unmap(0,nullptr);
            list->CopyBufferRegion(state->readback.handle.Get(),0,state->upload.handle.Get(),0,sizeof(state->expected));weak[i]=state;
        }
        retirements.emplace_back([state=states[0],&retired,&retirement_fence,check_bytes](u64 fence){
            check_bytes(*state);retirement_fence=fence;++retired;
        });
        publications.emplace_back([state=states[1],&published_b,&b_fence,fail,require](u64 fence){
            require(bool(state->upload.handle)&&bool(state->readback.handle),"B publication lost captured resources");
            b_fence=fence;++published_b;if(fail)throw PublicationFailure{};
        });
        publications.emplace_back([state=states[2],&published_c,&c_fence,require](u64 fence){
            require(bool(state->upload.handle)&&bool(state->readback.handle),"C publication lost captured resources");
            c_fence=fence;++published_c;
        });
        bool threw=false;u64 fence=0;
        try{fence=scheduler.Submit(retirements,publications);}catch(const PublicationFailure&){threw=true;}
        require(threw==fail,"Submission publication failure did not follow the selected case");
        if(fail)fence=scheduler.Submitted();
        require(fence&&fence==scheduler.Submitted()&&b_fence==fence,"Publication did not expose its actual submitted fence");
        require(published_b==1&&published_c==(fail?0u:1u)&&(!published_c||c_fence==fence),"Publication callback order/count changed");
        // Simulate caller unwinding: from here only scheduler-owned callback
        // copies may keep A/B/C resource states alive. Weak pointers do not own.
        states.fill({});retirements.clear();publications.clear();
        scheduler.Collect();
        require(gate->GetCompletedValue()==0&&scheduler.Completed()<fence&&retired==0,"Retirement ran before the blocked GPU fence");
        for(const auto& state:weak)require(!state.expired(),"Submitted resource capture died while GPU queue was gated");
        if(fail){
            const auto reject=[&](auto&& action){bool rejected=false;try{action();}catch(const PublicationFailure&){rejected=true;}
                require(rejected,"Failed publication allowed command-list replay");};
            reject([&]{(void)scheduler.Begin();});reject([&]{(void)scheduler.Submit();});
            reject([&]{scheduler.SubmitCopy([&](auto*){++replayed_copy;});});
            require(replayed_copy==0&&published_b==1&&published_c==0&&retired==0,"Rejected replay executed a callback or recorder");
            for(const auto& state:weak)require(!state.expired(),"Terminal submission dropped an uncalled publication capture");
        }
        Check(gate->Signal(1));scheduler.Wait(fence);
        // Wait does not collect leases. Inspect all three real GPU copies with
        // temporary weak locks, releasing them before the sole retirement step.
        for(const auto& state:weak){const auto retained=state.lock();require(bool(retained),"Wait retired a callback-owned resource early");check_bytes(*retained);}
        require(retired==0,"Wait invoked a deferred retirement callback");
        scheduler.Collect();release.done=true;
        require(retired==1&&retirement_fence==fence&&published_b==1&&published_c==(fail?0u:1u),"Fence collection replayed or lost publication/retirement effects");
        for(const auto& state:weak)require(state.expired(),"Fence collection retained a completed callback resource lease");
        scheduler.Collect();require(retired==1,"Repeated collection replayed retirement");
        validate();
        std::printf("PASS: scheduler %s publication; gated GPU copies, callback-only resource leases and exact retirement\n",fail?"throwing":"normal");
    }
    {
        unsigned first=0,later=0;
        CommandScheduler scheduler(device);(void)scheduler.Begin();
        const auto fence=scheduler.Submit();require(fence!=0,"Retirement drain case needs a real GPU fence");scheduler.Wait(fence);
        struct RetirementFailure final:std::exception {};
        scheduler.Defer(fence,[&](u64 completed){require(completed==fence,"First retirement got a different fence");++first;throw RetirementFailure{};});
        scheduler.Defer(fence,[&](u64 completed){require(completed==fence,"Later retirement got a different fence");++later;});
        bool threw=false;try{scheduler.Collect();}catch(const RetirementFailure&){threw=true;}
        require(threw&&first==1&&later==1,"Throwing retirement lost the error or abandoned a following ready callback");
        scheduler.Collect();require(first==1&&later==1,"Repeated collection replayed a drained retirement callback");
        validate();
        std::puts("PASS: scheduler retirement exception reports after all ready callbacks drain exactly once");
    }
}
} // namespace NativeDX12
