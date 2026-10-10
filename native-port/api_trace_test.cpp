// SPDX-License-Identifier: GPL-2.0-or-later
#include "core/tls.h"
#include <fstream>
#include <filesystem>
#include <set>
#include <thread>
#include <vector>
#include <stdexcept>
static int PS4_SYSV_ABI Sum(int a,int b){return a+b;}
static void PS4_SYSV_ABI VoidCall(int* value){++*value;}
static double PS4_SYSV_ABI FloatCall(double a){return a*2;}
struct Pair{unsigned long long a,b;};
static Pair PS4_SYSV_ABI PairCall(Pair a){return {a.b,a.a};}
static int PS4_SYSV_ABI ZeroArgs(){return 9;}
// Reproduce the guest signal thunk: normal entry RSP%16=8, deliberately
// preserve that remainder at the CALL so the API wrapper enters with RSP%16=0.
__attribute__((naked)) static int PS4_SYSV_ABI MisalignedInvoke(int (PS4_SYSV_ABI *fn)()) {
    __asm__("subq $32, %rsp\n\tcallq *%rdi\n\taddq $32, %rsp\n\tretq");
}
int main(int argc,char** argv){
    for(const auto& entry:std::filesystem::directory_iterator("."))
        if(entry.path().filename().string().starts_with("api-trace-test.jsonl"))std::filesystem::remove(entry.path());
    _putenv_s("SHADPS4_API_TRACE","api-trace-test.jsonl");
    _putenv_s("SHADPS4_API_TRACE_SYNC",argc>1&&std::string_view(argv[1])=="immediate"?"1":"0");
    auto sum=Core::TracedHostCall<Sum>("test:Sum");
    auto nothing=Core::TracedHostCall<VoidCall>("test:VoidCall");
    auto floating=Core::TracedHostCall<FloatCall>("test:FloatCall");
    auto pair=Core::TracedHostCall<PairCall>("test:PairCall");
    if(sum(17,23)!=40)return 1;
    int value{};nothing(&value);if(value!=1||floating(1.25)!=2.5)return 2;
    auto swapped=pair({17,23});if(swapped.a!=23||swapped.b!=17)return 3;
    if(MisalignedInvoke(Core::TracedHostCall<ZeroArgs>("test:MisalignedGuestSignal"))!=9)return 6;
    try{ApiTrace::Call("DX12","exception-test",[]{throw std::runtime_error("test");});}catch(const std::runtime_error&){}
    std::vector<std::jthread> threads;
    for(unsigned worker=0;worker<4;++worker)threads.emplace_back([=]{for(unsigned call=0;call<1024;++call)if(sum(worker,call)!=int(worker+call))std::terminate();});
    threads.clear();
    bool visible=false;
    for(unsigned attempt=0;attempt<20&&!visible;++attempt){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::ifstream live("api-trace-test.jsonl",std::ios::binary|std::ios::ate);visible=live&&live.tellg()>0;
    }
    if(!visible)return 7;
    ApiTrace::FlushAll();
    unsigned enters{},returns{},exceptions{};
    std::set<unsigned long long> ids;
    for(const auto& entry:std::filesystem::directory_iterator(".")) {
        if(!entry.path().filename().string().starts_with("api-trace-test.jsonl"))continue;
        std::ifstream input(entry.path());std::string line;
        while(std::getline(input,line)){
            const bool enter=line.find("\"phase\":\"enter\"")!=std::string::npos;
            enters+=enter;
            returns+=line.find("\"phase\":\"return\"")!=std::string::npos;
            exceptions+=line.find("\"phase\":\"exception\"")!=std::string::npos;
            if(enter && !ids.insert(std::stoull(line.substr(6))).second)return 5;
        }
    }
    if(enters!=4107||returns!=4106||exceptions!=1)return 4;
    std::puts("PASS: SysV arguments/void/scalar/float/aggregate returns and every concurrent API call recorded");
}
