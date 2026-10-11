#include "reference_trace.h"
#include <cassert>
#include <thread>
#include <vector>
int main() {
    assert(ReferenceTrace::Enabled());
    constexpr unsigned workers=4, calls=10000;
    std::vector<std::thread> threads;
    for(unsigned i=0;i<workers;++i)threads.emplace_back([]{
        for(unsigned n=0;n<calls;++n){ReferenceTrace::Scope parent("smoke.parent");
            {ReferenceTrace::Scope child("smoke.child");ReferenceTrace::Emit("smoke.bytes",64);ReferenceTrace::Packet(0x2d,1);}
        }
    });
    for(auto& thread:threads)thread.join();
    unsigned checked=0;
    for(const auto& a:ReferenceTrace::totals)if(const char* name=a.name.load()){
        if(std::strcmp(name,"smoke.parent")==0||std::strcmp(name,"smoke.child")==0){assert(a.calls==workers*calls);assert(a.exclusive_wall<=a.wall);++checked;}
        if(std::strcmp(name,"smoke.bytes")==0){assert(a.calls==workers*calls);assert(a.sum_a==64ull*workers*calls);++checked;}
    }
    assert(checked==3 && ReferenceTrace::aggregate_overflow==0);
    assert(ReferenceTrace::packet_counts[256+0x2d]==workers*calls && ReferenceTrace::invalid_packet_categories==0);
}
