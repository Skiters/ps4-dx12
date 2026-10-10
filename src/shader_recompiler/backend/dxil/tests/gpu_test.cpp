// SPDX-License-Identifier: GPL-2.0-or-later
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <string>
using Microsoft::WRL::ComPtr;
void Check(HRESULT hr) { if (FAILED(hr)) throw std::runtime_error("D3D12 HRESULT " + std::to_string(static_cast<unsigned>(hr))); }
std::vector<char> Read(const char* path) { std::ifstream f(path, std::ios::binary); return {std::istreambuf_iterator<char>(f), {}}; }
int main(int argc, char** argv) {
    try {
        auto vs = Read("vertex.dxil"), ps = Read("pixel.dxil");
        if (vs.empty() || ps.empty()) throw std::runtime_error("Generate fixture DXIL first");
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) debug->EnableDebugLayer();
        ComPtr<IDXGIFactory6> factory; Check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
        ComPtr<IDXGIAdapter1> adapter; ComPtr<ID3D12Device> device;
        for (UINT i = 0; factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc{}; Check(adapter->GetDesc1(&desc));
            if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) && SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)))) { std::wcout << L"Hardware: " << desc.Description << L"\n"; break; }
            adapter.Reset();
        }
        if (!device) throw std::runtime_error("No hardware D3D12 device");
        const bool barycentric = argc > 1 && std::string(argv[1]) == "barycentric";
        if (argc > 1) {
            D3D12_FEATURE_DATA_D3D12_OPTIONS3 features{};
            Check(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS3,&features,sizeof(features)));
            D3D12_FEATURE_DATA_SHADER_MODEL model{D3D_SHADER_MODEL_6_1};
            Check(device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,&model,sizeof(model)));
            if (!features.BarycentricsSupported || model.HighestShaderModel < D3D_SHADER_MODEL_6_1)
                throw std::runtime_error("Native barycentric smoke test requires hardware SM6.1+barycentrics");
        }
        D3D12_COMMAND_QUEUE_DESC qd{}; qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ComPtr<ID3D12CommandQueue> queue; Check(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue)));
        ComPtr<ID3D12CommandAllocator> allocator; Check(device->CreateCommandAllocator(qd.Type, IID_PPV_ARGS(&allocator)));
        ComPtr<ID3D12GraphicsCommandList> commands; Check(device->CreateCommandList(0, qd.Type, allocator.Get(), nullptr, IID_PPV_ARGS(&commands)));
        D3D12_ROOT_PARAMETER runtime{};
        runtime.ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        runtime.Constants={0,31,9}; runtime.ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
        D3D12_ROOT_SIGNATURE_DESC rs{}; rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        rs.NumParameters=1; rs.pParameters=&runtime;
        ComPtr<ID3DBlob> blob, errors; Check(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors));
        ComPtr<ID3D12RootSignature> root; Check(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root)));
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};
        pd.pRootSignature = root.Get(); pd.VS = {vs.data(), vs.size()}; pd.PS = {ps.data(), ps.size()};
        pd.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        pd.SampleMask = UINT_MAX; pd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; pd.RasterizerState.DepthClipEnable = TRUE;
        pd.DepthStencilState.DepthEnable = FALSE; pd.DepthStencilState.StencilEnable = FALSE;
        pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pd.NumRenderTargets = 1; pd.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; pd.SampleDesc.Count = 1;
        ComPtr<ID3D12PipelineState> pipeline;
        HRESULT pipeline_result = device->CreateGraphicsPipelineState(&pd, IID_PPV_ARGS(&pipeline));
        if (FAILED(pipeline_result)) {
            ComPtr<ID3D12InfoQueue> info;
            if (SUCCEEDED(device.As(&info))) {
                for (UINT64 i=0; i<info->GetNumStoredMessages(); ++i) {
                    SIZE_T size{}; info->GetMessage(i,nullptr,&size);
                    std::vector<char> storage(size);
                    auto message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
                    if (SUCCEEDED(info->GetMessage(i,message,&size))) std::cerr << message->pDescription << "\n";
                }
            }
        }
        Check(pipeline_result);
        D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC td{}; td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; td.Width = 4; td.Height = 4;
        td.DepthOrArraySize = 1; td.MipLevels = 1; td.Format = pd.RTVFormats[0]; td.SampleDesc.Count = 1; td.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        ComPtr<ID3D12Resource> texture; Check(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&texture)));
        D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors = 1;
        ComPtr<ID3D12DescriptorHeap> heap; Check(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap)));
        auto rtv = heap->GetCPUDescriptorHandleForHeapStart(); device->CreateRenderTargetView(texture.Get(), nullptr, rtv);
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{}; UINT rows{}; UINT64 row_size{}, total{};
        device->GetCopyableFootprints(&td, 0, 1, 0, &footprint, &rows, &row_size, &total);
        D3D12_RESOURCE_DESC bd{}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = total;
        bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        hp.Type = D3D12_HEAP_TYPE_READBACK; ComPtr<ID3D12Resource> readback;
        Check(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)));
        commands->SetGraphicsRootSignature(root.Get()); commands->SetPipelineState(pipeline.Get());
        const uint32_t runtime_data[9]{0,0,0,0,0,0x40800000,0x40800000,0,0};
        commands->SetGraphicsRoot32BitConstants(0,9,runtime_data,0);
        D3D12_VIEWPORT viewport{0,0,4,4,0,1}; D3D12_RECT scissor{0,0,4,4};
        commands->RSSetViewports(1, &viewport); commands->RSSetScissorRects(1, &scissor);
        commands->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        const float clear[4]{0,0,0,0}; commands->ClearRenderTargetView(rtv, clear, 0, nullptr);
        commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); commands->DrawInstanced(3,1,0,0);
        D3D12_RESOURCE_BARRIER barrier{}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition = {texture.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE};
        commands->ResourceBarrier(1, &barrier);
        D3D12_TEXTURE_COPY_LOCATION source{}; source.pResource = texture.Get(); source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION dest{}; dest.pResource = readback.Get(); dest.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; dest.PlacedFootprint = footprint;
        commands->CopyTextureRegion(&dest,0,0,0,&source,nullptr); Check(commands->Close());
        ID3D12CommandList* lists[]{commands.Get()}; queue->ExecuteCommandLists(1, lists);
        ComPtr<ID3D12Fence> fence; Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))); Check(queue->Signal(fence.Get(),1));
        HANDLE event = CreateEventW(nullptr,FALSE,FALSE,nullptr); if (!event) throw std::runtime_error("CreateEvent failed");
        Check(fence->SetEventOnCompletion(1,event)); DWORD wait = WaitForSingleObject(event,10000); CloseHandle(event);
        if (wait != WAIT_OBJECT_0) throw std::runtime_error("GPU did not complete within 10 seconds");
        const uint8_t* data{}; D3D12_RANGE range{0, static_cast<SIZE_T>(total)}; Check(readback->Map(0,&range,reinterpret_cast<void**>(const_cast<uint8_t**>(&data))));
        bool matched = true; const int expected[]{191,128,64,255};
        for (unsigned y=0;y<4;++y) for(unsigned x=0;x<4;++x) for(unsigned c=0;c<4;++c) {
            const int actual = data[footprint.Offset+y*footprint.Footprint.RowPitch+x*4+c];
            const float weights[]{1.0f-(float(x)+0.5f)/8.0f-(3.5f-float(y))/8.0f,
                                  (float(x)+0.5f)/8.0f,(3.5f-float(y))/8.0f,1.0f};
            const int target=barycentric ? int(weights[c]*255.0f+0.5f) : expected[c];
            if (abs(actual-target)>1) { matched=false; std::cerr << "Pixel " << x << "," << y << " component " << c << " actual=" << actual << " expected=" << target << "\n"; }
        }
        D3D12_RANGE no_write{}; readback->Unmap(0,&no_write);
        if (!matched) throw std::runtime_error("Draw/readback pixel mismatch");
        std::cout << "PASS: emitted vertex/pixel DXIL executed on native D3D12; 16 drawn pixels match expected RGBA; no Vulkan\n";
        return 0;
    } catch(const std::exception& error) { std::cerr << error.what() << "\n"; return 1; }
}
