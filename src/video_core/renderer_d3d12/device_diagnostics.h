// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "resources.h"
#include <algorithm>
#include <cstdio>
#include <cwchar>
namespace NativeDX12 {
inline void PrintDredWideText(const wchar_t* label) noexcept {
    if(!label)return;
    while(*label){
        unsigned value=static_cast<unsigned>(*label++);
        if(value>=0xd800&&value<=0xdbff){
            const auto low=static_cast<unsigned>(*label);
            if(low>=0xdc00&&low<=0xdfff){++label;value=0x10000+((value-0xd800)<<10)+(low-0xdc00);}
            else value=0xfffd;
        }else if(value>=0xdc00&&value<=0xdfff)value=0xfffd;
        unsigned char bytes[4];size_t count{};
        if(value<0x80){bytes[0]=static_cast<unsigned char>(value);count=1;}
        else if(value<0x800){bytes[0]=static_cast<unsigned char>(0xc0|(value>>6));bytes[1]=static_cast<unsigned char>(0x80|(value&0x3f));count=2;}
        else if(value<0x10000){bytes[0]=static_cast<unsigned char>(0xe0|(value>>12));bytes[1]=static_cast<unsigned char>(0x80|((value>>6)&0x3f));bytes[2]=static_cast<unsigned char>(0x80|(value&0x3f));count=3;}
        else{bytes[0]=static_cast<unsigned char>(0xf0|(value>>18));bytes[1]=static_cast<unsigned char>(0x80|((value>>12)&0x3f));bytes[2]=static_cast<unsigned char>(0x80|((value>>6)&0x3f));bytes[3]=static_cast<unsigned char>(0x80|(value&0x3f));count=4;}
        fwrite(bytes,1,count,stderr);
    }
}
inline void ReportDeviceRemoval(ID3D12Device* device) noexcept {
    if(!device)return;
    const auto reason=device->GetDeviceRemovedReason();if(SUCCEEDED(reason))return;
    fprintf(stderr,"DX12 device removed: HRESULT=%08lx\n",(unsigned long)reason);
    ComPtr<ID3D12DeviceRemovedExtendedData1> dred;
    if(FAILED(device->QueryInterface(IID_PPV_ARGS(&dred))))return;
    D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 breadcrumbs{};
    if(SUCCEEDED(dred->GetAutoBreadcrumbsOutput1(&breadcrumbs))){
        const auto* node=breadcrumbs.pHeadAutoBreadcrumbNode;UINT nodes{};
        for(;node&&nodes<1024;node=node->pNext,++nodes){
            const UINT completed=node->pLastBreadcrumbValue?std::min(*node->pLastBreadcrumbValue,node->BreadcrumbCount):0;
            fprintf(stderr,"DX12 DRED list=%p queue=%p completed=%u count=%u list_name=%s queue_name=%s\n",
                static_cast<void*>(node->pCommandList),static_cast<void*>(node->pCommandQueue),completed,node->BreadcrumbCount,
                node->pCommandListDebugNameA?node->pCommandListDebugNameA:"",node->pCommandQueueDebugNameA?node->pCommandQueueDebugNameA:"");
            if(node->pCommandListDebugNameW){fputs("DX12 DRED list_name_w=",stderr);PrintDredWideText(node->pCommandListDebugNameW);fputc('\n',stderr);}
            if(node->pCommandQueueDebugNameW){fputs("DX12 DRED queue_name_w=",stderr);PrintDredWideText(node->pCommandQueueDebugNameW);fputc('\n',stderr);}
            if(node->pCommandHistory){const UINT first=completed>16?completed-16:0;
                const UINT last=completed+std::min(16u,node->BreadcrumbCount-completed);
                for(UINT step=first;step<last;++step)fprintf(stderr,"DX12 DRED step=%u operation=%u\n",step,UINT(node->pCommandHistory[step]));}
            if(node->pBreadcrumbContexts)for(UINT context=0;context<node->BreadcrumbContextsCount;++context){
                const auto& value=node->pBreadcrumbContexts[context];
                if(value.pContextString){fprintf(stderr,"DX12 DRED marker step=%u label=",value.BreadcrumbIndex);PrintDredWideText(value.pContextString);fputc('\n',stderr);}
            }
        }
        if(node)fprintf(stderr,"DX12 DRED breadcrumb-node output truncated at 1024 nodes\n");
    }
    D3D12_DRED_PAGE_FAULT_OUTPUT1 fault{};
    if(SUCCEEDED(dred->GetPageFaultAllocationOutput1(&fault))){
        fprintf(stderr,"DX12 DRED page_fault=%llx\n",(unsigned long long)fault.PageFaultVA);
        const auto allocations=[](const D3D12_DRED_ALLOCATION_NODE1* node,const char* state){
            UINT count{};for(;node&&count<1024;node=node->pNext,++count){
                fprintf(stderr,"DX12 DRED allocation state=%s type=%u object=%p name=%s\n",state,UINT(node->AllocationType),
                    static_cast<const void*>(node->pObject),node->ObjectNameA?node->ObjectNameA:"");
                if(node->ObjectNameW){fputs("DX12 DRED allocation_name_w=",stderr);PrintDredWideText(node->ObjectNameW);fputc('\n',stderr);}
            }
            if(node)fprintf(stderr,"DX12 DRED %s allocation output truncated at 1024 nodes\n",state);
        };
        allocations(fault.pHeadExistingAllocationNode,"existing");allocations(fault.pHeadRecentFreedAllocationNode,"freed");
    }
    fflush(stderr);
}
}
