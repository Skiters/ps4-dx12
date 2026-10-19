// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_graphics_state.h"
#include <algorithm>
#include <cmath>
namespace NativeDX12 {
bool GuestPrimitiveNeedsExpansion(AmdGpu::PrimitiveType type) {
    using P=AmdGpu::PrimitiveType;
    return type==P::TriangleFan||type==P::Polygon||type==P::LineLoop||type==P::QuadStrip;
}
std::vector<u32> ExpandGuestPrimitive(AmdGpu::PrimitiveType type,std::span<const u32> indices,
    bool restart,u32 restart_index) {
    std::vector<u32> result;
    const auto emit=[&](std::span<const u32> part) {
        using P=AmdGpu::PrimitiveType;
        switch(type) {
        case P::TriangleFan:case P::Polygon:
            if(part.size()>2){result.reserve(result.size()+(part.size()-2)*3);
                for(size_t i=1;i+1<part.size();++i){result.push_back(part[0]);result.push_back(part[i]);result.push_back(part[i+1]);}}
            break;
        case P::LineLoop:
            if(part.size()>1){result.reserve(result.size()+part.size()*2);
                for(size_t i=0;i+1<part.size();++i){result.push_back(part[i]);result.push_back(part[i+1]);}
                result.push_back(part.back());result.push_back(part.front());}
            break;
        case P::QuadStrip:
            if(part.size()>3){result.reserve(result.size()+((part.size()-2)/2)*6);
                for(size_t i=0;i+3<part.size();i+=2){result.push_back(part[i]);result.push_back(part[i+1]);result.push_back(part[i+2]);
                    result.push_back(part[i+2]);result.push_back(part[i+1]);result.push_back(part[i+3]);}}
            break;
        case P::TriangleStrip:
            if(part.size()>2)for(size_t i=0;i+2<part.size();++i){
                // Keep the strip's leading vertex for flat interpolation,
                // while reversing the other two vertices on odd triangles.
                result.push_back(part[i]);result.push_back(part[i+1+(i&1)]);result.push_back(part[i+2-(i&1)]);
            }
            break;
        case P::LineStrip:
            if(part.size()>1)for(size_t i=0;i+1<part.size();++i){result.push_back(part[i]);result.push_back(part[i+1]);}
            break;
        case P::TriangleList:case P::RectList:
            result.insert(result.end(),part.begin(),part.begin()+part.size()/3*3);break;
        case P::LineList:
            result.insert(result.end(),part.begin(),part.begin()+part.size()/2*2);break;
        case P::QuadList:case P::AdjLineList:
            result.insert(result.end(),part.begin(),part.begin()+part.size()/4*4);break;
        case P::AdjTriangleList:
            result.insert(result.end(),part.begin(),part.begin()+part.size()/6*6);break;
        default:
            result.insert(result.end(),part.begin(),part.end());break;
        }
    };
    if(!restart){emit(indices);return result;}
    size_t begin{};
    for(size_t i=0;i<=indices.size();++i)if(i==indices.size()||indices[i]==restart_index){
        emit(indices.subspan(begin,i-begin));begin=i+1;
    }
    return result;
}
namespace {
D3D12_BLEND Factor(AmdGpu::BlendControl::BlendFactor factor,bool alpha=false,bool native_alpha_factor=false) {
    using F=AmdGpu::BlendControl::BlendFactor;
    switch(factor) {
    case F::Zero:return D3D12_BLEND_ZERO;case F::One:return D3D12_BLEND_ONE;
    case F::SrcColor:return alpha?D3D12_BLEND_SRC_ALPHA:D3D12_BLEND_SRC_COLOR;
    case F::OneMinusSrcColor:return alpha?D3D12_BLEND_INV_SRC_ALPHA:D3D12_BLEND_INV_SRC_COLOR;
    case F::SrcAlpha:return D3D12_BLEND_SRC_ALPHA;case F::OneMinusSrcAlpha:return D3D12_BLEND_INV_SRC_ALPHA;
    case F::DstColor:return alpha?D3D12_BLEND_DEST_ALPHA:D3D12_BLEND_DEST_COLOR;
    case F::OneMinusDstColor:return alpha?D3D12_BLEND_INV_DEST_ALPHA:D3D12_BLEND_INV_DEST_COLOR;
    case F::DstAlpha:return D3D12_BLEND_DEST_ALPHA;case F::OneMinusDstAlpha:return D3D12_BLEND_INV_DEST_ALPHA;
    case F::SrcAlphaSaturate:return alpha?D3D12_BLEND_ONE:D3D12_BLEND_SRC_ALPHA_SAT;
    case F::ConstantColor:return D3D12_BLEND_BLEND_FACTOR;case F::OneMinusConstantColor:return D3D12_BLEND_INV_BLEND_FACTOR;
    case F::ConstantAlpha:return !alpha&&native_alpha_factor?D3D12_BLEND_ALPHA_FACTOR:D3D12_BLEND_BLEND_FACTOR;
    case F::OneMinusConstantAlpha:return !alpha&&native_alpha_factor?D3D12_BLEND_INV_ALPHA_FACTOR:D3D12_BLEND_INV_BLEND_FACTOR;
    case F::Src1Color:return alpha?D3D12_BLEND_SRC1_ALPHA:D3D12_BLEND_SRC1_COLOR;
    case F::InvSrc1Color:return alpha?D3D12_BLEND_INV_SRC1_ALPHA:D3D12_BLEND_INV_SRC1_COLOR;
    case F::Src1Alpha:return D3D12_BLEND_SRC1_ALPHA;case F::InvSrc1Alpha:return D3D12_BLEND_INV_SRC1_ALPHA;
    }
    throw std::runtime_error("Native guest blend factor requires shader/device-specific emulation");
}
D3D12_BLEND_OP BlendOp(AmdGpu::BlendControl::BlendFunc func) {
    using F=AmdGpu::BlendControl::BlendFunc;
    switch(func){case F::Add:return D3D12_BLEND_OP_ADD;case F::Subtract:return D3D12_BLEND_OP_SUBTRACT;
    case F::ReverseSubtract:return D3D12_BLEND_OP_REV_SUBTRACT;case F::Min:return D3D12_BLEND_OP_MIN;case F::Max:return D3D12_BLEND_OP_MAX;}
    throw std::invalid_argument("Invalid native guest blend operation");
}
void NormalizeMinMaxBlend(D3D12_BLEND_OP& op,D3D12_BLEND& src,D3D12_BLEND& dst,AmdGpu::NumberFormat format){
    if(format!=AmdGpu::NumberFormat::Unorm&&format!=AmdGpu::NumberFormat::Srgb)return;
    if(op!=D3D12_BLEND_OP_MIN&&op!=D3D12_BLEND_OP_MAX)return;
    if((src!=D3D12_BLEND_ZERO&&src!=D3D12_BLEND_ONE)||
       (dst!=D3D12_BLEND_ZERO&&dst!=D3D12_BLEND_ONE)||
       (src!=D3D12_BLEND_ZERO&&dst!=D3D12_BLEND_ZERO))return;
    if(op==D3D12_BLEND_OP_MIN)src=dst=D3D12_BLEND_ZERO;
    op=D3D12_BLEND_OP_ADD;
}
D3D12_LOGIC_OP LogicOp(AmdGpu::ColorControl::LogicOp op) {
    using L=AmdGpu::ColorControl::LogicOp;
    switch(op) {
    case L::Clear:return D3D12_LOGIC_OP_CLEAR;case L::Nor:return D3D12_LOGIC_OP_NOR;
    case L::AndInverted:return D3D12_LOGIC_OP_AND_INVERTED;case L::CopyInverted:return D3D12_LOGIC_OP_COPY_INVERTED;
    case L::AndReverse:return D3D12_LOGIC_OP_AND_REVERSE;case L::Invert:return D3D12_LOGIC_OP_INVERT;
    case L::BrushXor:case L::Xor:return D3D12_LOGIC_OP_XOR;case L::Nand:return D3D12_LOGIC_OP_NAND;
    case L::And:return D3D12_LOGIC_OP_AND;case L::Equiv:return D3D12_LOGIC_OP_EQUIV;
    case L::Noop:return D3D12_LOGIC_OP_NOOP;case L::OrInverted:return D3D12_LOGIC_OP_OR_INVERTED;
    case L::Copy:return D3D12_LOGIC_OP_COPY;case L::OrReverse:return D3D12_LOGIC_OP_OR_REVERSE;
    case L::Or:return D3D12_LOGIC_OP_OR;case L::Set:return D3D12_LOGIC_OP_SET;
    }
    throw std::invalid_argument("Invalid native guest raster logic operation");
}
struct StencilReference {
    UINT value{},mask{};
    void Require(UINT requested,UINT bits) {
        if(((value^requested)&mask&bits)!=0)
            throw std::runtime_error("Native stencil comparison/write references conflict: current="+
                std::to_string(value)+" requested="+std::to_string(requested)+" mask="+std::to_string(mask&bits));
        value=(value&~bits)|(requested&bits);mask|=bits;
    }
};
// Bits that this guest operation can change for any input stencil value.
// A face shares one write mask across fail, depth-fail and pass operations.
UINT StencilChangedBits(AmdGpu::StencilFunc func,const AmdGpu::StencilRefMask& source) {
    using F=AmdGpu::StencilFunc;
    const UINT mask=source.stencil_write_mask,operand=source.stencil_op_val;
    switch(func) {
    case F::Keep:return 0;
    case F::And:case F::Xnor:return mask&~operand;
    case F::Or:case F::Xor:return mask&operand;
    default:return mask;
    }
}
D3D12_STENCIL_OP StencilOp(AmdGpu::StencilFunc func,const AmdGpu::StencilRefMask& source,StencilReference& reference) {
    using F=AmdGpu::StencilFunc;
    if(!source.stencil_write_mask)return D3D12_STENCIL_OP_KEEP;
    const auto replace=[&](UINT value){reference.Require(value,source.stencil_write_mask);return D3D12_STENCIL_OP_REPLACE;};
    switch(func){case F::Keep:return D3D12_STENCIL_OP_KEEP;case F::Zero:return D3D12_STENCIL_OP_ZERO;
    case F::Ones:return replace(255);case F::ReplaceTest:return replace(source.stencil_test_val);
    case F::ReplaceOp:return replace(source.stencil_op_val);case F::AddClamp:return D3D12_STENCIL_OP_INCR_SAT;
    case F::SubClamp:return D3D12_STENCIL_OP_DECR_SAT;case F::Invert:return D3D12_STENCIL_OP_INVERT;
    case F::AddWrap:return D3D12_STENCIL_OP_INCR;case F::SubWrap:return D3D12_STENCIL_OP_DECR;
    default:break;}
    const auto value=source.stencil_op_val&source.stencil_write_mask;
    if(value==0||value==source.stencil_write_mask)switch(func){
    case F::And:return value?D3D12_STENCIL_OP_KEEP:D3D12_STENCIL_OP_ZERO;
    case F::Or:return value?replace(255):D3D12_STENCIL_OP_KEEP;
    case F::Xor:return value?D3D12_STENCIL_OP_INVERT:D3D12_STENCIL_OP_KEEP;
    case F::Nand:return value?D3D12_STENCIL_OP_INVERT:replace(255);
    case F::Nor:return value?D3D12_STENCIL_OP_ZERO:D3D12_STENCIL_OP_INVERT;
    case F::Xnor:return value?D3D12_STENCIL_OP_KEEP:D3D12_STENCIL_OP_INVERT;
    default:break;}
    // Do not silently replace an unrepresentable operation with KEEP: that
    // would lose coverage/ownership while looking like a successful draw.
    throw std::runtime_error("Native stencil needs ordered emulation: operation="+
        std::to_string(UINT(func))+" operand="+std::to_string(source.stencil_op_val)+
        " shared_write_mask="+std::to_string(source.stencil_write_mask));
}
}
GuestGraphicsState BuildGuestGraphicsState(const AmdGpu::Regs& regs,
    std::span<const DXGI_FORMAT> targets,DXGI_FORMAT depth,UINT patch_points,bool native_alpha_factor) {
    if(targets.size()>8)throw std::invalid_argument("Native guest render-target slot count");
    GuestGraphicsState result;auto& p=result.pipeline;
    p.NumRenderTargets=UINT(targets.size());std::copy(targets.begin(),targets.end(),p.RTVFormats);
    p.DSVFormat=depth;p.SampleDesc.Count=depth!=DXGI_FORMAT_UNKNOWN?regs.depth_buffer.NumSamples():1;
    UINT color_samples=0;bool same_color_samples=true;
    for(size_t slot=0;slot<targets.size();++slot)if(targets[slot]!=DXGI_FORMAT_UNKNOWN){
        const auto samples=regs.color_buffers[slot].NumSamples();
        same_color_samples&=color_samples==0||color_samples==samples;color_samples=samples;
    }
    if(depth==DXGI_FORMAT_UNKNOWN&&same_color_samples&&color_samples)p.SampleDesc.Count=color_samples;
    p.SampleMask=UINT_MAX;
    p.BlendState.IndependentBlendEnable=TRUE;
    const bool normal=regs.color_control.mode==AmdGpu::ColorControl::OperationMode::Normal;
    if(!normal&&regs.color_control.mode!=AmdGpu::ColorControl::OperationMode::Disable)
        throw std::runtime_error("Native guest fast-clear/resolve/decompress pass needs attachment operation");
    u32 active_color_mask{};
    for (u32 i = 0; i < targets.size(); ++i)
        if (targets[i] != DXGI_FORMAT_UNKNOWN) active_color_mask |= 1u << i;
    const u32 ordered_color_mask = GuestOrderedColorMask(regs, active_color_mask);
    bool needs_constant_color_rgb=false,needs_constant_alpha_rgb=false;
    for(UINT i=0;i<8;++i) {
        auto& rt=p.BlendState.RenderTarget[i];rt.SrcBlend=rt.SrcBlendAlpha=D3D12_BLEND_ONE;
        rt.DestBlend=rt.DestBlendAlpha=D3D12_BLEND_ZERO;rt.BlendOp=rt.BlendOpAlpha=D3D12_BLEND_OP_ADD;
        rt.LogicOp=D3D12_LOGIC_OP_COPY;
        if(i>=targets.size()||targets[i]==DXGI_FORMAT_UNKNOWN)continue;
        rt.RenderTargetWriteMask=normal?UINT8(regs.color_target_mask.GetMask(i)):0;
        if(!rt.RenderTargetWriteMask)continue;
        // Match the pinned CB mask scatter independently of the shader export's
        // component gather. Native-only callers can omit the guest CB format.
        if(regs.color_buffers[i].info.format)
            rt.RenderTargetWriteMask=UINT8(regs.color_buffers[i].Swizzle().ApplyMask(rt.RenderTargetWriteMask));
        if (ordered_color_mask & (1u << i)) {
            using BF = AmdGpu::BlendControl::BlendFactor;
            const auto dual = [](BF factor) {
                return factor == BF::Src1Color || factor == BF::InvSrc1Color ||
                       factor == BF::Src1Alpha || factor == BF::InvSrc1Alpha;
            };
            const auto& blend = regs.blend_control[i];
            if (regs.color_buffers[i].NumSamples() != 1)
                throw std::runtime_error("Native packed10 SNORM ordered color needs single-sample storage");
            if (regs.color_control.rop3 == AmdGpu::ColorControl::LogicOp::Copy &&
                (dual(blend.color_src_factor) || dual(blend.color_dst_factor) ||
                (blend.separate_alpha_blend &&
                 (dual(blend.alpha_src_factor) || dual(blend.alpha_dst_factor)))))
                throw std::runtime_error("Native packed10 SNORM ordered blending does not support dual-source input");
            result.ordered_color_mask |= 1u << i;
            p.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
            rt.RenderTargetWriteMask = 0;
            continue;
        }
        const bool logic=regs.color_control.rop3!=AmdGpu::ColorControl::LogicOp::Copy;
        rt.LogicOpEnable=logic;rt.LogicOp=LogicOp(regs.color_control.rop3);
        const auto& source=regs.blend_control[i];rt.BlendEnable=!logic&&source.enable&&!regs.color_buffers[i].info.blend_bypass;
        if(rt.BlendEnable) {
            using BF=AmdGpu::BlendControl::BlendFactor;
            const auto is_color=[](BF f){return f==BF::ConstantColor||f==BF::OneMinusConstantColor;};
            const auto is_alpha=[](BF f){return f==BF::ConstantAlpha||f==BF::OneMinusConstantAlpha;};
            needs_constant_color_rgb|=is_color(source.color_src_factor)||is_color(source.color_dst_factor);
            needs_constant_alpha_rgb|=is_alpha(source.color_src_factor)||is_alpha(source.color_dst_factor);
            rt.SrcBlend=Factor(source.color_src_factor,false,native_alpha_factor);
            rt.DestBlend=Factor(source.color_dst_factor,false,native_alpha_factor);rt.BlendOp=BlendOp(source.color_func);
            rt.SrcBlendAlpha=Factor(source.separate_alpha_blend?source.alpha_src_factor:source.color_src_factor,true);
            rt.DestBlendAlpha=Factor(source.separate_alpha_blend?source.alpha_dst_factor:source.color_dst_factor,true);
            rt.BlendOpAlpha=BlendOp(source.separate_alpha_blend?source.alpha_func:source.color_func);
            if(regs.color_buffers[i].GetDataFmt()==AmdGpu::DataFormat::Format32_32_32){
                const auto rgb_destination_alpha=[](D3D12_BLEND factor){
                    return factor==D3D12_BLEND_DEST_ALPHA?D3D12_BLEND_ONE:
                        factor==D3D12_BLEND_INV_DEST_ALPHA||factor==D3D12_BLEND_SRC_ALPHA_SAT?
                            D3D12_BLEND_ZERO:factor;
                };
                rt.SrcBlend=rgb_destination_alpha(rt.SrcBlend);
                rt.DestBlend=rgb_destination_alpha(rt.DestBlend);
            }
            // Match vk_graphics_pipeline: GCN supplies alpha=1 to color
            // blending when CB_SHADER_MASK suppresses the alpha export.
            // Native shaders can otherwise supply zero for that component.
            if(!(regs.color_shader_mask.GetMask(i)&AmdGpu::ColorBufferMask::ComponentA)) {
                const auto default_alpha=[](D3D12_BLEND factor) {
                    return factor==D3D12_BLEND_SRC_ALPHA?D3D12_BLEND_ONE:
                        factor==D3D12_BLEND_INV_SRC_ALPHA?D3D12_BLEND_ZERO:factor;
                };
                rt.SrcBlend=default_alpha(rt.SrcBlend);
                rt.DestBlend=default_alpha(rt.DestBlend);
            }
            NormalizeMinMaxBlend(rt.BlendOp,rt.SrcBlend,rt.DestBlend,regs.color_buffers[i].GetNumberFmt());
            NormalizeMinMaxBlend(rt.BlendOpAlpha,rt.SrcBlendAlpha,rt.DestBlendAlpha,regs.color_buffers[i].GetNumberFmt());
        }
    }
    // Ordered targets are UAV-only. D3D12 rejects a nonzero RTV count when
    // every slot in that trailing range is UNKNOWN, while lower ordinary MRT
    // slots still require their original indices.
    while (p.NumRenderTargets && p.RTVFormats[p.NumRenderTargets - 1] == DXGI_FORMAT_UNKNOWN)
        --p.NumRenderTargets;
    // D3D12 forbids logic operations with IndependentBlendEnable. Packed SNORM
    // targets with distinct masks use the ordered UAV path above; remaining
    // fixed-function targets are exact only when every bound target has the same
    // scattered mask. Do not replay side-effecting guest shaders to emulate it.
    // Regression: snorm-color-owned and signed-logic (real GCN/readback);
    // native_guest_graphics_resources rejects unsupported combinations.
    const D3D12_RENDER_TARGET_BLEND_DESC* logic_state=nullptr;
    for(UINT i=0;i<p.NumRenderTargets;++i)
        if(targets[i]!=DXGI_FORMAT_UNKNOWN&&p.BlendState.RenderTarget[i].LogicOpEnable){
            logic_state=&p.BlendState.RenderTarget[i];break;
        }
    if(logic_state){
        const auto common=*logic_state;
        for(UINT i=0;i<p.NumRenderTargets;++i)if(targets[i]!=DXGI_FORMAT_UNKNOWN){
            const auto& target=p.BlendState.RenderTarget[i];
            if(target.RenderTargetWriteMask!=common.RenderTargetWriteMask||
               target.LogicOpEnable!=common.LogicOpEnable||target.LogicOp!=common.LogicOp)
                throw std::runtime_error("Native MRT logic operations require ordered independent write masks");
        }
        p.BlendState.IndependentBlendEnable=FALSE;p.BlendState.RenderTarget[0]=common;
    }
    if(!native_alpha_factor&&needs_constant_color_rgb&&needs_constant_alpha_rgb)
        throw std::runtime_error("Native MRT blend simultaneously requires constant color and constant alpha RGB factors");
    result.blend_constant_alpha_rgb=needs_constant_alpha_rgb&&!native_alpha_factor;
    auto& z=p.DepthStencilState;const auto& source=regs.depth_control;
    // These DB compatibility bits have no Vulkan pipeline equivalent and the
    // reference backend deliberately leaves normal color/depth state intact.
    // Treat them identically here instead of terminating the guest draw.
    const auto tests=GuestDepthTests(regs);
    result.depth_bounds=tests.bounds;
    if(result.depth_bounds) {
        if(depth==DXGI_FORMAT_UNKNOWN)throw std::runtime_error("Native depth bounds need a depth attachment");
        if(!std::isfinite(regs.depth_bounds_min)||!std::isfinite(regs.depth_bounds_max))
            throw std::runtime_error("Non-finite guest depth bounds require explicit semantics");
        result.rasterizer_discard=regs.depth_bounds_min>regs.depth_bounds_max;
        result.depth_bounds_min=regs.depth_bounds_min;
        result.depth_bounds_max=regs.depth_bounds_max;
    }
    z.DepthEnable=tests.depth;
    z.DepthWriteMask=tests.depth&&source.depth_write_enable&&!regs.depth_render_control.depth_clear_enable?
        D3D12_DEPTH_WRITE_MASK_ALL:D3D12_DEPTH_WRITE_MASK_ZERO;
    z.DepthFunc=tests.depth?D3D12_COMPARISON_FUNC(UINT(source.depth_func)+1):D3D12_COMPARISON_FUNC_ALWAYS;
    z.StencilEnable=tests.stencil;
    z.FrontFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_ALWAYS};z.BackFace=z.FrontFace;
    if((z.DepthEnable||z.StencilEnable)&&depth==DXGI_FORMAT_UNKNOWN)throw std::runtime_error("Native guest depth/stencil test has no attachment");
    if(z.StencilEnable) {
        if(depth!=DXGI_FORMAT_D24_UNORM_S8_UINT&&depth!=DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
            throw std::runtime_error("Native guest stencil test requires a stencil-capable attachment");
        const auto& front=regs.stencil_ref_front;const auto& back=source.backface_enable?regs.stencil_ref_back:front;
        z.StencilReadMask=front.stencil_mask;z.StencilWriteMask=front.stencil_write_mask;
        result.stencil_read_mask_front=front.stencil_mask;result.stencil_write_mask_front=front.stencil_write_mask;
        result.stencil_read_mask_back=back.stencil_mask;result.stencil_write_mask_back=back.stencil_write_mask;
        const auto& ctl=regs.stencil_control;
        StencilReference front_reference,back_reference;
        const auto face=[&](AmdGpu::StencilRefMask ref,AmdGpu::CompareFunc compare,
            AmdGpu::StencilFunc fail,AmdGpu::StencilFunc zfail,AmdGpu::StencilFunc pass,StencilReference& reference,UINT8& write_mask){
            using C=AmdGpu::CompareFunc;
            using F=AmdGpu::StencilFunc;
            // The pinned renderer uses op_val for both comparison and every
            // replacement when any of the face's original operations is ReplaceOp.
            if(fail==F::ReplaceOp||zfail==F::ReplaceOp||pass==F::ReplaceOp)
                ref.stencil_test_val=ref.stencil_op_val;
            if(regs.depth_render_control.stencil_clear_enable)ref.stencil_write_mask=0;
            if(compare==C::Always)fail=F::Keep;
            if(compare==C::Never)zfail=pass=F::Keep;
            if(!z.DepthEnable||z.DepthFunc==D3D12_COMPARISON_FUNC_ALWAYS)zfail=F::Keep;
            if(z.DepthEnable&&z.DepthFunc==D3D12_COMPARISON_FUNC_NEVER)pass=F::Keep;
            // Narrow only bits untouched by EVERY reachable outcome. This
            // makes AND/OR/XOR/XNOR exact without affecting other branches.
            write_mask=UINT8(StencilChangedBits(fail,ref)|StencilChangedBits(zfail,ref)|StencilChangedBits(pass,ref));
            ref.stencil_write_mask=write_mask;
            if(compare!=C::Always&&compare!=C::Never)reference.Require(ref.stencil_test_val,ref.stencil_mask);
            D3D12_DEPTH_STENCILOP_DESC result{D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC(UINT(compare)+1)};
            if(compare!=C::Always)result.StencilFailOp=StencilOp(fail,ref,reference);
            if(compare!=C::Never){
                if(z.DepthEnable&&z.DepthFunc!=D3D12_COMPARISON_FUNC_ALWAYS)result.StencilDepthFailOp=StencilOp(zfail,ref,reference);
                if(!z.DepthEnable||z.DepthFunc!=D3D12_COMPARISON_FUNC_NEVER)result.StencilPassOp=StencilOp(pass,ref,reference);
            }return result;
        };
        z.FrontFace=face(front,source.stencil_ref_func,ctl.stencil_fail_front,ctl.stencil_zfail_front,ctl.stencil_zpass_front,front_reference,result.stencil_write_mask_front);
        z.BackFace=source.backface_enable?face(back,source.stencil_bf_func,ctl.stencil_fail_back,ctl.stencil_zfail_back,ctl.stencil_zpass_back,back_reference,result.stencil_write_mask_back):z.FrontFace;
        if(!source.backface_enable){back_reference=front_reference;result.stencil_write_mask_back=result.stencil_write_mask_front;}
        z.StencilWriteMask=result.stencil_write_mask_front;
        result.stencil_reference=front_reference.value;result.stencil_reference_back=back_reference.value;
        result.separate_stencil_faces=front.stencil_mask!=back.stencil_mask||
            result.stencil_write_mask_front!=result.stencil_write_mask_back||front_reference.value!=back_reference.value;
    }
    auto& r=p.RasterizerState;const auto& poly=regs.polygon_control;
    if(poly.NeedsBias()) {
        const bool front=poly.enable_polygon_offset_front;
        result.depth_bias=front?regs.poly_offset.front_offset:regs.poly_offset.back_offset;
        result.depth_bias_clamp=regs.poly_offset.depth_bias;
        result.slope_depth_bias=(front?regs.poly_offset.front_scale:regs.poly_offset.back_scale)/16.f;
        if(!std::isfinite(result.depth_bias)||!std::isfinite(result.depth_bias_clamp)||!std::isfinite(result.slope_depth_bias))
            throw std::runtime_error("Non-finite guest polygon depth bias");
        // Preserve fractional guest constants with native dynamic depth bias;
        // integral constants retain the cheaper traditional rasterizer PSO.
        const double bias=result.depth_bias;
        result.dynamic_depth_bias=bias<INT_MIN||bias>INT_MAX||std::trunc(bias)!=bias;
        if(result.dynamic_depth_bias)p.Flags=D3D12_PIPELINE_STATE_FLAGS(p.Flags|D3D12_PIPELINE_STATE_FLAG_DYNAMIC_DEPTH_BIAS);
        else {r.DepthBias=INT(bias);r.DepthBiasClamp=result.depth_bias_clamp;r.SlopeScaledDepthBias=result.slope_depth_bias;}
    }
    r.FillMode=poly.PolyMode()==AmdGpu::PolygonMode::Line?D3D12_FILL_MODE_WIREFRAME:D3D12_FILL_MODE_SOLID;
    r.CullMode=poly.cull_front?D3D12_CULL_MODE_FRONT:poly.cull_back?D3D12_CULL_MODE_BACK:D3D12_CULL_MODE_NONE;
    r.FrontCounterClockwise=poly.front_face==AmdGpu::FrontFace::CounterClockwise;
    r.DepthClipEnable=!regs.clipper_control.clip_disable&&regs.clipper_control.ZclipEnable();r.MultisampleEnable=p.SampleDesc.Count>1;
    result.rasterizer_discard|=regs.clipper_control.dx_rasterization_kill;
    using P=AmdGpu::PrimitiveType;
    switch(regs.primitive_type) {
    case P::None:result.discard=true;break;
    case P::PointList:p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;result.topology=D3D_PRIMITIVE_TOPOLOGY_POINTLIST;break;
    case P::LineList:p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;result.topology=D3D_PRIMITIVE_TOPOLOGY_LINELIST;break;
    case P::LineStrip:p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;result.topology=D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;break;
    case P::TriangleList:p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;result.topology=D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;break;
    case P::TriangleFan:case P::Polygon:case P::QuadStrip:
        p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;result.topology=D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;break;
    case P::TriangleStrip:p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;result.topology=D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;break;
    case P::LineLoop:p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;result.topology=D3D_PRIMITIVE_TOPOLOGY_LINELIST;break;
    case P::RectList:case P::QuadList:
        p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
        result.topology=regs.primitive_type==P::RectList?D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST:
            D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST;break;
    case P::AdjLineList:p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;result.topology=D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;break;
    case P::AdjLineStrip:p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;result.topology=D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;break;
    case P::AdjTriangleList:p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;result.topology=D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;break;
    case P::AdjTriangleStrip:p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;result.topology=D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;break;
    case P::PatchPrimitive:
        if(!patch_points||patch_points>32)throw std::out_of_range("Native guest patch control point count");
        p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;result.topology=D3D_PRIMITIVE_TOPOLOGY(UINT(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST)+patch_points-1);break;
    default:throw std::runtime_error("Unknown native guest primitive topology");
    }
    // Culling/fill apply to primitives reaching rasterization, not the IA
    // input. A GS may emit lines from triangles.
    // Vulkan leaves this distinction to fixed-function rasterization; our
    // cull-both emulation must classify the final output before discarding.
    const bool polygon_input=p.PrimitiveTopologyType==D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE||
        p.PrimitiveTopologyType==D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
    bool rasterizes_polygons=polygon_input;
    if(regs.stage_enable.gs_en)
        rasterizes_polygons=regs.vgt_gs_out_prim_type.GetPrimitiveType(0)==AmdGpu::GsOutputPrimitiveType::TriangleStrip;
    else if(regs.stage_enable.hs_en&&regs.tess_config.type==AmdGpu::TessellationType::Isoline)
        rasterizes_polygons=false;
    // Match LiverpoolToVK::IsPrimitiveCulled: screen-aligned RectList is
    // never culled, even though its native expansion uses triangle patches.
    const bool cullable=polygon_input&&regs.primitive_type!=P::RectList&&rasterizes_polygons;
    result.rasterizer_discard|=cullable&&poly.cull_front&&poly.cull_back;
    if(!cullable)r.CullMode=D3D12_CULL_MODE_NONE;
    if(!rasterizes_polygons)r.FillMode=D3D12_FILL_MODE_SOLID;
    if(poly.PolyMode()==AmdGpu::PolygonMode::Point&&rasterizes_polygons&&!result.rasterizer_discard&&!result.discard)
        throw std::runtime_error("Guest point polygon fill requires native clipping and point rasterization");
    if(result.rasterizer_discard)p.StreamOutput.RasterizedStream=D3D12_SO_NO_RASTERIZED_STREAM;
    if(regs.enable_primitive_restart) {
        if(result.topology==D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP||result.topology==D3D_PRIMITIVE_TOPOLOGY_LINESTRIP){
            const bool narrow=regs.index_buffer_type.index_type==AmdGpu::IndexType::Index16;
            p.IBStripCutValue=narrow?D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF:D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFFFFFF;
        }
    }
    return result;
}
}
