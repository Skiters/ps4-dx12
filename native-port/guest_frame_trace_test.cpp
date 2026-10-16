// SPDX-License-Identifier: GPL-2.0-or-later
#include "common/guest_frame_trace.h"
#include "video_core/renderer_d3d12/resources.h"
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <algorithm>
#include <fstream>
#include <sstream>

using namespace NativeDX12;
namespace Trace=Common::GuestFrameTrace;
struct Row{std::string phase;u64 observed{},completed{};bool valid{};};
static std::vector<Row> Rows(const std::filesystem::path& path){
    std::ifstream input(path);std::string line;std::getline(input,line);std::vector<Row> rows;
    while(std::getline(input,line)){std::istringstream stream(line);std::array<std::string,8> values;
        for(auto& value:values)if(!std::getline(stream,value,','))throw std::runtime_error("Incomplete capture snapshot");
        rows.push_back({values[1],std::stoull(values[4]),std::stoull(values[5]),values[7]=="true"});}
    return rows;
}
template<class Predicate>static void Wait(const std::filesystem::path& file,Predicate predicate){
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(std::chrono::steady_clock::now()<end){if(predicate(Rows(file)))return;std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    throw std::runtime_error("Capture did not publish the expected snapshot");
}
static ComPtr<ID3D12Device> HardwareDevice(){
    ComPtr<ID3D12Debug> debug;Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> adapter;const auto hr=factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter));
        if(hr==DXGI_ERROR_NOT_FOUND)break;Check(hr);DXGI_ADAPTER_DESC1 desc{};Check(adapter->GetDesc1(&desc));
        if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;ComPtr<ID3D12Device> device;
        if(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))return device;}
    throw std::runtime_error("No hardware D3D12 device");
}
int main(int argc,char** argv){try{
    if(argc!=4)throw std::invalid_argument("Expected fixture wrapper arguments");
    const auto root=std::filesystem::current_path()/("frame-capture-fixture-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    if(!std::filesystem::create_directory(root))throw std::runtime_error("Capture fixture directory exists");
    const auto marker=root/"capture.enabled",file=root/"actual-fences.csv";_putenv_s("SHADPS4_FRAME_CAPTURE",marker.string().c_str());
    auto device=HardwareDevice();ComPtr<ID3D12InfoQueue> messages;Check(device.As(&messages));
    ComPtr<ID3D12CommandQueue> queue;D3D12_COMMAND_QUEUE_DESC description{};Check(device->CreateCommandQueue(&description,IID_PPV_ARGS(&queue)));
    ComPtr<ID3D12Fence> ready,gate;Check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&ready)));
    Check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&gate)));
    const auto completion=[&](u64 value){return [fence=ready,value]{const auto done=fence->GetCompletedValue();
        return done==UINT64_MAX?Trace::Completion::Failed:done>=value?Trace::Completion::Complete:Trace::Completion::Pending;};};
    {
        Trace::Capture capture(file.string().c_str());Check(queue->Wait(gate.Get(),1));Check(queue->Signal(ready.Get(),10));
        capture.Await(capture.New(),10,completion(10));std::ofstream(marker).put('1');
        Wait(file,[](const auto& rows){return std::ranges::any_of(rows,[](const auto& row){return row.phase=="capture-start"&&row.observed==1&&row.completed==0&&row.valid;});});
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if(ready->GetCompletedValue()!=0||std::ranges::any_of(Rows(file),[](const auto& row){return row.phase=="gpu-complete";}))
            throw std::runtime_error("Uncompleted submitted fence was counted");
        Check(gate->Signal(1));Wait(file,[](const auto& rows){return std::ranges::any_of(rows,[](const auto& row){return row.phase=="gpu-complete"&&row.completed==1&&row.valid;});});
        std::filesystem::remove(marker);Wait(file,[](const auto& rows){return std::ranges::any_of(rows,[](const auto& row){return row.phase=="capture-stop"&&row.completed==1&&row.valid;});});
        capture.Await(capture.New(),20,completion(20));Check(queue->Signal(ready.Get(),20));
        const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        while(ready->GetCompletedValue()<20&&std::chrono::steady_clock::now()<limit)std::this_thread::sleep_for(std::chrono::milliseconds(2));
        if(ready->GetCompletedValue()<20)throw std::runtime_error("Actual fence did not finish outside capture");
        std::this_thread::sleep_for(std::chrono::milliseconds(20));std::ofstream(marker).put('1');
        Wait(file,[](const auto& rows){return std::ranges::any_of(rows,[](const auto& row){return row.phase=="capture-start"&&row.completed==2&&row.valid;});});
        Check(queue->Wait(gate.Get(),2));Check(queue->Signal(ready.Get(),30));capture.Await(capture.New(),30,completion(30));Check(gate->Signal(2));
        Wait(file,[](const auto& rows){return std::ranges::any_of(rows,[](const auto& row){return row.phase=="gpu-complete"&&row.completed==3&&row.valid;});});
        std::filesystem::remove(marker);Wait(file,[](const auto& rows){return std::ranges::any_of(rows,[](const auto& row){return row.phase=="capture-stop"&&row.completed==3&&row.valid;});});
    }
    std::vector<Row> starts,stops;for(const auto& row:Rows(file)){
        if(!row.valid)throw std::runtime_error("Successful actual-fence capture was invalidated");
        if(row.phase=="capture-start")starts.push_back(row);if(row.phase=="capture-stop")stops.push_back(row);}
    if(starts.size()!=2||stops.size()!=2||stops[0].completed-starts[0].completed!=1||stops[1].completed-starts[1].completed!=1)
        throw std::runtime_error("Interval snapshots lost pre-armed completion or counted past completed frames");
    const auto duplicate=root/"duplicate.csv";{
        Trace::Capture capture(duplicate.string().c_str());const auto id=capture.New();capture.Await(id,30,completion(30));capture.Await(id,30,completion(30));
        Wait(duplicate,[](const auto& rows){return std::ranges::any_of(rows,[](const auto& row){return row.phase=="duplicate-new-flip"&&!row.valid;});});}
    const auto failure=root/"failure.csv";{
        Trace::Capture capture(failure.string().c_str());capture.Await(capture.New(),1,[]{return Trace::Completion::Failed;});
        Wait(failure,[](const auto& rows){return std::ranges::any_of(rows,[](const auto& row){return row.phase=="gpu-completion-error"&&!row.valid;});});}
    for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T size{};Check(messages->GetMessage(i,nullptr,&size));std::vector<std::byte> bytes(size);
        auto* message=reinterpret_cast<D3D12_MESSAGE*>(bytes.data());Check(messages->GetMessage(i,message,&size));
        if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);}
    puts("PASS: actual GPU completion fences, pre-armed pending frames, exact interval count deltas, duplicate rejection and completion error invalidation");return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
