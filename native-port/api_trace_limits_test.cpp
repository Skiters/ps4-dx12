// SPDX-License-Identifier: GPL-2.0-or-later
#include "common/api_trace.h"
#include <filesystem>
#include <string_view>

int main(int argc,char** argv){
    const std::string_view mode=argc>1?argv[1]:"limit";
    const std::string prefix="bounded-trace-"+std::string(mode)+".jsonl";
    for(const auto& entry:std::filesystem::directory_iterator("."))
        if(entry.path().filename().string().starts_with(prefix))std::filesystem::remove(entry.path());
    _putenv_s("SHADPS4_API_TRACE",mode=="open-failure"?"missing-trace-directory/output.jsonl":prefix.c_str());
    _putenv_s("SHADPS4_API_TRACE_MAX_BYTES","512");
    auto& session=ApiTrace::GetSession();
    if(mode=="write-failure"){
        session.root->file=std::freopen(prefix.c_str(),"rb",session.root->file);
        if(!session.root->file)return 1;
    }
    std::atomic<unsigned> executed{};
    std::vector<std::jthread> workers;
    // Exercise the failed stream on its original thread before workers bypass it.
    if(mode=="write-failure")ApiTrace::Call("PS4","failure",[]{return 7;});
    for(unsigned worker=0;worker<4;++worker)workers.emplace_back([&]{
        for(unsigned call=0;call<1000;++call)
            if(ApiTrace::Call("DX12","bounded-test",[&]{++executed;return 42;})!=42)std::terminate();
    });
    workers.clear();ApiTrace::FlushAll();
    if(executed!=4000 || ApiTrace::Enabled())return 2;
    uintmax_t bytes{};
    for(const auto& entry:std::filesystem::directory_iterator("."))
        if(entry.path().filename().string().starts_with(prefix))bytes+=entry.file_size();
    if(bytes>512)return 3;
    std::printf("PASS: %s; all 4000 calls executed, trace bytes=%llu\n",argv[1],static_cast<unsigned long long>(bytes));
}
