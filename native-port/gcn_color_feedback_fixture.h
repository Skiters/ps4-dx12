// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_rasterizer.h"
#include "video_core/amdgpu/liverpool.h"
#include "video_core/amdgpu/cb_db_extent.h"
#include <array>
#include <cstring>
#include <vector>

namespace NativeDX12 {
template<class Initialize>
void RunColorFeedbackGcnFixture(ID3D12Device* device,GuestRasterizer& rasterizer,AmdGpu::Liverpool& gpu,
    const std::array<u32,128>& vertex,std::byte* memory,std::byte* backing,u64 base,
    const Initialize& initialize,bool raw_domain=false,bool integer_view=false,bool shared_depth=false,
    bool independent_class=false,bool independent_depth=false,bool shared_clear=false,bool shared_ms=false,bool shared_sampled=false,
    bool retained_mips=false,bool grown_mips=false,bool shifted_mip=false,UINT converted_color=0,bool compressed_color=false){
    alignas(256) std::array<u32,128> seed{},left{},right{};
    initialize(seed,{0xBEEB03FF,15,0x7e0002ff,0x3f800000,0x7e020280,0x7e040280,
        0x7e0602ff,0x3f800000,0xf800000f,0x03020100,0xbf810000},0x4645454447430001ULL);
    if(shared_ms)initialize(seed,{0xBEEB03FF,15,
        0x7e0002ff,0x3e000000,0x7e0202ff,0x3e800000,
        0x7e0402ff,0x3f000000,0x7e0602ff,0x3f400000,
        0xf800000f,0x03020100,0xbf810000},0x4645454447430041ULL);
    const auto sample=[&](auto& program,u32 x,u64 hash){
        // image_load R -> v4, multiply by exactly .5, export to R32_FLOAT.
        initialize(program,{0xBEEB03FF,15,0x7e000280U|x,0x7e020280,
            0xf0000100,0x00000400,0xbf8c0000,0x100808ff,0x3f000000,
            0x7e0a0280,0x7e0c0280,0x7e0e02ff,0x3f800000,
            0xf800000f,0x07060504,0xbf810000},hash);
    };
    sample(left,0,0x4645454447430002ULL);sample(right,4,0x4645454447430003ULL);
    auto& regs=gpu.regs;regs={};regs.stage_enable.raw=AmdGpu::ShaderStageEnable::Vs;
    regs.primitive_type=AmdGpu::PrimitiveType::TriangleList;regs.num_indices=3;
    regs.vs_program.address=reinterpret_cast<u64>(vertex.data())>>8;
    regs.vs_program.settings.num_user_regs=4;regs.vs_program.settings.num_vgprs=1;
    regs.color_export_format.raw=u32(AmdGpu::ShaderExportFormat::ABGR_32);
    regs.color_control.mode=AmdGpu::ColorControl::OperationMode::Normal;
    regs.color_control.rop3=AmdGpu::ColorControl::LogicOp::Copy;
    regs.color_target_mask.raw=regs.color_shader_mask.raw=shared_ms?15:1;
    regs.clipper_control.clip_space=AmdGpu::ClipSpace::ZeroToW;
    auto& vp=regs.viewport_control;
    vp.xscale_enable=vp.yscale_enable=vp.xoffset_enable=vp.yoffset_enable=vp.zscale_enable=1;
    regs.viewports[0].xscale=4;regs.viewports[0].yscale=-4;
    regs.viewports[0].xoffset=regs.viewports[0].yoffset=4;regs.viewports[0].zscale=1;
    regs.screen_scissor.bottom_right_x=regs.window_scissor.bottom_right_x=regs.generic_scissor.bottom_right_x=8;
    regs.screen_scissor.bottom_right_y=regs.window_scissor.bottom_right_y=regs.generic_scissor.bottom_right_y=8;
    auto& cb=regs.color_buffers[0];cb.base_address=u32((base+16384)>>8);
    cb.info.format=shared_ms?14:4;cb.info.number_type=7;cb.attrib.tile_mode_index=AmdGpu::TileMode::Thin1DThin;
    cb.pitch.tile_max=cb.slice.tile_max=1;gpu.last_cb_extent[0]={8,8};
    if(converted_color==2){cb.info.format=9;cb.info.number_type=1;}
    if(compressed_color){cb.info.compression=1;cb.fmask_base_address=u32((base+57344)>>8);}
    const auto target=DescribeGuestColorBuffer(cb,gpu.last_cb_extent[0]);
    const auto layout=LayoutGuestTexture(target);
    AmdGpu::Image image{};image.base_address=(base+16384)>>8;
    image.type=u64(AmdGpu::ImageType::Color2D);image.width=image.height=7;image.pitch=15;
    if(independent_class)image.pitch=31;
    // GCN registers carry bits: loading UINT and then v_mul_f32 must interpret
    // the original FLOAT attachment bits, not numerically convert the texel.
    image.data_format=shared_ms?14:4;image.num_format=integer_view?4:7;image.tiling_index=u32(AmdGpu::TileMode::Thin1DThin);
    image.dst_sel_x=4;image.dst_sel_y=5;image.dst_sel_z=6;image.dst_sel_w=7;
    if(converted_color==1){image.data_format=10;image.num_format=0;image.dst_sel_x=6;}
    if(converted_color==2){image.data_format=9;image.num_format=1;}
    if(retained_mips||grown_mips)image.last_level=1;
    const auto sampled=DescribeGuestTexture(image);
    const auto sampled_layout=LayoutGuestTexture(sampled);
    if(independent_class){
        if(!raw_domain||!GuestTextureCache::SeparateColorSampledImages(target,layout,sampled,sampled_layout))
            throw std::runtime_error("Independent color fixture lost its different BlockDim class");
        auto same=sampled;same.pitch=target.pitch;
        if(GuestTextureCache::SeparateColorSampledImages(target,layout,same,LayoutGuestTexture(same)))
            throw std::runtime_error("Compatible feedback was misclassified as independent pool reuse");
    }else if(!converted_color&&!retained_mips&&!grown_mips&&!CompatibleGuestColorSampledMapping(target,layout,sampled,sampled_layout))
        throw std::runtime_error("Feedback fixture target/sample physical mapping differs");
    AmdGpu::Buffer input{};input.base_address=base;input.stride=32;input.num_records=3;
    input.data_format=4;input.num_format=4;
    input.dst_sel_x=4;input.dst_sel_y=5;input.dst_sel_z=6;input.dst_sel_w=7;
    memcpy(regs.vs_program.user_data.data(),&input,sizeof(input));
    rasterizer.InvalidateMemory(base,65536);memset(memory,0xa5,65536);
    const float xy[3][2]{{-1,-1},{3,-1},{-1,3}};
    for(u32 i=0;i<3;++i){const float p[]{xy[i][0],xy[i][1],.5f,1.f};memcpy(memory+i*32,p,sizeof(p));}
    if(independent_class){const float source=.75f;
        memcpy(memory+16384+GuestPixelOffset(sampled,sampled_layout,0,0,0,0),&source,4);}
    if(retained_mips||grown_mips){
        if(!raw_domain||sampled.levels!=2||sampled_layout.mips.size()!=2)
            throw std::runtime_error("Retained mip fixture requires two mips and independent B");
        const float value=.75f;
        for(UINT y=0;y<4;++y)for(UINT x=0;x<4;++x)
            memcpy(memory+16384+GuestPixelOffset(sampled,sampled_layout,1,0,x,y),&value,4);
    }
    std::vector<std::byte> physical(backing,backing+65536);
    // A large original index sharp selects resident B rather than the clean
    // <=16KiB private CPU stream. Establish it before any image writes.
    constexpr UINT raw_bytes=32768;
    Rasterizer::IndexBinding original_b{};
    if(raw_domain){
        original_b=rasterizer.ObtainIndexBuffer(base+16384,raw_bytes,DXGI_FORMAT_R32_UINT);
        if(!original_b.resource||original_b.bytes!=raw_bytes)
            throw std::runtime_error("Feedback fixture did not establish resident raw B");
    }
    const auto draw=[&](const auto& program,bool feedback){
        regs.ps_program.address=reinterpret_cast<u64>(program.data())>>8;
        regs.ps_program.settings.num_user_regs=feedback?8:0;regs.ps_program.settings.num_vgprs=1;
        if(feedback)memcpy(regs.ps_program.user_data.data(),&image,sizeof(image));
        if((shared_clear||shifted_mip)&&cb.cmask_base_address)
            std::fill_n(physical.begin()+49152,size_t(GuestCmaskBytes(cb)),std::byte{0xff});
        rasterizer.Draw(false);
        if(memcmp(backing,physical.data(),65536))
            throw std::runtime_error("Feedback draw published physical CPU bytes before explicit readback");
    };
    if(retained_mips){
        // First cache the entire readonly mip chain while drawing elsewhere.
        // CB then names only mip zero of this already-current allocation.
        cb.base_address=u32((base+61440)>>8);draw(left,true);
        cb.base_address=u32((base+16384)>>8);
    }
    draw(seed,false);
    if(converted_color){
        if(!raw_domain||!CompatibleGuestPackedStorage(target,sampled))
            throw std::runtime_error("Converted feedback fixture has no exact packed mapping");
        draw(left,true);
        cb.base_address=u32((base+61440)>>8);cb.info.format=4;cb.info.number_type=7;
        if(converted_color==1){image.data_format=4;image.num_format=7;image.dst_sel_x=4;}
        const auto probe=DescribeGuestColorBuffer(cb,gpu.last_cb_extent[0]);const auto pl=LayoutGuestTexture(probe);
        draw(left,true);
        const auto raw=rasterizer.ObtainIndexBuffer(base+16384,raw_bytes,DXGI_FORMAT_R32_UINT);
        if(!raw.resource||raw.bytes!=raw_bytes)throw std::runtime_error("Converted feedback lost independent B");
        auto rb=Buffer(device,raw_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        rasterizer.RetainUntilFinish([rb,lease=raw.allocation](u64){});const auto prior=raw.resource->State(0);
        raw.resource->Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        rasterizer.RecordingList()->CopyBufferRegion(rb.handle.Get(),0,raw.resource->handle.Get(),raw.offset,raw_bytes);
        raw.resource->Transition(rasterizer.RecordingList(),prior);rasterizer.Finish();
        void* bytes{};D3D12_RANGE range{0,raw_bytes};Check(rb.handle->Map(0,&range,&bytes));
        const bool preserved=memcmp(bytes,physical.data()+16384,raw_bytes)==0;
        D3D12_RANGE none{};rb.handle->Unmap(0,&none);
        if(!preserved)throw std::runtime_error("Converted feedback published I into independent B");
        rasterizer.ReadMemory(probe.address,pl.guest_bytes);rasterizer.Finish();
        // R32(1) -> RGBA8.B=128/255 -> half -> R32 probe half.
        // SNORM10(1) -> half -> quantized256/511 -> R32 probe half.
        auto expected=physical;const float value=converted_color==1?128.f/1020.f:128.f/511.f;
        for(UINT y=0;y<8;++y)for(UINT x=0;x<8;++x)
            memcpy(expected.data()+61440+GuestPixelOffset(probe,pl,0,0,x,y),&value,4);
        rasterizer.ValidateDebugMessages();
        if(memcmp(backing,expected.data(),65536))
            throw std::runtime_error("Converted feedback lost image contents, source state, lifetime or padding");
        return;
    }
    if(independent_class){
        // I_color now contains1, physical B still contains.75. The distinct
        // image class must sample B, so the next CB write is.375, never.5.
        draw(left,true);
        cb.base_address=u32((base+61440)>>8);image.pitch=15;
        const auto probe=DescribeGuestColorBuffer(cb,gpu.last_cb_extent[0]);
        const auto probe_layout=LayoutGuestTexture(probe);
        draw(left,true); // Return to the preserved CB class: .375*.5=.1875.
        auto raw_readback=Buffer(device,raw_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        const auto current_b=rasterizer.ObtainIndexBuffer(base+16384,raw_bytes,DXGI_FORMAT_R32_UINT);
        if(!current_b.resource||current_b.bytes!=raw_bytes)throw std::runtime_error("Independent color lost physical B");
        rasterizer.RetainUntilFinish([raw_readback,lease=current_b.allocation](u64){});
        const auto previous=current_b.resource->State(0);
        current_b.resource->Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        rasterizer.RecordingList()->CopyBufferRegion(raw_readback.handle.Get(),0,
            current_b.resource->handle.Get(),current_b.offset,raw_bytes);
        current_b.resource->Transition(rasterizer.RecordingList(),previous);
        rasterizer.Finish();void* bytes{};D3D12_RANGE range{0,raw_bytes};
        Check(raw_readback.handle->Map(0,&range,&bytes));
        const bool preserved=memcmp(bytes,physical.data()+16384,raw_bytes)==0;
        D3D12_RANGE none{};raw_readback.handle->Unmap(0,&none);
        if(!preserved)throw std::runtime_error("Independent color class replaced physical B with attachment data");
        rasterizer.ReadMemory(probe.address,probe_layout.guest_bytes);rasterizer.Finish();
        auto expected=physical;const float result=.1875f;
        for(u32 y=0;y<8;++y)for(u32 x=0;x<8;++x)
            memcpy(expected.data()+61440+GuestPixelOffset(probe,probe_layout,0,0,x,y),&result,4);
        rasterizer.ValidateDebugMessages();
        if(memcmp(backing,expected.data(),65536))
            throw std::runtime_error("Distinct color images lost B/I separation, cached contents or padding");
        return;
    }
    if(shared_depth||independent_depth||shared_ms||shared_sampled){
        auto& db=regs.depth_buffer;db.z_read_base=db.z_write_base=cb.base_address;
        db.z_info.format=AmdGpu::DepthBuffer::ZFormat::Z32Float;db.z_info.tile_mode_index=5;
        if(shared_ms){db.z_info.num_samples=2;regs.aa_config.msaa_num_samples=2;}
        db.depth_size.pitch_tile_max=independent_depth?3:1;
        db.depth_size.height_tile_max=0;db.depth_slice.tile_max=independent_depth?3:1;
        gpu.last_db_extent={8,8};
        regs.depth_control.depth_enable=regs.depth_control.depth_write_enable=1;
        regs.depth_control.depth_func=AmdGpu::CompareFunc::Always;
        const auto depth=DescribeGuestDepthBuffer(db,regs.depth_view,gpu.last_db_extent);
        const auto dl=LayoutGuestTexture(depth);
        if(independent_depth){
            if(!raw_domain||!GuestTextureCache::SeparateColorDepthAttachments(target,layout,depth,dl)||
               GuestTextureCache::CanRecreateColorDepthAttachments(target,layout,depth,dl))
                throw std::runtime_error("Independent depth fixture lost its distinct overlap class");
            auto same=depth;same.pitch=target.pitch;
            if(GuestTextureCache::SeparateColorDepthAttachments(target,layout,same,LayoutGuestTexture(same)))
                throw std::runtime_error("Equal color/depth geometry was misclassified as pool reuse");
        }else if(!GuestTextureCache::CanRecreateColorDepthAttachments(target,layout,depth,dl))
            throw std::runtime_error("Shared attachment fixture lost its original overlap class");
        auto shifted=depth;shifted.address+=256;
        if(GuestTextureCache::CanRecreateColorDepthAttachments(target,layout,shifted,LayoutGuestTexture(shifted)))
            throw std::runtime_error("Shifted attachment recreation was admitted without a physical plan");
        if(shared_sampled){
            regs.depth_control.depth_write_enable=0;
            // D inherits old C=1. PS samples D and writes .5 to the retained
            // retired CB. The next draw must still see D=1, not that CB write.
            draw(left,true);draw(left,true);
            const auto raw=rasterizer.ObtainIndexBuffer(base+16384,raw_bytes,DXGI_FORMAT_R32_UINT);
            if(!raw.resource||raw.bytes!=raw_bytes)throw std::runtime_error("Shared sampled depth lost resident B");
            auto rb=Buffer(device,raw_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            rasterizer.RetainUntilFinish([rb,lease=raw.allocation](u64){});const auto prior=raw.resource->State(0);
            raw.resource->Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            rasterizer.RecordingList()->CopyBufferRegion(rb.handle.Get(),0,raw.resource->handle.Get(),raw.offset,raw_bytes);
            raw.resource->Transition(rasterizer.RecordingList(),prior);rasterizer.Finish();
            void* bytes{};D3D12_RANGE range{0,raw_bytes};Check(rb.handle->Map(0,&range,&bytes));
            const bool preserved=memcmp(bytes,physical.data()+16384,raw_bytes)==0;
            D3D12_RANGE none{};rb.handle->Unmap(0,&none);
            if(!preserved)throw std::runtime_error("Shared sampled depth published I into independent raw B");
            regs.depth_control={};cb.base_address=u32((base+61440)>>8);
            const auto probe=DescribeGuestColorBuffer(cb,gpu.last_cb_extent[0]);const auto pl=LayoutGuestTexture(probe);
            draw(left,true);rasterizer.Finish();rasterizer.ReadMemory(probe.address,pl.guest_bytes);rasterizer.Finish();
            auto expected=physical;const float value=.5f;
            for(UINT y=0;y<8;++y)for(UINT x=0;x<8;++x)
                memcpy(expected.data()+61440+GuestPixelOffset(probe,pl,0,0,x,y),&value,4);
            rasterizer.ValidateDebugMessages();
            if(memcmp(backing,expected.data(),65536))
                throw std::runtime_error("Shared sampled depth lost prefetch ownership/order or published retired color");
            return;
        }
        if(shared_ms){
            // The original RGBA32 payload must reach D32x4 before CB is
            // expanded to four physical samples. Draw only the left half;
            // the right half distinguishes every transferred channel.
            regs.generic_scissor.bottom_right_x=4;
            draw(seed,false);
            const auto current_b=rasterizer.ObtainIndexBuffer(base+16384,raw_bytes,DXGI_FORMAT_R32_UINT);
            if(!current_b.resource||current_b.bytes!=raw_bytes)
                throw std::runtime_error("Shared MS attachments lost resident B");
            auto rb=Buffer(device,raw_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            rasterizer.RetainUntilFinish([rb,lease=current_b.allocation](u64){});
            const auto prior=current_b.resource->State(0);
            current_b.resource->Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            rasterizer.RecordingList()->CopyBufferRegion(rb.handle.Get(),0,current_b.resource->handle.Get(),current_b.offset,raw_bytes);
            current_b.resource->Transition(rasterizer.RecordingList(),prior);
            rasterizer.Finish();void* bytes{};D3D12_RANGE range{0,raw_bytes};Check(rb.handle->Map(0,&range,&bytes));
            const bool unchanged=memcmp(bytes,physical.data()+16384,raw_bytes)==0;
            D3D12_RANGE none{};rb.handle->Unmap(0,&none);
            if(!unchanged)throw std::runtime_error("Shared MS attachments published color or depth into raw B");
            rasterizer.ReadMemory(depth.address,dl.guest_bytes);rasterizer.Finish();
            auto expected=physical;constexpr float channels[]{.125f,.25f,.5f,.75f};
            for(UINT sample=0;sample<4;++sample)for(UINT y=0;y<8;++y)for(UINT x=0;x<8;++x){
                const float value=x<4?.5f:channels[sample];
                memcpy(expected.data()+16384+GuestPixelOffset(depth,dl,0,0,x,y,sample),&value,4);
            }
            rasterizer.ValidateDebugMessages();
            if(memcmp(backing,expected.data(),65536))
                throw std::runtime_error("Shared MS attachments lost channel/sample order, depth writer or physical padding");
            return;
        }
        if(shared_clear){
            cb.info.fast_clear=1;cb.cmask_base_address=u32((base+49152)>>8);
            cb.clear_word0=0x3f400000; // .75: must clear retired CB AFTER the copy into D.
            const auto bytes=GuestCmaskBytes(cb);
            if(bytes>61440-49152)throw std::runtime_error("Shared CMASK fixture metadata overlaps probe target");
            rasterizer.InvalidateMemory(base+49152,bytes);memset(memory+49152,0,size_t(bytes));
            physical.assign(backing,backing+65536);
            regs.generic_scissor.bottom_right_x=4;
        }
        draw(seed,false); // CB writes1, D writes vertex Z=.5 into the new owner.
        rasterizer.InvalidateMemory(base,96);
        for(UINT i=0;i<3;++i){const float z=.25f;memcpy(memory+i*32+8,&z,4);}
        physical.assign(backing,backing+65536);
        draw(seed,false); // Repeat D->CB->D recreation, now with Z=.25.
        regs.depth_control={};cb.base_address=u32((base+61440)>>8);
        if(shared_clear){cb.info.fast_clear=0;cb.cmask_base_address=0;regs.generic_scissor.bottom_right_x=8;}
        const auto probe_desc=DescribeGuestColorBuffer(cb,gpu.last_cb_extent[0]);
        const auto probe_layout=LayoutGuestTexture(probe_desc);
        if(independent_depth){
            // D must not retire C. Sample both independently through actual
            // GCN, while the resident raw buffer remains the initial bytes.
            auto expected=physical;
            for(const auto [pitch,value]:std::array<std::pair<u32,float>,2>{{{15,.5f},{31,.125f}}}){
                image.pitch=pitch;draw(left,true);rasterizer.Finish();
                rasterizer.ReadMemory(probe_desc.address,probe_layout.guest_bytes);
                for(UINT y=0;y<8;++y)for(UINT x=0;x<8;++x)
                    memcpy(expected.data()+61440+GuestPixelOffset(probe_desc,probe_layout,0,0,x,y),&value,4);
                if(memcmp(backing,expected.data(),65536))
                    throw std::runtime_error("Independent CB/DB lost an image, changed B or padding");
                physical.assign(backing,backing+65536);
            }
            const auto current_b=rasterizer.ObtainIndexBuffer(base+16384,raw_bytes,DXGI_FORMAT_R32_UINT);
            if(!current_b.resource||current_b.bytes!=raw_bytes)throw std::runtime_error("Independent CB/DB lost resident B");
            auto readback=Buffer(device,raw_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            rasterizer.RetainUntilFinish([readback,lease=current_b.allocation](u64){});
            const auto prior=current_b.resource->State(0);
            current_b.resource->Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            rasterizer.RecordingList()->CopyBufferRegion(readback.handle.Get(),0,current_b.resource->handle.Get(),current_b.offset,raw_bytes);
            current_b.resource->Transition(rasterizer.RecordingList(),prior);
            rasterizer.Finish();void* bytes{};D3D12_RANGE range{0,raw_bytes};
            Check(readback.handle->Map(0,&range,&bytes));
            const bool preserved=memcmp(bytes,physical.data()+16384,raw_bytes)==0;
            D3D12_RANGE none{};readback.handle->Unmap(0,&none);
            if(!preserved)throw std::runtime_error("Independent CB/DB published I into resident B");
            return;
        }
        draw(left,true); // Real original GCN reads CURRENT D and multiplies .5.
        rasterizer.Finish();rasterizer.ReadMemory(base+61440,probe_layout.guest_bytes);
        auto expected=physical;const float expected_depth=.125f;
        for(UINT y=0;y<8;++y)for(UINT x=0;x<8;++x)
            memcpy(expected.data()+61440+GuestPixelOffset(probe_desc,probe_layout,0,0,x,y),&expected_depth,4);
        if(memcmp(backing,expected.data(),65536))
            throw std::runtime_error("Shared attachments lost depth owner, published retired color or changed B/padding");
        if(shared_clear){
            physical.assign(backing,backing+65536);
            draw(right,true);rasterizer.Finish();rasterizer.ReadMemory(base+61440,probe_layout.guest_bytes);
            // Untouched D came from old C=1, not the subsequent retired clear=.75.
            const float untouched=.5f;
            for(UINT y=0;y<8;++y)for(UINT x=0;x<8;++x)
                memcpy(expected.data()+61440+GuestPixelOffset(probe_desc,probe_layout,0,0,x,y),&untouched,4);
            if(memcmp(backing,expected.data(),65536))
                throw std::runtime_error("Retired color clear was reordered before depth recreation or published into B");
        }
        return;
    }
    // Read outside the written rectangle. Successive draws consume the last
    // GPU result; this requires no intra-draw fragment communication.
    regs.generic_scissor.top_left_x=4;draw(left,true);
    regs.generic_scissor.top_left_x=0;regs.generic_scissor.bottom_right_x=4;draw(right,true);
    regs.generic_scissor.top_left_x=4;regs.generic_scissor.bottom_right_x=8;draw(left,true);
    if(raw_domain){
        const auto current_b=rasterizer.ObtainIndexBuffer(base+16384,raw_bytes,DXGI_FORMAT_R32_UINT);
        if(!current_b.resource||current_b.bytes!=raw_bytes)
            throw std::runtime_error("Feedback fixture lost resident raw B");
        auto raw_readback=Buffer(device,raw_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        rasterizer.RetainUntilFinish([raw_readback,lease=current_b.allocation](u64){(void)raw_readback;(void)lease;});
        const auto prior=current_b.resource->State(0);
        current_b.resource->Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        rasterizer.RecordingList()->CopyBufferRegion(raw_readback.handle.Get(),0,
            current_b.resource->handle.Get(),current_b.offset,raw_bytes);
        current_b.resource->Transition(rasterizer.RecordingList(),prior);
        if(memcmp(backing,physical.data(),65536))
            throw std::runtime_error("Resident B admission published native image I to CPU");
        rasterizer.Finish();
        void* raw{};const D3D12_RANGE read_range{0,raw_bytes};
        Check(raw_readback.handle->Map(0,&read_range,&raw));
        const bool preserved=memcmp(raw,physical.data()+16384,raw_bytes)==0;
        const D3D12_RANGE no_write{0,0};raw_readback.handle->Unmap(0,&no_write);
        if(!preserved)throw std::runtime_error("Feedback native image writes replaced independent raw B contents");
        if(memcmp(backing,physical.data(),65536))
            throw std::runtime_error("Independent image I escaped into physical backing after raw B readback");
        // Once B and I are independent, CPU reads observe B, not I. Observe I via
        // a real guest shader into a separate color target, never by forcing an
        // image download over the physical B bytes we are testing.
        cb.base_address=u32((base+61440)>>8);
        const auto probe_desc=DescribeGuestColorBuffer(cb,gpu.last_cb_extent[0]);
        const auto probe_layout=LayoutGuestTexture(probe_desc);
        regs.generic_scissor.top_left_x=0;
        auto expected=physical;
        const auto probe=[&](const auto& program,float result){
            draw(program,true);rasterizer.Finish();rasterizer.ReadMemory(base+61440,probe_layout.guest_bytes);
            for(u32 y=0;y<8;++y)for(u32 x=0;x<8;++x)
                memcpy(expected.data()+61440+GuestPixelOffset(probe_desc,probe_layout,0,0,x,y),&result,4);
            if(memcmp(backing,expected.data(),65536))
                throw std::runtime_error("Feedback independent I probe, physical B or padding mismatch");
            physical.assign(backing,backing+65536);
        };
        probe(left,.125f);probe(right,.0625f);
        if(retained_mips||grown_mips){
            image.base_level=1;
            probe(left,.375f); // Mip1 survives every mip0 render/feedback write.
        }
        if(shifted_mip){
            const auto saved_cb=cb;
            const auto mip_address=sampled.address+sampled_layout.mips[1].offset;
            if(mip_address&255)throw std::runtime_error("Shifted CB mip is not register aligned");
            cb.base_address=u32(mip_address>>8);cb.pitch.tile_max=cb.slice.tile_max=0;
            cb.info.fast_clear=1;cb.cmask_base_address=u32((base+49152)>>8);cb.cmask_slice.tile_max=0;
            cb.clear_word0=0x3e800000;cb.clear_word1=0;
            const auto metadata_bytes=GuestCmaskBytes(cb);
            rasterizer.InvalidateMemory(base+49152,metadata_bytes);
            memset(memory+49152,0,size_t(metadata_bytes));
            std::fill_n(physical.begin()+49152,size_t(metadata_bytes),std::byte{0});
            gpu.last_cb_extent[0]={4,4};
            regs.viewports[0].xscale=2;regs.viewports[0].yscale=-2;
            regs.viewports[0].xoffset=regs.viewports[0].yoffset=2;
            regs.generic_scissor.top_left_x=2;
            draw(seed,false); // Clear left to .25; render right to 1 in parent mip1.
            cb=saved_cb;gpu.last_cb_extent[0]={8,8};
            regs.viewports[0].xscale=4;regs.viewports[0].yscale=-4;
            regs.viewports[0].xoffset=regs.viewports[0].yoffset=4;
            regs.generic_scissor.top_left_x=0;
            expected=physical;
            alignas(256) std::array<u32,128> middle{};sample(middle,2,0x4645454447430051ULL);
            probe(left,.125f);probe(middle,.5f);
            image.base_level=0;probe(left,.125f); // Original mip0 remains .25.
        }
        return;
    }
    rasterizer.Finish();rasterizer.ReadMemory(base,65536);
    auto expected=physical;
    for(u32 y=0;y<8;++y)for(u32 x=0;x<8;++x){
        const float value=x<4?.25f:.125f;
        memcpy(expected.data()+16384+GuestPixelOffset(target,layout,0,0,x,y),&value,4);
    }
    if(memcmp(backing,expected.data(),65536))
        throw std::runtime_error("GCN feedback current GPU pixels or complete physical padding mismatch");
    // After completed readback, an ordinary CPU write invalidates the cached
    // image version. The next feedback draw must load that precise texel;
    // neither the previous private snapshot nor resident B can override it.
    const auto cpu_offset=16384+GuestPixelOffset(target,layout,0,0,0,0);
    rasterizer.InvalidateMemory(base+cpu_offset,sizeof(float));
    const float cpu_value=.75f;memcpy(memory+cpu_offset,&cpu_value,sizeof(cpu_value));
    memcpy(expected.data()+cpu_offset,&cpu_value,sizeof(cpu_value));
    physical.assign(backing,backing+65536);
    draw(left,true);
    rasterizer.Finish();rasterizer.ReadMemory(base,65536);
    for(u32 y=0;y<8;++y)for(u32 x=4;x<8;++x){
        const float value=.375f;
        memcpy(expected.data()+16384+GuestPixelOffset(target,layout,0,0,x,y),&value,4);
    }
    if(memcmp(backing,expected.data(),65536))
        throw std::runtime_error("CPU texel winner was lost by feedback rebinding or changed neighbouring bytes");
}
} // namespace NativeDX12
