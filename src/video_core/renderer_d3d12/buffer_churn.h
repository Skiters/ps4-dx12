// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "diagnostics.h"
#include "common/types.h"
#include <map>
#include <vector>

namespace NativeDX12 {
// Bounded first-4096-address sample, ranked by materialized bytes. Counters
// outside this sample remain exhaustive. One report per armed capture window.
class BufferChurn {
    struct Row {
        u64 address{},requested{},base{},size{},allocation{},reuse{},recreate{},materialized{},uploaded{},fence{};
        bool dirty{},owned{};
        const char* reason="first-use";
    };
    std::map<u64,Row> rows_;
    bool capturing_{};
    u64 excluded_{};
public:
    void Poll() {
        if(!Diagnostics::Enabled())return;
        const bool capture=Diagnostics::Hub::Get().capturing;
        if(capture==capturing_)return;
        capturing_=capture;
        if(capture){rows_.clear();excluded_=0;return;}
        std::vector<Row> sorted;for(auto& [_,r]:rows_)sorted.push_back(r);
        std::sort(sorted.begin(),sorted.end(),[](const auto& a,const auto& b){return a.materialized>b.materialized;});
        fprintf(stderr,"DX12 churn sample=%zu excluded_events=%llu top=16\n",rows_.size(),excluded_);
        for(size_t i=0;i<std::min<size_t>(16,sorted.size());++i){const auto& r=sorted[i];
            fprintf(stderr,"DX12 churn address=%llx request=%llu base=%llx size=%llu allocation=%llu reuse=%llu recreate=%llu reason=%s dirty=%u owned=%u completed=%llu materialized=%llu uploaded=%llu\n",
                r.address,r.requested,r.base,r.size,r.allocation,r.reuse,r.recreate,r.reason,unsigned(r.dirty),unsigned(r.owned),r.fence,r.materialized,r.uploaded);
        }
        fflush(stderr);
    }
    void Record(u64 address,u64 requested,u64 base,u64 size,u64 allocation,bool fresh,bool dirty,bool owned,u64 fence,const char* reason=nullptr,u64 uploaded=0) {
        Poll();if(!capturing_)return;
        auto it=rows_.find(address);
        if(it==rows_.end()){
            if(rows_.size()==4096){++excluded_;return;}
            it=rows_.emplace(address,Row{}).first;
        }
        auto& r=it->second;r.address=address;r.requested=requested;r.base=base;r.size=size;
        r.allocation=allocation;r.dirty=dirty;r.owned=owned;r.fence=fence;
        if(fresh){++r.recreate;r.materialized+=size;}else if(!reason&&!uploaded)++r.reuse;
        if(reason)r.reason=reason;r.uploaded+=uploaded;
    }
};
}
