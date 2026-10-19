// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/serdes.h"
#include <d3d12.h>
#include <string>
#include <vector>
#include <stdexcept>

namespace NativeDX12 {
// Host-internal primitive stream layouts must outlive the temporary shader
// bridge declarations. This does not enable guest VGT stream-output commands.
class StreamOutputLayout {
    std::vector<D3D12_SO_DECLARATION_ENTRY> entries_;
    std::vector<std::string> semantics_;
    std::vector<UINT> strides_;
    UINT rasterized_stream_{};
public:
    static void Validate(const D3D12_STREAM_OUTPUT_DESC& d){
        if(d.NumEntries>D3D12_SO_STREAM_COUNT*D3D12_SO_OUTPUT_COMPONENT_COUNT||
           d.NumStrides>D3D12_SO_BUFFER_SLOT_COUNT||
           (d.NumEntries&&!d.pSODeclaration)||(d.NumStrides&&!d.pBufferStrides)||
           (d.NumEntries&&!d.NumStrides)||
           (d.RasterizedStream>=D3D12_SO_STREAM_COUNT&&d.RasterizedStream!=D3D12_SO_NO_RASTERIZED_STREAM))
            throw std::invalid_argument("Invalid native primitive stream layout");
        for(UINT i=0;i<d.NumStrides;++i)
            if(d.pBufferStrides[i]%4||d.pBufferStrides[i]>D3D12_SO_BUFFER_MAX_STRIDE_IN_BYTES)
                throw std::invalid_argument("Invalid native primitive stream stride");
        for(UINT i=0;i<d.NumEntries;++i){const auto& e=d.pSODeclaration[i];
            if(e.Stream>=D3D12_SO_STREAM_COUNT||e.OutputSlot>=d.NumStrides||!e.ComponentCount||
               (e.SemanticName&&(e.StartComponent+e.ComponentCount>4||!e.SemanticName[0]))||
               (!e.SemanticName&&e.StartComponent))
                throw std::invalid_argument("Invalid native primitive stream declaration");
        }
    }
    StreamOutputLayout()=default;
    explicit StreamOutputLayout(const D3D12_STREAM_OUTPUT_DESC& d):rasterized_stream_(d.RasterizedStream){
        Validate(d);
        entries_.reserve(d.NumEntries);semantics_.reserve(d.NumEntries);
        for(UINT i=0;i<d.NumEntries;++i){
            entries_.push_back(d.pSODeclaration[i]);
            semantics_.emplace_back(d.pSODeclaration[i].SemanticName?d.pSODeclaration[i].SemanticName:"");
        }
        for(UINT i=0;i<d.NumEntries;++i)
            entries_[i].SemanticName=d.pSODeclaration[i].SemanticName?semantics_[i].c_str():nullptr;
        if(d.NumStrides)strides_.assign(d.pBufferStrides,d.pBufferStrides+d.NumStrides);
    }
    StreamOutputLayout(const StreamOutputLayout&)=delete;
    StreamOutputLayout& operator=(const StreamOutputLayout&)=delete;
    StreamOutputLayout(StreamOutputLayout&&)=default;
    StreamOutputLayout& operator=(StreamOutputLayout&&)=default;
    D3D12_STREAM_OUTPUT_DESC Desc()const{
        return {entries_.data(),UINT(entries_.size()),strides_.data(),UINT(strides_.size()),rasterized_stream_};
    }
    template<class Key>static void AppendKey(Key& key,const D3D12_STREAM_OUTPUT_DESC& d){
        Validate(d);key.Add(d.NumEntries);key.Add(d.NumStrides);
        for(UINT i=0;i<d.NumEntries;++i){const auto& e=d.pSODeclaration[i];
            key.Add(e.Stream);key.Add(e.SemanticName!=nullptr);
            if(e.SemanticName)key.Text(e.SemanticName);
            key.Add(e.SemanticIndex);key.Add(e.StartComponent);key.Add(e.ComponentCount);key.Add(e.OutputSlot);
        }
        for(UINT i=0;i<d.NumStrides;++i)key.Add(d.pBufferStrides[i]);
        key.Add(d.RasterizedStream);
    }
    void Save(Serialization::Writer& save)const{
        const auto d=Desc();save.Write(d.NumEntries);save.Write(d.NumStrides);save.Write(d.RasterizedStream);
        for(const auto& e:entries_){
            save.Write(e.Stream);save.Write(e.SemanticName!=nullptr);
            if(e.SemanticName)save.Write(std::string(e.SemanticName));
            save.Write(e.SemanticIndex);save.Write(e.StartComponent);save.Write(e.ComponentCount);save.Write(e.OutputSlot);
        }
        for(const auto stride:strides_)save.Write(stride);
    }
    static StreamOutputLayout Load(Serialization::Reader& load){
        UINT count{},stride_count{},rasterized{};load.Read(count);load.Read(stride_count);load.Read(rasterized);
        if(count>D3D12_SO_STREAM_COUNT*D3D12_SO_OUTPUT_COMPONENT_COUNT||stride_count>D3D12_SO_BUFFER_SLOT_COUNT)
            throw std::runtime_error("Invalid cached primitive stream extent");
        std::vector<D3D12_SO_DECLARATION_ENTRY> entries(count);std::vector<std::string> names(count);
        std::vector<UINT> strides(stride_count);
        for(UINT i=0;i<count;++i){auto& e=entries[i];bool named{};load.Read(e.Stream);load.Read(named);
            if(named){load.Read(names[i]);e.SemanticName=names[i].c_str();}
            load.Read(e.SemanticIndex);load.Read(e.StartComponent);load.Read(e.ComponentCount);load.Read(e.OutputSlot);
        }
        for(auto& stride:strides)load.Read(stride);
        return StreamOutputLayout({entries.data(),count,strides.data(),stride_count,rasterized});
    }
};
} // namespace NativeDX12
