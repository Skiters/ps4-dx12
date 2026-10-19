// SPDX-License-Identifier: GPL-2.0-or-later
#include "shader_recompiler/backend/dxil/dxil_compiler.h"
#include "shader_recompiler/backend/dxil/spirv_converter.h"
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using Microsoft::WRL::ComPtr;
using namespace Shader::Backend::DX12;

static void Check(HRESULT result) {
    if (FAILED(result)) throw std::runtime_error("D3D12 HRESULT " + std::to_string(uint32_t(result)));
}

static std::vector<uint32_t> ReadSpirv(const wchar_t* path) {
    std::ifstream file(std::filesystem::path{path}, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("Cannot read ROV SPIR-V fixture");
    const auto bytes = file.tellg();
    if (bytes <= 0 || bytes % 4) throw std::runtime_error("Malformed ROV SPIR-V fixture");
    std::vector<uint32_t> words(size_t(bytes) / 4);
    file.seekg(0); file.read(reinterpret_cast<char*>(words.data()), bytes);
    if (!file) throw std::runtime_error("Short ROV SPIR-V fixture");
    return words;
}

static ComPtr<ID3D12Device> HardwareDevice() {
    ComPtr<IDXGIFactory6> factory; Check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
    for (UINT index = 0;; ++index) {
        ComPtr<IDXGIAdapter1> adapter;
        const auto status = factory->EnumAdapterByGpuPreference(index,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));
        if (status == DXGI_ERROR_NOT_FOUND) break;
        Check(status); DXGI_ADAPTER_DESC1 desc{}; Check(adapter->GetDesc1(&desc));
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        ComPtr<ID3D12Device> device;
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
                                       IID_PPV_ARGS(&device)))) return device;
    }
    throw std::runtime_error("No hardware D3D12 device");
}

