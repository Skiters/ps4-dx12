#pragma once
#include "common/reference_trace.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
namespace ReferenceTrace {
// Only bounds one recorded GPU operation. No WAIT query flag or new CPU fence wait.
class GpuScope {
    inline static std::atomic<unsigned> pending{};
    const Vulkan::Instance* instance{};Vulkan::Scheduler* scheduler{};
    vk::QueryPool pool{};vk::CommandBuffer command{};const char* name{};
    U64 frame{},tick{},parent_id{};unsigned valid_bits{};double period{};
public:
    GpuScope(const Vulkan::Instance& i,Vulkan::Scheduler& s,vk::CommandBuffer cmd,const char* label){
        if(!Enabled()||!std::getenv("SHADPS4_REF_GPU_TIMING"))return;
        if(pending.fetch_add(1)>=64){--pending;Emit("gpu.timestamp.skipped_capacity");return;}
        try{
            valid_bits=i.GetPhysicalDevice().getQueueFamilyProperties().at(i.GetGraphicsQueueFamilyIndex()).timestampValidBits;
            if(!valid_bits){--pending;Emit("gpu.timestamp.unsupported");return;}
            period=i.GetPhysicalDevice().getProperties().limits.timestampPeriod;
            const vk::QueryPoolCreateInfo info{.queryType=vk::QueryType::eTimestamp,.queryCount=2};
            const auto created=i.GetDevice().createQueryPool(info);
            if(created.result!=vk::Result::eSuccess){--pending;Emit("gpu.timestamp.setup_failed");return;}
            pool=created.value;
            instance=&i;scheduler=&s;command=cmd;name=label;frame=frame_hint.load();tick=s.CurrentTick();parent_id=active_id;
            command.resetQueryPool(pool,0,2);command.writeTimestamp(vk::PipelineStageFlagBits::eAllCommands,pool,0);
        }catch(...){--pending;Emit("gpu.timestamp.setup_failed");}
    }
    ~GpuScope(){
        if(!name)return;command.writeTimestamp(vk::PipelineStageFlagBits::eAllCommands,pool,1);
        scheduler->DeferOperation([i=instance,q=pool,label=name,f=frame,t=tick,bits=valid_bits,p=period,parent=parent_id]{
            std::array<U64,2> data{};
            try{
                const auto result=i->GetDevice().getQueryPoolResults(q,0,2,sizeof(data),data.data(),sizeof(U64),vk::QueryResultFlagBits::e64);
                if(result==vk::Result::eSuccess){const U64 mask=bits==64?~U64(0):((U64(1)<<bits)-1);Emit(label,U64(double((data[1]-data[0])&mask)*p),data[0],data[1],t,f,parent);}
                else Emit("gpu.timestamp.not_ready",U64(result),0,0,t,f);
            }catch(...){Emit("gpu.timestamp.read_failed",0,0,0,t,f);}
            i->GetDevice().destroyQueryPool(q);--pending;
        });
    }
};
}
