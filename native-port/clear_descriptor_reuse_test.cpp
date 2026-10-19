// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_image_operations.h"
#include "video_core/renderer_d3d12/texture_resources.h"
#include "video_core/renderer_d3d12/backend_factory.h"
#include <d3d12sdklayers.h>
#include <cstdio>
using namespace NativeDX12;
static void Require(bool value){if(!value)throw std::runtime_error("Clear descriptor reuse contract failed");}
static GuestTextureCache::Entry Image(ID3D12Device* device,bool target){
    GuestTextureCache::Entry e;e.desc.width=e.desc.pitch=16;e.desc.height=8;
    e.desc.levels=2;e.desc.layers=2;e.desc.first_mip=1;e.desc.first_layer=1;
    e.desc.view_layers=1;e.desc.data_format=4;e.desc.number_format=7;
    e.desc.render_target=target;e.desc.writable=!target;e.layout=LayoutGuestTexture(e.desc);
    D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d.Width=16;d.Height=8;d.MipLevels=2;d.DepthOrArraySize=2;
    d.Format=DXGI_FORMAT_R32_TYPELESS;d.SampleDesc.Count=1;
    d.Flags=target?D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET:D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
    Check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COMMON,
        nullptr,IID_PPV_ARGS(&e.texture.handle)));return e;
}
static std::vector<TextureTransfer> Read(ID3D12Device* device,Rasterizer& renderer,Resource& image){
    std::vector<TextureTransfer> result;
    for(UINT sub=0;sub<4;++sub){auto transfer=TextureTransfer::CreateSubresource(device,image.handle->GetDesc(),sub,true);
        transfer.Readback(renderer.RecordingList(),image);result.push_back(std::move(transfer));}
    return result;
}
static void Verify(std::vector<TextureTransfer>& transfers,float expected){
    for(auto& transfer:transfers){const auto width=transfer.footprint.Footprint.Width,height=transfer.rows;
        std::vector<float> values(size_t(width)*height);
        transfer.ReadRows(values.data(),values.size()*4,width*4);
        for(float value:values)Require(std::bit_cast<uint32_t>(value)==std::bit_cast<uint32_t>(expected));}
}
int main(){try{
    DeviceOptions options;options.validation_enabled=true;
    auto device=CreateHardwareDevice(nullptr,options);ComPtr<ID3D12InfoQueue> messages;Check(device.As(&messages));
    Rasterizer renderer(device.Get());
    for(bool target:{false,true}){
        auto e=Image(device.Get(),target);const float first[]{0.25f,0,0,0},second[]{0.75f,0,0,0};
        std::shared_ptr<GuestStorageImageClearLease> storage;
        std::shared_ptr<GuestImageClearLease> render;
        const auto clear=[&](const float (&color)[4]){
            if(target)RecordGuestImageClear(device.Get(),e,renderer.RecordingList(),color);
            else RecordGuestStorageImageClear(device.Get(),e,renderer.RecordingList(),color);
        };
        clear(first);storage=e.storage_clear_descriptors;render=e.render_clear_descriptors;
        Require(target?bool(render):bool(storage));
        auto first_read=Read(device.Get(),renderer,e.texture);
        for(unsigned repeat=0;repeat<64;++repeat)clear(second);
        Require(e.storage_clear_descriptors==storage&&e.render_clear_descriptors==render);
        auto second_read=Read(device.Get(),renderer,e.texture);
        // Retiring a submission must not recreate immutable descriptors on
        // the following command list.
        renderer.Finish();Verify(first_read,0.25f);Verify(second_read,0.75f);
        clear(first);Require(e.storage_clear_descriptors==storage&&e.render_clear_descriptors==render);
        auto old_read=Read(device.Get(),renderer,e.texture);
        // Replace the backing while the previous clear/copy remains pending.
        // Old descriptors/resource must remain usable until its fence retires.
        auto replacement=Image(device.Get(),target);e.texture=replacement.texture;
        bool rejected=false;
        try{if(target)RecordGuestImageClear(device.Get(),e,renderer.RecordingList(),second,render);
            else RecordGuestStorageImageClear(device.Get(),e,renderer.RecordingList(),second,storage);}
        catch(const std::invalid_argument&){rejected=true;}Require(rejected);
        clear(second);Require(target?e.render_clear_descriptors!=render:e.storage_clear_descriptors!=storage);
        auto new_read=Read(device.Get(),renderer,e.texture);
        renderer.Finish();Verify(old_read,0.25f);Verify(new_read,0.75f);
        // A descriptor-affecting view change on the same typeless resource
        // cannot reuse the old format, even though the resource pointer matches.
        if(!target){const float bits[]{std::bit_cast<float>(uint32_t(37)),0,0,0};
            auto prior=e.storage_clear_descriptors;
            e.desc.number_format=4;e.layout=LayoutGuestTexture(e.desc);rejected=false;
            try{RecordGuestStorageImageClear(device.Get(),e,renderer.RecordingList(),bits,prior);}
            catch(const std::invalid_argument&){rejected=true;}Require(rejected);
            clear(bits);Require(e.storage_clear_descriptors!=prior);
            auto integer_read=Read(device.Get(),renderer,e.texture);renderer.Finish();
            Verify(integer_read,std::bit_cast<float>(uint32_t(37)));}
        std::weak_ptr<GuestStorageImageClearLease> storage_lifetime=e.storage_clear_descriptors;
        std::weak_ptr<GuestImageClearLease> render_lifetime=e.render_clear_descriptors;
        e.storage_clear_descriptors.reset();e.render_clear_descriptors.reset();storage.reset();render.reset();
        Require(storage_lifetime.expired()&&render_lifetime.expired());
    }
    for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T bytes{};messages->GetMessage(i,nullptr,&bytes);
        std::vector<std::byte> buffer(bytes);auto* message=reinterpret_cast<D3D12_MESSAGE*>(buffer.data());
        Check(messages->GetMessage(i,message,&bytes));
        if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);}
    puts("GPU clear descriptor reuse: ordered contents, mips/layers, replacement, format and lifetime passed");return 0;
}catch(const std::exception& error){fprintf(stderr,"%s\n",error.what());return 1;}}
