// SPDX-License-Identifier: GPL-2.0-or-later
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <cstdio>
#include <string>
#include <vector>
static void Stack(HANDLE process,HANDLE thread){
    CONTEXT context{};context.ContextFlags=CONTEXT_FULL;
    if(!GetThreadContext(thread,&context))return;
    STACKFRAME64 frame{};frame.AddrPC={context.Rip,0,AddrModeFlat};
    frame.AddrFrame={context.Rbp,0,AddrModeFlat};frame.AddrStack={context.Rsp,0,AddrModeFlat};
    for(unsigned i=0;i<40&&frame.AddrPC.Offset;++i){
        const auto base=SymGetModuleBase64(process,frame.AddrPC.Offset);wchar_t name[MAX_PATH]{};
        if(base)GetModuleFileNameExW(process,reinterpret_cast<HMODULE>(base),name,MAX_PATH);
        fwprintf(stderr,L"DEBUG STACK %u: %ls + 0x%llx (0x%llx)\n",i,name,
            frame.AddrPC.Offset-base,frame.AddrPC.Offset);
        if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,process,thread,&frame,&context,nullptr,
            SymFunctionTableAccess64,SymGetModuleBase64,nullptr))break;
    }
    fflush(stderr);
}
int wmain(int argc,wchar_t** argv){
    if(argc==3&&std::wstring(argv[1])==L"--snapshot"){
        const auto pid=DWORD(wcstoul(argv[2],nullptr,10));
        auto process=OpenProcess(PROCESS_VM_READ|PROCESS_QUERY_INFORMATION,FALSE,pid);
        if(!process)return int(GetLastError());
        SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME);SymInitialize(process,nullptr,TRUE);
        auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
        THREADENTRY32 entry{};entry.dwSize=sizeof(entry);
        if(Thread32First(snapshot,&entry))do{if(entry.th32OwnerProcessID!=pid)continue;
            auto thread=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,entry.th32ThreadID);
            if(!thread)continue;
            if(SuspendThread(thread)!=DWORD(-1)){
                PWSTR name{};GetThreadDescription(thread,&name);FILETIME create{},exit{},kernel{},user{};GetThreadTimes(thread,&create,&exit,&kernel,&user);
                fwprintf(stderr,L"DEBUG THREAD %lu %ls CPU=%llu ms\n",entry.th32ThreadID,name?name:L"",
                    (((unsigned long long)user.dwHighDateTime<<32)|user.dwLowDateTime)/10000);
                if(name)LocalFree(name);Stack(process,thread);ResumeThread(thread);
            }CloseHandle(thread);
        }while(Thread32Next(snapshot,&entry));
        CloseHandle(snapshot);SymCleanup(process);CloseHandle(process);return 0;
    }
    if(argc!=3){fprintf(stderr,"usage: debug-launcher core.exe eboot.bin\n");return 2;}
    std::wstring command=L"\""+std::wstring(argv[1])+L"\" --game \""+argv[2]+L"\"";
    STARTUPINFOW start{};start.cb=sizeof(start);start.dwFlags=STARTF_USESTDHANDLES;
    start.hStdOutput=GetStdHandle(STD_OUTPUT_HANDLE);start.hStdError=GetStdHandle(STD_ERROR_HANDLE);start.hStdInput=GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION child{};
    if(!CreateProcessW(argv[1],command.data(),nullptr,nullptr,TRUE,DEBUG_ONLY_THIS_PROCESS|CREATE_NO_WINDOW,nullptr,nullptr,&start,&child))return int(GetLastError());
    SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME);SymInitialize(child.hProcess,nullptr,FALSE);
    bool initial_break=true;
    for(;;){DEBUG_EVENT event{};if(!WaitForDebugEvent(&event,1000))continue;DWORD status=DBG_CONTINUE;
        switch(event.dwDebugEventCode){
        case CREATE_PROCESS_DEBUG_EVENT:if(event.u.CreateProcessInfo.hFile)CloseHandle(event.u.CreateProcessInfo.hFile);break;
        case LOAD_DLL_DEBUG_EVENT:if(event.u.LoadDll.hFile)CloseHandle(event.u.LoadDll.hFile);break;
        case EXCEPTION_DEBUG_EVENT:{const auto& e=event.u.Exception;
            if(initial_break&&e.ExceptionRecord.ExceptionCode==EXCEPTION_BREAKPOINT){initial_break=false;break;}
            status=DBG_EXCEPTION_NOT_HANDLED;
            if(!e.dwFirstChance||e.ExceptionRecord.ExceptionCode==0xc0000409){
                SymRefreshModuleList(child.hProcess);
                fprintf(stderr,"DEBUG EXCEPTION code=%08lx chance=%lu address=%p thread=%lu param=%llx\n",
                    e.ExceptionRecord.ExceptionCode,e.dwFirstChance,e.ExceptionRecord.ExceptionAddress,event.dwThreadId,
                    (unsigned long long)e.ExceptionRecord.ExceptionInformation[0]);
                auto thread=OpenThread(THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,event.dwThreadId);
                if(thread){Stack(child.hProcess,thread);CloseHandle(thread);}
            }break;}
        case EXIT_PROCESS_DEBUG_EVENT:{const auto code=event.u.ExitProcess.dwExitCode;ContinueDebugEvent(event.dwProcessId,event.dwThreadId,status);
            SymCleanup(child.hProcess);CloseHandle(child.hThread);CloseHandle(child.hProcess);return int(code);}
        default:break;
        }
        ContinueDebugEvent(event.dwProcessId,event.dwThreadId,status);
    }
}
