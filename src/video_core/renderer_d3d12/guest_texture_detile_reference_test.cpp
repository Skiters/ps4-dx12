#include "common/api_trace.h"
#include "guest_texture_cache.h"
#include "compute_pipeline.h"
#include "shader_recompiler/backend/dxil/spirv_converter.h"
#include <dxgi1_6.h>
#include <fstream>
#include <cstdio>
#include <bit>
using namespace NativeDX12;
void Require(bool ok){if(!ok)throw std::runtime_error("reference detiler assertion failed");}
int wmain(int argc,wchar_t** argv){try{
    if(argc<3||argc>6)throw std::invalid_argument("usage: guest-detile-reference original-tiling.spv converter.dll");
    std::ifstream f(argv[1],std::ios::binary|std::ios::ate);Require(bool(f));auto size=f.tellg();Require(size>0&&size%4==0);std::vector<uint32_t> spirv(size_t(size)/4);f.seekg(0);f.read(reinterpret_cast<char*>(spirv.data()),size);Require(bool(f));
    Shader::Backend::DX12::SpirvConvertOptions options;options.stage=Shader::Backend::DX12::SpirvStage::Compute;
    uint32_t constants[12]={1,1,1,0,4,12,1,1,16,4,512,2};
    const uint32_t tile=argc>=4?uint32_t(std::stoul(argv[3])):14;
    const uint32_t samples=argc>=5?uint32_t(std::stoul(argv[4])):1;
    const uint32_t bpp=argc==6?uint32_t(std::stoul(argv[5])):4;
    Require(bpp==4||bpp==8||bpp==16);
    constants[10]=std::min(1024u,std::max(256u,128*bpp));
    Require(std::has_single_bit(samples)&&samples<=16);
    constants[0]=samples;
    if(tile==10)constants[1]=0;
    else if(tile<=5){constants[1]=2;constants[10]=64u<<std::min(tile,4u);}
    else if(tile!=13&&tile!=14)throw std::invalid_argument("Unsupported reference tiling test");
    const auto mode=std::bit_width(std::min(constants[10],64*bpp*samples)/64)-1;
    constexpr uint32_t bh[]{4,2,1,1,1},aspect[]{4,2,2,2,1},banks[]{16,16,16,16,8};
    constants[7]=bh[mode];constants[8]=banks[mode];constants[9]=std::countr_zero(banks[mode]);constants[11]=aspect[mode];
    for(UINT i=0;i<12;++i)options.specializations.push_back({i,constants[i],true});
    auto dxil=Shader::Backend::DX12::ConvertSpirv(argv[2],spirv,options);if(!dxil.success||!dxil.validated)throw std::runtime_error(dxil.diagnostics);
    printf("Original shadPS4 tiling.comp -> signed DXIL: %zu bytes\n",dxil.bytecode.size());
    ComPtr<IDXGIFactory6> factory;Check(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))));ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1>a;if(DX12_API_CALL(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&a)))==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 info{};DX12_API_CALL(a->GetDesc1(&info));if(info.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;if(SUCCEEDED(DX12_API_CALL(D3D12CreateDevice(a.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))){wprintf(L"Hardware: %ls\n",info.Description);break;}}Require(device!=nullptr);
    D3D12_ROOT_PARAMETER params[4]{};for(UINT i=0;i<2;++i){params[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;params[i].Descriptor={i,0};}
    params[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;params[2].Descriptor={2,0};params[3].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[3].Constants={0,31,8};
    D3D12_ROOT_SIGNATURE_DESC rs{4,params,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};ComPtr<ID3DBlob> signature,errors;Check(DX12_API_CALL(D3D12SerializeRootSignature(&rs,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&errors)));ComPtr<ID3D12RootSignature> root;Check(DX12_API_CALL(device->CreateRootSignature(0,DX12_API_CALL(signature->GetBufferPointer()),DX12_API_CALL(signature->GetBufferSize()),IID_PPV_ARGS(&root))));
    D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.CS={dxil.bytecode.data(),dxil.bytecode.size()};ComPtr<ID3D12PipelineState> pipeline;Check(DX12_API_CALL(device->CreateComputePipelineState(&pd,IID_PPV_ARGS(&pipeline))));
    GuestTextureDesc desc;desc.width=137;desc.height=73;desc.pitch=144;desc.levels=samples>1?1:4;desc.samples=samples;desc.layers=2;desc.view_layers=2;desc.tile_mode=tile;if(bpp==8){desc.data_format=12;desc.number_format=7;}else if(bpp==16){desc.data_format=14;desc.number_format=7;}auto layout=LayoutGuestTexture(desc);
    Require(layout.mips[0].macro==(tile!=5&&tile!=13));if(samples==1&&tile!=5&&tile!=13)Require(!layout.mips[1].macro);auto input=UavBuffer(device.Get(),layout.guest_bytes),output=UavBuffer(device.Get(),layout.guest_bytes);auto upload=Buffer(device.Get(),layout.guest_bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);auto readback=Buffer(device.Get(),layout.guest_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    void* data=nullptr;D3D12_RANGE empty{0,0};Check(DX12_API_CALL(upload.handle->Map(0,&empty,&data)));for(UINT64 i=0;i<layout.guest_bytes/4;++i)static_cast<uint32_t*>(data)[i]=uint32_t(i)^0xabcde123u;DX12_API_CALL(upload.handle->Unmap(0,nullptr));
    auto info=Buffer(device.Get(),512,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);Check(DX12_API_CALL(info.handle->Map(0,&empty,&data)));memset(data,0,512);auto words=static_cast<uint32_t*>(data);words[2]=desc.levels;
    for(UINT m=0;m<desc.levels;++m){const auto& ml=layout.mips[m];if(!ml.macro)words[1]|=1u<<m;words[3+m*4]=uint32_t(ml.slice_bytes*desc.layers);words[4+m*4]=ml.pitch;words[5+m*4]=ml.height;words[6+m*4]=uint32_t(ml.offset);}DX12_API_CALL(info.handle->Unmap(0,nullptr));
    ComPtr<ID3D12CommandQueue> queue;D3D12_COMMAND_QUEUE_DESC q{};Check(DX12_API_CALL(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))));ComPtr<ID3D12CommandAllocator> allocator;Check(DX12_API_CALL(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))));ComPtr<ID3D12GraphicsCommandList> list;Check(DX12_API_CALL(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list))));
    input.Transition(list.Get(),D3D12_RESOURCE_STATE_COPY_DEST);DX12_API_CALL(list->CopyBufferRegion(input.handle.Get(),0,upload.handle.Get(),0,layout.guest_bytes));input.Transition(list.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    DX12_API_CALL(list->SetComputeRootSignature(root.Get()));DX12_API_CALL(list->SetPipelineState(pipeline.Get()));DX12_API_CALL(list->SetComputeRootUnorderedAccessView(0,DX12_API_CALL(input.handle->GetGPUVirtualAddress())));DX12_API_CALL(list->SetComputeRootUnorderedAccessView(1,DX12_API_CALL(output.handle->GetGPUVirtualAddress())));DX12_API_CALL(list->SetComputeRootConstantBufferView(2,DX12_API_CALL(info.handle->GetGPUVirtualAddress())));
    const UINT groups=UINT(layout.guest_bytes/bpp/64),runtime[8]={groups,1,1,0,0,0,0,0};DX12_API_CALL(list->SetComputeRoot32BitConstants(3,8,runtime,0));DX12_API_CALL(list->Dispatch(groups,1,1));output.UavBarrier(list.Get());output.Transition(list.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE);DX12_API_CALL(list->CopyBufferRegion(readback.handle.Get(),0,output.handle.Get(),0,layout.guest_bytes));Check(DX12_API_CALL(list->Close()));Timeline timeline(device.Get());timeline.Wait(timeline.Submit(queue.Get(),list.Get()));
    Check(DX12_API_CALL(readback.handle->Map(0,nullptr,&data)));auto result=static_cast<const uint32_t*>(data);UINT64 checked=0;
    for(UINT m=0;m<desc.levels;++m)for(UINT l=0;l<desc.layers;++l)for(UINT sample=0;sample<samples;++sample)for(UINT y=0;y<std::max(desc.height>>m,1u);++y)for(UINT x=0;x<std::max(desc.width>>m,1u);++x)for(UINT c=0;c<bpp/4;++c){const auto& ml=layout.mips[m];auto linear=(ml.offset+l*ml.slice_bytes)/4+((UINT64(sample)*ml.height+y)*ml.pitch+x)*(bpp/4)+c;auto at=GuestPixelOffset(desc,layout,m,l,x,y,sample)/4+c;auto expected=uint32_t(at)^0xabcde123u;if(result[linear]!=expected)throw std::runtime_error("CPU/native-original-detiler mismatch mip="+std::to_string(m)+" layer="+std::to_string(l)+" x="+std::to_string(x)+" y="+std::to_string(y));++checked;}
    std::vector<uint32_t> guest(size_t(layout.guest_bytes/4));
    for(size_t i=0;i<guest.size();++i)guest[i]=uint32_t(i)^0xabcde123u;
    for(UINT m=0;m<desc.levels;++m)for(UINT layer=0;layer<desc.layers;++layer){
        auto decoded=DecodeGuestSubresource(desc,layout,std::as_bytes(std::span(guest)),m,layer);
        const auto& ml=layout.mips[m];const auto w=std::max(desc.width>>m,1u),h=std::max(desc.height>>m,1u);
        for(UINT sample=0;sample<samples;++sample)for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x)for(UINT c=0;c<bpp/4;++c){uint32_t value;memcpy(&value,decoded.data()+((size_t(sample)*h+y)*w+x)*bpp+c*4,4);
            const auto at=(ml.offset+layer*ml.slice_bytes)/4+((UINT64(sample)*ml.height+y)*ml.pitch+x)*(bpp/4)+c;
            if(value!=result[at])throw std::runtime_error("Optimized tile copy disagrees with original GPU detiler");}
    }
    // Encoding must restore every logical byte and leave padding untouched.
    auto roundtrip=guest;
    for(UINT m=0;m<desc.levels;++m)for(UINT layer=0;layer<desc.layers;++layer){
        auto decoded=DecodeGuestSubresource(desc,layout,std::as_bytes(std::span(guest)),m,layer);
        for(auto& byte:decoded)byte^=std::byte{0xff};
        EncodeGuestSubresource(desc,layout,std::as_writable_bytes(std::span(roundtrip)),decoded,m,layer);
        auto reread=DecodeGuestSubresource(desc,layout,std::as_bytes(std::span(roundtrip)),m,layer);
        Require(reread==decoded);
        for(auto& byte:decoded)byte^=std::byte{0xff};
        EncodeGuestSubresource(desc,layout,std::as_writable_bytes(std::span(roundtrip)),decoded,m,layer);
    }
    Require(roundtrip==guest);
    DX12_API_CALL(readback.handle->Unmap(0,&empty));printf("PASS: CPU guest macro/micro address decoder and optimized tile copy match original shadPS4 detiler running natively in D3D12: %llu visible texels, %u samples, %u mips, 2 layers\n",checked,samples,desc.levels);return 0;
}catch(const std::exception&e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}

