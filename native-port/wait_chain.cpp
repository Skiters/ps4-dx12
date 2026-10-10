// SPDX-License-Identifier: GPL-2.0-or-later
// One-shot read-only wait-chain inspection, separate from the emulator runtime.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wct.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstdlib>
int wmain(int argc,wchar_t** argv){
    if(argc!=2)return 2;const DWORD pid=wcstoul(argv[1],nullptr,10);
    auto session=OpenThreadWaitChainSession(0,nullptr);if(!session)return int(GetLastError());
    auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);THREADENTRY32 thread{sizeof(thread)};
    unsigned failures{};
    if(Thread32First(snapshot,&thread))do{
        if(thread.th32OwnerProcessID!=pid)continue;
        WAITCHAIN_NODE_INFO nodes[WCT_MAX_NODE_COUNT]{};DWORD count=WCT_MAX_NODE_COUNT;BOOL cycle{};
        if(!GetThreadWaitChain(session,0,WCT_OUT_OF_PROC_CS_FLAG,thread.th32ThreadID,&count,nodes,&cycle)){
            printf("THREAD %lu ERROR %lu\n",thread.th32ThreadID,GetLastError());++failures;continue;
        }
        if(count<=1&&!cycle)continue;
        printf("THREAD %lu cycle=%d nodes=%lu\n",thread.th32ThreadID,int(cycle),count);
        for(DWORD i=0;i<count;++i){
            if(nodes[i].ObjectType==WctThreadType)printf("  thread pid=%lu tid=%lu status=%u wait_ms=%lu\n",
                nodes[i].ThreadObject.ProcessId,nodes[i].ThreadObject.ThreadId,nodes[i].ObjectStatus,nodes[i].ThreadObject.WaitTime);
            else printf("  object type=%u status=%u\n",nodes[i].ObjectType,nodes[i].ObjectStatus);
        }
    }while(Thread32Next(snapshot,&thread));
    CloseHandle(snapshot);CloseThreadWaitChainSession(session);return failures?1:0;
}
