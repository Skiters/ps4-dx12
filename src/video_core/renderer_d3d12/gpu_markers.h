// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "resources.h"
#include "../../../native-port/vendor/pix-events/include/PIXEventsLegacy.h"
#include <cstring>
#include <string_view>
#include <vector>
namespace NativeDX12::GpuMarkers {
inline void Record(ID3D12GraphicsCommandList* list,std::string_view label,UINT color,bool begin){
    label=label.substr(0,label.find('\0'));
    if(label.size()>UINT_MAX-40)throw std::out_of_range("Native PIX marker exceeds command payload size");
    std::vector<UINT64> payload(4+(label.size()+8)/8);
    payload[0]=PixEventsLegacy::PIXEncodeEventInfo(0,begin?PixEventsLegacy::PIXEvent_BeginEvent_NoArgs:
        PixEventsLegacy::PIXEvent_SetMarker_NoArgs);
    payload[1]=color;
    payload[2]=PixEventsLegacy::PIXEncodeStringInfo(0,8,TRUE,FALSE);
    if(!label.empty())memcpy(payload.data()+3,label.data(),label.size());
    if(begin)list->BeginEvent(2,payload.data(),UINT(payload.size()*sizeof(UINT64)));
    else list->SetMarker(2,payload.data(),UINT(payload.size()*sizeof(UINT64)));
}
inline void Begin(ID3D12GraphicsCommandList* list,std::string_view label){Record(list,label,0xff000000,true);}
inline void End(ID3D12GraphicsCommandList* list){list->EndEvent();}
inline void Insert(ID3D12GraphicsCommandList* list,std::string_view label,UINT color){Record(list,label,color,false);}
class Scope {
    ID3D12GraphicsCommandList* list_{};
public:
    Scope(ID3D12GraphicsCommandList* list,bool enabled,std::string_view label){
        if(enabled){Begin(list,label);list_=list;}
    }
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
    ~Scope(){End();}
    void End(){if(list_){GpuMarkers::End(list_);list_=nullptr;}}
};
}
