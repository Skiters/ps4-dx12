#include "common/api_trace.h"
#include "compute_pipeline.h"
#include "resource_bindings.h"
#include "shader_pipeline.h"
#include "shader_recompiler/backend/dxil/spirv_converter.h"
#include <dxgi1_6.h>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <d3d12sdklayers.h>
using namespace NativeDX12;
void Require(bool ok){if(!ok)throw std::runtime_error("compute assertion failed");}
int wmain(int argc,wchar_t** argv){try{
    const bool capture=argc==5&&std::wstring_view(argv[4])==L"capture";
    if(argc!=4&&!capture)throw std::invalid_argument("usage: binding-test fixture.spv converter.dll dxcompiler.dll [capture]");
    if(capture){
        Require(_putenv_s("SHADPS4_NATIVE_DISPLAY_CAPTURE","binding-fixture")==0);
        ComPtr<ID3D12Debug> debug;Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    }
    std::ifstream f(argv[1],std::ios::binary|std::ios::ate);Require(bool(f));auto size=f.tellg();Require(size>0&&size%4==0);
    std::vector<uint32_t> spirv(size_t(size)/4);f.seekg(0);f.read(reinterpret_cast<char*>(spirv.data()),size);Require(bool(f));
    Shader::Backend::DX12::SpirvConvertOptions options;options.stage=Shader::Backend::DX12::SpirvStage::Compute;
    auto converted=Shader::Backend::DX12::ConvertSpirv(argv[2],spirv,options);
    if(!converted.success||!converted.validated)throw std::runtime_error(converted.diagnostics);
    printf("requires_runtime_data=%d, needs_draw_sysvals=%d\n",converted.requires_runtime_data,converted.needs_draw_sysvals);printf("Mesa converted and validated/signed compute DXIL: %zu bytes\n",converted.bytecode.size());
    ComPtr<IDXGIFactory6> factory;Check(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))));ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1>a;if(DX12_API_CALL(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&a)))==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 d{};DX12_API_CALL(a->GetDesc1(&d));if(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;if(SUCCEEDED(DX12_API_CALL(D3D12CreateDevice(a.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))){wprintf(L"Hardware: %ls\n",d.Description);break;}}
    Require(device!=nullptr);
    DescriptorHeapPool pool(device.Get());
    auto heap0=pool.Acquire(3,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto heap1=pool.Acquire(3,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    Require(heap0.page==heap1.page&&heap0.offset==0&&heap1.offset==3);
    auto* retired_heap=heap0.page->heap.Get();pool.Release(heap0);
    auto reused=pool.Acquire(4,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    Require(reused.page->heap.Get()==retired_heap&&reused.offset==6);pool.Release(reused);
    pool.Release(heap1);
    auto reset=pool.Acquire(4,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    Require(reset.page->heap.Get()==retired_heap&&reset.offset==0);pool.Release(reset);
    // A larger table must not underflow capacity-count and reuse a small page.
    auto large=pool.Acquire(131100,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    Require(large.page->heap.Get()!=retired_heap&&large.page->capacity>=large.count&&large.offset==0);
    pool.Release(large);
    ShaderPipeline pipeline(device.Get(),argv[3],converted.bytecode);
    const auto& layout=pipeline.Layout();
    ResourceBindings bindings(device.Get(),layout,&pool);
    ComPtr<ID3D12CommandQueue> queue;D3D12_COMMAND_QUEUE_DESC q{};Check(DX12_API_CALL(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))));
    ComPtr<ID3D12CommandAllocator> alloc;Check(DX12_API_CALL(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&alloc))));
    ComPtr<ID3D12GraphicsCommandList> list;Check(DX12_API_CALL(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,alloc.Get(),nullptr,IID_PPV_ARGS(&list))));
    constexpr UINT count=1024;auto output=UavBuffer(device.Get(),count*4);auto readback=Buffer(device.Get(),count*4,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    auto runtime=Buffer(device.Get(),256,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
    void* mapped=nullptr;D3D12_RANGE empty{};Check(DX12_API_CALL(runtime.handle->Map(0,&empty,&mapped)));
    memset(mapped,0,256);static_cast<UINT*>(mapped)[0]=count/64;static_cast<UINT*>(mapped)[1]=1;static_cast<UINT*>(mapped)[2]=1;
    DX12_API_CALL(runtime.handle->Unmap(0,nullptr));
    const auto fill_descriptors=[&](ResourceBindings& table){for(const auto& b:layout.Bindings()) {
        printf("binding type=%u reg=%u space=%u count=%u\n",unsigned(b.type),b.reg,b.space,b.count);
        if(b.type==D3D12_DESCRIPTOR_RANGE_TYPE_UAV&&b.reg==0&&b.space==0&&b.count==1) {
            D3D12_UNORDERED_ACCESS_VIEW_DESC view{};view.Format=DXGI_FORMAT_R32_TYPELESS;
            view.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;view.Buffer.NumElements=count;view.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
            table.Uav(0,0,output.handle.Get(),view);
        } else if(b.type==D3D12_DESCRIPTOR_RANGE_TYPE_CBV&&b.reg==0&&b.space==31&&b.count==1) {
            table.Cbv(0,31,runtime.handle.Get(),0,256);
        } else throw std::runtime_error("Unexpected compute fixture binding");
    }};
    fill_descriptors(bindings);
    unsigned preparations{};
    bindings.BeforeBind([&](ResourceBindings& table){
        ++preparations;bool rejected=false;try{table.Prepare();}catch(const std::logic_error&){rejected=true;}
        Require(rejected);
    });
    bindings.Prepare();bindings.Prepare();Require(preparations==1);
    output.Transition(list.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    pipeline.Bind(list.Get(),bindings);DX12_API_CALL(list->Dispatch(count/64,1,1));
    std::vector<ResourceBindings::BufferCapture> captures,offset_captures;
    std::unique_ptr<ResourceBindings> capture_table;
    if(capture){
        Require(bindings.CaptureBufferInputs(list.Get(),64,0,D3D12_RESOURCE_STATE_UNORDERED_ACCESS).empty());
        captures=bindings.CaptureBufferInputs(list.Get(),64,128,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Require(captures.size()==2);
        capture_table=std::make_unique<ResourceBindings>(device.Get(),layout,&pool);fill_descriptors(*capture_table);
        D3D12_UNORDERED_ACCESS_VIEW_DESC sub{};sub.Format=DXGI_FORMAT_R32_TYPELESS;
        sub.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;sub.Buffer.FirstElement=16;
        sub.Buffer.NumElements=32;sub.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
        capture_table->Uav(0,0,output.handle.Get(),sub);
        offset_captures=capture_table->CaptureBufferInputs(list.Get(),128,256,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Require(offset_captures.size()==2);
        capture_table->Uav(0,0,nullptr,sub);
        const auto null_captures=capture_table->CaptureBufferInputs(list.Get(),4,4,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Require(null_captures.size()==1&&null_captures[0].type==D3D12_DESCRIPTOR_RANGE_TYPE_CBV);
        bool rejected=false;try{(void)capture_table->CaptureBufferInputs(nullptr,4,4,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);}
        catch(const std::invalid_argument&){rejected=true;}Require(rejected);
    }
    output.UavBarrier(list.Get());output.Transition(list.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE);
    DX12_API_CALL(list->CopyBufferRegion(readback.handle.Get(),0,output.handle.Get(),0,count*4));Check(DX12_API_CALL(list->Close()));Timeline timeline(device.Get());timeline.Wait(timeline.Submit(queue.Get(),list.Get()));
    void* data=nullptr;D3D12_RANGE range{0,count*4};Check(DX12_API_CALL(readback.handle->Map(0,&range,&data)));auto words=static_cast<const uint32_t*>(data);
    for(UINT i=0;i<count;++i)if(words[i]!=i*3+7)throw std::runtime_error("GPU result differs at index "+std::to_string(i)+": "+std::to_string(words[i]));
    D3D12_RANGE no_write{0,0};DX12_API_CALL(readback.handle->Unmap(0,&no_write));
    if(capture){
        const auto verify_capture=[&](auto& buffers){for(auto& buffer:buffers){
            Require(buffer.bytes<=128&&buffer.bytes<=buffer.view_bytes);
            void* data{};D3D12_RANGE used{0,SIZE_T(buffer.bytes)};Check(buffer.readback.handle->Map(0,&used,&data));
            bool equal=true;for(UINT i=0;i<buffer.bytes/4;++i){
                const UINT expected=buffer.type==D3D12_DESCRIPTOR_RANGE_TYPE_UAV?UINT(buffer.offset/4+i)*3+7:
                    i==0?count/64:i<=2?1:0;
                equal&=static_cast<const UINT*>(data)[i]==expected;
            }
            buffer.readback.handle->Unmap(0,&no_write);Require(equal);
        }};
        verify_capture(captures);verify_capture(offset_captures);
        capture_table->RecycleRetiredHeaps();
        ComPtr<ID3D12InfoQueue> messages;Check(device.As(&messages));
        for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T bytes{};Check(messages->GetMessage(i,nullptr,&bytes));
            std::vector<std::byte> storage(bytes);auto* message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());
            Check(messages->GetMessage(i,message,&bytes));if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)
                throw std::runtime_error(message->pDescription);
        }
    }
    bindings.RecycleRetiredHeaps();
    // A realistic PS4 packet retires far more than 32 tables. Reuse every
    // matching heap after retirement instead of recreating driver objects.
    std::vector<std::unique_ptr<ResourceBindings>> batch;
    for(unsigned i=0;i<512;++i)batch.push_back(std::make_unique<ResourceBindings>(device.Get(),layout,&pool));
    for(auto& entry:batch)entry->RecycleRetiredHeaps();
    const auto before=pool.Stats();Require(before.available<=2&&before.created<=2&&before.discarded==0);
    batch.clear();
    for(unsigned i=0;i<512;++i)batch.push_back(std::make_unique<ResourceBindings>(device.Get(),layout,&pool));
    const auto after=pool.Stats();Require(after.created==before.created&&after.reused>=before.reused+512);
    bool rejected=false;try{bindings.Bind(list.Get(),true);}catch(const std::logic_error&){rejected=true;}
    Require(rejected);
    ResourceBindings failed(device.Get(),layout,&pool);
    failed.BeforeBind([&](ResourceBindings& table){fill_descriptors(table);throw std::runtime_error("Injected preparation failure");});
    rejected=false;try{failed.Prepare();}catch(const std::runtime_error&){rejected=true;}Require(rejected);
    rejected=false;try{failed.Bind(list.Get(),true);}catch(const std::logic_error&){rejected=true;}Require(rejected);
    failed.RecycleRetiredHeaps();
    puts("PASS: GLSL -> SPIR-V -> Mesa signed DXIL -> native D3D12 compute PSO/reflected descriptor tables -> Dispatch -> 1024 exact readback values");return 0;
}catch(const std::exception&e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}

