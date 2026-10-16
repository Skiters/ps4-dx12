// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_graphics_state.h"
#include <cstdio>
using namespace NativeDX12;
namespace {
UINT Guest(AmdGpu::StencilFunc op,UINT old,UINT operand) {
    using F=AmdGpu::StencilFunc;
    switch(op) {
    case F::And:return old&operand;
    case F::Or:return old|operand;
    case F::Xor:return old^operand;
    case F::Xnor:return ~(old^operand);
    case F::Nand:return ~(old&operand);
    case F::Nor:return ~(old|operand);
    default:throw std::runtime_error("Unexpected test operation");
    }
}
UINT Native(D3D12_STENCIL_OP op,UINT old,UINT reference) {
    switch(op) {
    case D3D12_STENCIL_OP_KEEP:return old;
    case D3D12_STENCIL_OP_ZERO:return 0;
    case D3D12_STENCIL_OP_REPLACE:return reference;
    case D3D12_STENCIL_OP_INVERT:return ~old;
    default:throw std::runtime_error("Unexpected lowered operation");
    }
}
void Require(bool value,const char* text) {if(!value)throw std::runtime_error(text);}
}
int main(){try {
    AmdGpu::Regs regs{};
    regs.depth_buffer.stencil_read_base=1;
    regs.depth_buffer.stencil_info.format=AmdGpu::DepthBuffer::StencilFormat::Stencil8;
    regs.depth_control.stencil_enable=1;
    regs.depth_control.stencil_ref_func=AmdGpu::CompareFunc::Always;
    using F=AmdGpu::StencilFunc;
    UINT checked{};
    for(auto op:{F::And,F::Or,F::Xor,F::Xnor,F::Nand,F::Nor})
    for(UINT operand=0;operand<256;++operand)
    for(UINT mask:{0u,1u,0x55u,0xaau,0x0fu,0xf0u,0xffu}) {
        // NAND/NOR need mixed invert/constant behavior when the operand has
        // both zero and one bits in the write mask: no single native op.
        const bool supported=(op!=F::Nand&&op!=F::Nor)||
            !(operand&mask)||(operand&mask)==mask;
        regs.stencil_ref_front={0,255,u8(mask),u8(operand)};
        regs.stencil_control.stencil_zpass_front=op;
        GuestGraphicsState state;
        try {state=BuildGuestGraphicsState(regs,{},DXGI_FORMAT_D32_FLOAT_S8X24_UINT);}
        catch(const std::runtime_error& e) {
            Require(!supported,"Representable operation was rejected");
            Require(std::string_view(e.what()).starts_with("Native stencil needs ordered emulation:"),"Wrong rejection reason");
            continue;
        }
        Require(supported,"Unrepresentable operation silently accepted");
        const auto& z=state.pipeline.DepthStencilState;
        Require(!state.separate_stencil_faces,"Disabled back-face state diverged");
        for(UINT old=0;old<256;++old) {
            const UINT expected=((old&~mask)|(Guest(op,old,operand)&mask))&255;
            const UINT actual=((old&~z.StencilWriteMask)|
                (Native(z.FrontFace.StencilPassOp,old,state.stencil_reference)&z.StencilWriteMask))&255;
            Require(actual==expected,"Bitwise stencil byte mismatch");
            ++checked;
        }
    }
    // The narrowed masks differ even though the original guest masks match.
    regs.depth_control.backface_enable=1;regs.depth_control.stencil_bf_func=AmdGpu::CompareFunc::Always;
    regs.stencil_ref_front={0,255,255,0x55};regs.stencil_ref_back=regs.stencil_ref_front;
    regs.stencil_control.stencil_zpass_front=F::And;regs.stencil_control.stencil_zpass_back=F::Or;
    auto state=BuildGuestGraphicsState(regs,{},DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
    Require(state.separate_stencil_faces&&state.stencil_write_mask_front==0xaa&&
        state.stencil_write_mask_back==0x55,"Independent lowered face masks lost");
    // A second reachable branch must not inherit a mask narrowed for AND.
    regs.depth_control.backface_enable=0;regs.depth_control.stencil_ref_func=AmdGpu::CompareFunc::Equal;
    regs.stencil_ref_front.stencil_mask=255;
    regs.stencil_control.stencil_fail_front=F::Invert;
    bool rejected=false;
    try {(void)BuildGuestGraphicsState(regs,{},DXGI_FORMAT_D32_FLOAT_S8X24_UINT);}
    catch(const std::runtime_error& e) {rejected=std::string_view(e.what()).starts_with("Native stencil needs ordered emulation:");}
    Require(rejected,"Different branch write masks silently corrupted stencil");
    // Never/always branches cannot constrain the effective native mask.
    regs.depth_control.stencil_ref_func=AmdGpu::CompareFunc::Always;
    state=BuildGuestGraphicsState(regs,{},DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
    Require(state.stencil_write_mask_front==0xaa,"Unreachable stencil-fail operation constrained mask");
    regs.stencil_ref_front={0x11,255,255,0x77};
    regs.depth_control.stencil_ref_func=AmdGpu::CompareFunc::Equal;
    regs.stencil_control.stencil_fail_front=F::ReplaceOp;
    regs.stencil_control.stencil_zfail_front=F::ReplaceTest;
    regs.stencil_control.stencil_zpass_front=F::ReplaceTest;
    state=BuildGuestGraphicsState(regs,{},DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
    Require(state.stencil_reference==0x77&&state.pipeline.DepthStencilState.FrontFace.StencilFunc==D3D12_COMPARISON_FUNC_EQUAL&&
        state.pipeline.DepthStencilState.FrontFace.StencilPassOp==D3D12_STENCIL_OP_REPLACE,
        "Pinned ReplaceOp reference was not shared by comparison and ReplaceTest");
    regs.depth_control.stencil_ref_func=AmdGpu::CompareFunc::Always;
    state=BuildGuestGraphicsState(regs,{},DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
    Require(state.stencil_reference==0x77,"Unreachable ReplaceOp changed the original face reference selection");
    regs.depth_render_control.stencil_clear_enable=1;
    state=BuildGuestGraphicsState(regs,{},DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
    Require(!state.stencil_write_mask_front&&!state.stencil_write_mask_back&&!state.WritesDepthStencil(),
        "Explicit stencil clear allowed a subsequent raster stencil write");
    std::printf("PASS: %u exact stencil byte comparisons; independent faces and incompatible branch masks\n",checked);
    return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