int wmain(int argc, wchar_t** argv) try {
    if (argc != 4) throw std::invalid_argument("usage: rov-gpu-test converter.dll dxcompiler.dll fixture.spv");
    SpirvConvertOptions options; options.stage = SpirvStage::Pixel;
    options.raster_ordered_uav_count = 1;
    const auto pixel = ConvertSpirv(argv[1], ReadSpirv(argv[3]), options);
    if (!pixel.success) throw std::runtime_error("ROV pixel conversion failed: " + pixel.diagnostics);
    const auto vertex = CompileDxil(argv[2], R"(
float4 main(uint id : SV_VertexID) : SV_Position {
    float2 p = id == 0 ? float2(-1.0, -1.0) : id == 1 ? float2(-1.0, 3.0) : float2(3.0, -1.0);
    return float4(p, 0.0, 1.0);
})", Stage::Vertex);
    if (!vertex.success) throw std::runtime_error("ROV vertex compilation failed: " + vertex.diagnostics);

    auto device = HardwareDevice();
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1,
                                         D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE};
    ComPtr<ID3D12DescriptorHeap> heap; Check(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&heap)));
    D3D12_HEAP_PROPERTIES properties{}; properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC texture_desc{}; texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture_desc.Width = 4; texture_desc.Height = 4; texture_desc.DepthOrArraySize = 1;
    texture_desc.MipLevels = 1; texture_desc.Format = DXGI_FORMAT_R32_UINT;
    texture_desc.SampleDesc.Count = 1; texture_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    ComPtr<ID3D12Resource> texture; Check(device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE,
        &texture_desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&texture)));
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{}; uav.Format = DXGI_FORMAT_R32_UINT;
    uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    const auto cpu = heap->GetCPUDescriptorHandleForHeapStart();
    const auto gpu = heap->GetGPUDescriptorHandleForHeapStart();
    device->CreateUnorderedAccessView(texture.Get(), nullptr, &uav, cpu);

    D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 0};
    D3D12_ROOT_PARAMETER parameter{}; parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameter.DescriptorTable = {1, &range}; parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC root_desc{1, &parameter, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
    ComPtr<ID3DBlob> root_blob, root_errors;
    Check(D3D12SerializeRootSignature(&root_desc, D3D_ROOT_SIGNATURE_VERSION_1, &root_blob, &root_errors));
    ComPtr<ID3D12RootSignature> root;
    Check(device->CreateRootSignature(0, root_blob->GetBufferPointer(), root_blob->GetBufferSize(),
                                      IID_PPV_ARGS(&root)));
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_desc{}; pipeline_desc.pRootSignature = root.Get();
    pipeline_desc.VS = {vertex.bytecode.data(), vertex.bytecode.size()};
    pipeline_desc.PS = {pixel.bytecode.data(), pixel.bytecode.size()};
    pipeline_desc.SampleMask = UINT_MAX; pipeline_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipeline_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipeline_desc.RasterizerState.DepthClipEnable = TRUE;
    pipeline_desc.DepthStencilState.DepthEnable = FALSE;
    pipeline_desc.DepthStencilState.StencilEnable = FALSE;
    pipeline_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeline_desc.SampleDesc.Count = 1;
    ComPtr<ID3D12PipelineState> pipeline;
    Check(device->CreateGraphicsPipelineState(&pipeline_desc, IID_PPV_ARGS(&pipeline)));

    D3D12_COMMAND_QUEUE_DESC queue_desc{}; queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue; Check(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue)));
    ComPtr<ID3D12CommandAllocator> allocator; Check(device->CreateCommandAllocator(queue_desc.Type, IID_PPV_ARGS(&allocator)));
    ComPtr<ID3D12GraphicsCommandList> commands;
    Check(device->CreateCommandList(0, queue_desc.Type, allocator.Get(), pipeline.Get(), IID_PPV_ARGS(&commands)));
    ID3D12DescriptorHeap* heaps[]{heap.Get()}; commands->SetDescriptorHeaps(1, heaps);
    const UINT zeros[4]{}; commands->ClearUnorderedAccessViewUint(gpu, cpu, texture.Get(), zeros, 0, nullptr);
    D3D12_RESOURCE_BARRIER barrier{}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = texture.Get(); commands->ResourceBarrier(1, &barrier);
    commands->SetGraphicsRootSignature(root.Get()); commands->SetGraphicsRootDescriptorTable(0, gpu);
    D3D12_VIEWPORT viewport{0, 0, 4, 4, 0, 1}; const D3D12_RECT scissor{0, 0, 4, 4};
    commands->RSSetViewports(1, &viewport); commands->RSSetScissorRects(1, &scissor);
    commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    constexpr UINT invocations = 64; commands->DrawInstanced(3, invocations, 0, 0);
    commands->ResourceBarrier(1, &barrier);

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{}; UINT rows{}; UINT64 row_bytes{}, total{};
    device->GetCopyableFootprints(&texture_desc, 0, 1, 0, &footprint, &rows, &row_bytes, &total);
    D3D12_RESOURCE_DESC buffer_desc{}; buffer_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer_desc.Width = total; buffer_desc.Height = 1; buffer_desc.DepthOrArraySize = 1;
    buffer_desc.MipLevels = 1; buffer_desc.SampleDesc.Count = 1; buffer_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    properties.Type = D3D12_HEAP_TYPE_READBACK; ComPtr<ID3D12Resource> readback;
    Check(device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &buffer_desc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)));
    barrier = {}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition = {texture.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                          D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE};
    commands->ResourceBarrier(1, &barrier);
    D3D12_TEXTURE_COPY_LOCATION source{texture.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
    D3D12_TEXTURE_COPY_LOCATION destination{}; destination.pResource = readback.Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; destination.PlacedFootprint = footprint;
    commands->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr); Check(commands->Close());
    ID3D12CommandList* lists[]{commands.Get()}; queue->ExecuteCommandLists(1, lists);
    ComPtr<ID3D12Fence> fence; Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
    Check(queue->Signal(fence.Get(), 1)); const HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event) throw std::runtime_error("CreateEvent failed");
    Check(fence->SetEventOnCompletion(1, event)); const auto wait = WaitForSingleObject(event, 10000); CloseHandle(event);
    if (wait != WAIT_OBJECT_0) throw std::runtime_error("ROV GPU work timed out");
    const uint8_t* mapped{}; const D3D12_RANGE read_range{0, SIZE_T(total)};
    Check(readback->Map(0, &read_range, reinterpret_cast<void**>(const_cast<uint8_t**>(&mapped))));
    for (UINT y = 0; y < 4; ++y) for (UINT x = 0; x < 4; ++x) {
        const auto value = *reinterpret_cast<const uint32_t*>(mapped + footprint.Offset +
            y * footprint.Footprint.RowPitch + x * sizeof(uint32_t));
        if (value != invocations) throw std::runtime_error("ROV ordered increment mismatch at " +
            std::to_string(x) + "," + std::to_string(y) + ": " + std::to_string(value));
    }
    const D3D12_RANGE no_write{}; readback->Unmap(0, &no_write);
    std::cout << "PASS: 64 overlapping pixel invocations produced ordered value 64 in all 16 UAV texels\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n'; return 1;
}
