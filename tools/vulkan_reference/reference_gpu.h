#pragma once
#include "common/reference_trace.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include <memory>
namespace ReferenceTrace {
// Only bounds one recorded GPU operation. No WAIT query flag or new CPU fence wait.
class GpuScope {
    inline static std::atomic<unsigned> pending{},pending_batch{};
    std::atomic<unsigned>* capacity{};
    const Vulkan::Instance* instance{};Vulkan::Scheduler* scheduler{};
    vk::QueryPool pool{};vk::CommandBuffer command{};const char* name{};
    struct PoolOwner {
        vk::Device device;vk::QueryPool pool;std::atomic<unsigned>* capacity;
        PoolOwner(vk::Device d,vk::QueryPool p,std::atomic<unsigned>* c):device(d),pool(p),capacity(c){}
        ~PoolOwner(){device.destroyQueryPool(pool);--*capacity;}
    };
    std::shared_ptr<PoolOwner> owner;
    U64 frame{},tick{},parent_id{};unsigned valid_bits{};double period{};
public:
    GpuScope(const Vulkan::Instance& i,Vulkan::Scheduler& s,vk::CommandBuffer cmd,const char* label){
        if(!Enabled()||!std::getenv("SHADPS4_REF_GPU_TIMING"))return;
        capacity=std::strncmp(label,"gpu.queue.",10)==0?&pending_batch:&pending;
        if(capacity->fetch_add(1)>=64){--*capacity;Emit("gpu.timestamp.skipped_capacity");return;}
        try{
            valid_bits=i.GetPhysicalDevice().getQueueFamilyProperties().at(i.GetGraphicsQueueFamilyIndex()).timestampValidBits;
            if(!valid_bits){--*capacity;Emit("gpu.timestamp.unsupported");return;}
            period=i.GetPhysicalDevice().getProperties().limits.timestampPeriod;
            const vk::QueryPoolCreateInfo info{.queryType=vk::QueryType::eTimestamp,.queryCount=2};
            const auto created=i.GetDevice().createQueryPool(info);
            if(created.result!=vk::Result::eSuccess){--*capacity;Emit("gpu.timestamp.setup_failed");return;}
            pool=created.value;
            owner=std::make_shared<PoolOwner>(i.GetDevice(),pool,capacity);
            instance=&i;scheduler=&s;command=cmd;name=label;frame=frame_hint.load();tick=s.CurrentTick();parent_id=active_id;
            command.resetQueryPool(pool,0,2);command.writeTimestamp(vk::PipelineStageFlagBits::eAllCommands,pool,0);
        }catch(...){
            if(owner)owner.reset();else{if(pool)i.GetDevice().destroyQueryPool(pool);--*capacity;}
            name=nullptr;Emit("gpu.timestamp.setup_failed");
        }
    }
    ~GpuScope(){
        if(!name)return;command.writeTimestamp(vk::PipelineStageFlagBits::eAllCommands,pool,1);
        scheduler->DeferOperation([i=instance,q=pool,label=name,f=frame,t=tick,bits=valid_bits,p=period,parent=parent_id,keep=owner]{
            std::array<U64,2> data{};
            try{
                const auto result=i->GetDevice().getQueryPoolResults(q,0,2,sizeof(data),data.data(),sizeof(U64),vk::QueryResultFlagBits::e64);
                if(result==vk::Result::eSuccess){const U64 mask=bits==64?~U64(0):((U64(1)<<bits)-1);Emit(label,U64(double((data[1]-data[0])&mask)*p),data[0],data[1],t,f,parent);}
                else Emit("gpu.timestamp.not_ready",U64(result),0,0,t,f);
            }catch(...){Emit("gpu.timestamp.read_failed",0,0,0,t,f);}
            // Owner also releases pools if an unsubmitted callback is discarded
            // during normal scheduler destruction, rather than leaking them.
        });
    }
};
}
