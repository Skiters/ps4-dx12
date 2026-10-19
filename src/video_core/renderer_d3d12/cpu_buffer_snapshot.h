// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>
#include <span>
#include <cstdint>

namespace NativeDX12 {
// CPU equality witnesses can also supply deferred host uploads. A worker only
// sees an immutable lease; subsequent CPU/GPU reconciliation detaches before
// changing those bytes. No worker ever reads a protected guest VA.
class CpuBufferSnapshot {
    std::shared_ptr<std::vector<std::byte>> bytes_;
    void MakeWritable() {
        if(!bytes_)bytes_=std::make_shared<std::vector<std::byte>>();
        else if(bytes_.use_count()!=1)bytes_=std::make_shared<std::vector<std::byte>>(*bytes_);
    }
public:
    size_t size()const{return bytes_?bytes_->size():0;}
    size_t capacity()const{return bytes_?bytes_->capacity():0;}
    bool empty()const{return size()==0;}
    const std::byte* data()const{return bytes_?bytes_->data():nullptr;}
    std::byte* WriteData(){MakeWritable();return bytes_->data();}
    void resize(size_t size){MakeWritable();bytes_->resize(size);}
    void clear(){if(bytes_&&bytes_.use_count()==1)bytes_->clear();else bytes_.reset();}
    CpuBufferSnapshot& operator=(std::vector<std::byte> bytes){
        bytes_=std::make_shared<std::vector<std::byte>>(std::move(bytes));return *this;
    }
    std::shared_ptr<const std::vector<std::byte>> Share()const{return bytes_;}
    void Assign(std::span<const std::byte> source){
        if(source.data()==data()&&source.size()==size())return;
        const auto from=reinterpret_cast<uintptr_t>(source.data()),to=reinterpret_cast<uintptr_t>(data());
        const bool overlap=!source.empty()&&!empty()&&
            (from>=to?from-to<size():to-from<source.size());
        // Construct the replacement before releasing old storage, including
        // subspans of this very snapshot. Unshared external sources reuse capacity.
        if(!bytes_||bytes_.use_count()!=1||overlap)
            bytes_=std::make_shared<std::vector<std::byte>>(source.begin(),source.end());
        else bytes_->assign(source.begin(),source.end());
    }
};
} // namespace NativeDX12
