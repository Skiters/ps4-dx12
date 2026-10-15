#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_image_readback.h"
#include "guest_cmask_address.h"
#include "guest_color_clear.h"
#include "guest_raw_color_clear.h"
#include "video_core/amdgpu/regs_color.h"
#include "video_core/amdgpu/cb_db_extent.h"
#include <dxgi1_6.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
using namespace NativeDX12;
void Require(bool b){if(!b)throw std::runtime_error("CMASK test assertion failed");}
using ClearSwap=AmdGpu::ColorBuffer::SwapMode;
AmdGpu::ColorBuffer ClearBuffer(uint32_t format,uint32_t number,uint32_t word0,uint32_t word1=0,
    ClearSwap swap=ClearSwap::Standard){
    AmdGpu::ColorBuffer b{};b.info.format=format;b.info.number_type=number;
    b.info.comp_swap=swap;b.clear_word0=word0;b.clear_word1=word1;return b;
}
GuestColorClearValue CheckClearPixel(const char* name,const AmdGpu::ColorBuffer& b,
    std::initializer_list<uint8_t> expected,bool rtv_compatible=true){
    const auto clear=DecodeGuestColorClear(b);
    if(!clear||clear->pixel_bytes!=expected.size()||clear->rtv_compatible!=rtv_compatible)
        throw std::runtime_error(std::string("CMASK codec classification: ")+name);
    size_t offset{};
    for(const auto value:expected)if(clear->pixel[offset++]!=std::byte(value))
        throw std::runtime_error(std::string("CMASK codec fixed pixel: ")+name);
    return *clear;
}
void CheckColorClearCodec(){
    // Fixed bytes derived from the pinned original-format clear and component
    // tables, never from the decoder under test. Numeric inputs are checked
    // separately from byte packing and GPU-path eligibility.
    CheckClearPixel("RGBA8 Standard",ClearBuffer(10,0,0x80402010),{0x10,0x20,0x40,0x80});
    CheckClearPixel("RGBA8 Alternate",ClearBuffer(10,0,0x80402010,0,ClearSwap::Alternate),
        {0x40,0x20,0x10,0x80});
    CheckClearPixel("RGBA8 StandardReverse",ClearBuffer(10,0,0x80402010,0,ClearSwap::StandardReverse),
        {0x80,0x40,0x20,0x10});
    CheckClearPixel("RGBA8 AlternateReverse",ClearBuffer(10,0,0x80402010,0,ClearSwap::AlternateReverse),
        {0x80,0x10,0x20,0x40});
    auto clear=CheckClearPixel("RG16 Alternate alpha",ClearBuffer(5,0,0x24681357,0,ClearSwap::Alternate),
        {0x57,0x13,0xff,0xff});
    Require(clear.rtv[1]==1.f);
    clear=CheckClearPixel("RGBA16 UNORM",ClearBuffer(12,0,0xffff0000,0x12348000),
        {0,0,0xff,0xff,0,0x80,0x34,0x12});
    Require(clear.rtv[0]==0.f&&clear.rtv[1]==1.f&&std::abs(clear.rtv[2]-0.5000076295f)<1.e-7f);
    clear=CheckClearPixel("RGBA8 SNORM canonical minimum",ClearBuffer(10,1,0x7f008180),
        {0x81,0x81,0,0x7f});
    Require(clear.rtv[0]==-1.f&&clear.rtv[1]==-1.f&&clear.rtv[2]==0.f&&clear.rtv[3]==1.f);
    clear=CheckClearPixel("RG16 SNORM canonical minimum",ClearBuffer(5,1,0x7fff8000),
        {1,0x80,0xff,0x7f});
    Require(clear.rtv[0]==-1.f&&clear.rtv[1]==1.f);

    clear=CheckClearPixel("R8 UINT narrows before RTV",ClearBuffer(1,4,0x101),{1});
    Require(clear.rtv[0]==1.f);
    clear=CheckClearPixel("R16 UINT narrows before RTV",ClearBuffer(2,4,0x12348001),{1,0x80});
    Require(clear.rtv[0]==32769.f);
    clear=CheckClearPixel("R8 sign-extended SINT",ClearBuffer(1,5,0xffffffff),{0xff});
    Require(clear.rtv[0]==-1.f);
    clear=CheckClearPixel("R16 sign-extended SINT minimum",ClearBuffer(2,5,0xffff8000),{0,0x80});
    Require(clear.rtv[0]==-32768.f);
    CheckClearPixel("RG8 defined SINT",ClearBuffer(3,5,0x7f01),{1,0x7f});
    Require(!DecodeGuestColorClear(ClearBuffer(1,5,0xff)));
    Require(!DecodeGuestColorClear(ClearBuffer(3,5,0xff7f)));
    Require(!DecodeGuestColorClear(ClearBuffer(4,0,0x12345678)));

    clear=CheckClearPixel("R32 exact UINT above 2^24",ClearBuffer(4,4,0x01000002),{2,0,0,1});
    Require(clear.rtv[0]==16777218.f);
    CheckClearPixel("R32 inexact UINT retained",ClearBuffer(4,4,0x01000001),{1,0,0,1},false);
    CheckClearPixel("R32 UINT maximum retained",ClearBuffer(4,4,0xffffffff),{0xff,0xff,0xff,0xff},false);
    CheckClearPixel("R32 SINT negative boundary",ClearBuffer(4,5,0xff000000),{0,0,0,0xff});
    CheckClearPixel("R32 large negative SINT retained",ClearBuffer(4,5,0xfe000000),{0,0,0,0xfe},false);
    CheckClearPixel("R32 SINT minimum retained",ClearBuffer(4,5,0x80000000),{0,0,0,0x80},false);
    CheckClearPixel("RG32 preserves both integer words",ClearBuffer(11,4,0x01234567,0xfedcba98),
        {0x67,0x45,0x23,1,0x98,0xba,0xdc,0xfe},false);
    CheckClearPixel("RGBA32 replication then swap",ClearBuffer(14,4,2,9,ClearSwap::AlternateReverse),
        {9,0,0,0,2,0,0,0,2,0,0,0,2,0,0,0});

    clear=CheckClearPixel("RGBA16 finite half incl subnormal and negative zero",
        ClearBuffer(12,7,0xbc003c00,0x80000001),{0,0x3c,0,0xbc,1,0,0,0x80});
    Require(clear.rtv[0]==1.f&&clear.rtv[1]==-1.f&&clear.rtv[2]==0x1p-24f);
    Require(clear.rtv[3]==0.f&&std::signbit(clear.rtv[3]));
    clear=CheckClearPixel("R16 maximum finite half",ClearBuffer(2,7,0x7bff),{0xff,0x7b});
    Require(clear.rtv[0]==65504.f);
    CheckClearPixel("R16 half infinity raw fallback",ClearBuffer(2,7,0x7c00),{0,0x7c},false);
    CheckClearPixel("R16 half NaN payload raw fallback",ClearBuffer(2,7,0x7e01),{1,0x7e},false);
    clear=CheckClearPixel("RG32 float negative zero and one",ClearBuffer(11,7,0x80000000,0x3f800000),
        {0,0,0,0x80,0,0,0x80,0x3f});
    Require(clear.rtv[0]==0.f&&std::signbit(clear.rtv[0])&&clear.rtv[1]==1.f);
    CheckClearPixel("R32 smallest normal float",ClearBuffer(4,7,0x00800000),{0,0,0x80,0});
    CheckClearPixel("R32 float denormal raw fallback",ClearBuffer(4,7,1),{1,0,0,0},false);
    CheckClearPixel("R32 float NaN payload raw fallback",ClearBuffer(4,7,0x7fc01234),{0x34,0x12,0xc0,0x7f},false);
    CheckClearPixel("R32 float infinity raw fallback",ClearBuffer(4,7,0x7f800000),{0,0,0x80,0x7f},false);

    // CB sRGB encoding is 6, not the generic NumberFormat::Srgb value 9.
    // Fixed ideal transfer values: linear byte 1 -> 13, 32 -> 99, 64 -> 137.
    // CPU packing uses these exact bytes; this is not a GPU sRGB rounding oracle.
    clear=CheckClearPixel("RGBA8 SRGB reference conversion",ClearBuffer(10,6,0x80402001),
        {13,99,137,128});
    Require(clear.number_format==9&&std::abs(clear.rtv[0]-0.0039215686f)<1.e-8f&&
        std::abs(clear.rtv[1]-0.1254901961f)<1.e-7f&&
        std::abs(clear.rtv[2]-0.2509803922f)<1.e-7f&&
        std::abs(clear.rtv[3]-0.5019607843f)<1.e-7f);
    CheckClearPixel("RGBA8 SRGB swap before transfer",ClearBuffer(10,6,0x80402001,0,ClearSwap::Alternate),
        {137,99,13,128});
    CheckClearPixel("RGBA8 SRGB zero",ClearBuffer(10,6,0),{0,0,0,0});
    CheckClearPixel("RGBA8 SRGB one",ClearBuffer(10,6,0xffffffff),{255,255,255,255});
}
void CheckPackedColorClearCodec(){
    CheckClearPixel("original8 UINT moves low A2 to native high A2",ClearBuffer(8,4,1),{0,0,0,0x40});
    auto clear=CheckClearPixel("original9 UINT narrows swizzled alpha",ClearBuffer(9,4,4,0,ClearSwap::StandardReverse),
        {0,0,0,0});
    Require(clear.rtv[3]==0.f);
    // Original9 fields are [1023,0,341,1]. UNORM conversion therefore starts
    // from [1,0,1/3,1/3]; the final A2/R10 widths apply AFTER each fixed swap.
    CheckClearPixel("original9 UNORM Standard",ClearBuffer(9,0,0x555003ff),{0xff,3,0x50,0x55});
    CheckClearPixel("original9 UNORM Alternate",ClearBuffer(9,0,0x555003ff,0,ClearSwap::Alternate),
        {0x55,1,0xf0,0x7f});
    clear=CheckClearPixel("original9 UNORM StandardReverse",ClearBuffer(9,0,0x555003ff,0,ClearSwap::StandardReverse),
        {0x55,0x55,5,0xc0});
    Require(std::abs(clear.rtv[0]-0.3333333333f)<1.e-7f&&clear.rtv[3]==1.f);
    CheckClearPixel("original9 UNORM AlternateReverse",ClearBuffer(9,0,0x555003ff,0,ClearSwap::AlternateReverse),
        {0x55,0xfd,0x0f,0x40});
    // Original8 fields are [1,341,0,1023], with original widths [2,10,10,10].
    // These constants also detect decoding the remapped format a second time.
    CheckClearPixel("original8 UNORM Standard",ClearBuffer(8,0,0xffc00555),{0xff,3,0x50,0x55});
    CheckClearPixel("original8 UNORM Alternate",ClearBuffer(8,0,0xffc00555,0,ClearSwap::Alternate),
        {0xff,0x57,0x55,0x15});
    CheckClearPixel("original8 UNORM StandardReverse",ClearBuffer(8,0,0xffc00555,0,ClearSwap::StandardReverse),
        {0x55,0x55,5,0xc0});
    CheckClearPixel("original8 UNORM AlternateReverse",ClearBuffer(8,0,0xffc00555,0,ClearSwap::AlternateReverse),
        {0,0x54,0x55,0xd5});
}
void CheckPacked16ColorClearCodec(){
    // Original16 fields [5/31,17/63,23/31,1]. The native/Vulkan physical
    // offsets are R11/G5/B0; Alternate selects the initialized alpha as R.
    CheckClearPixel("original16 UNORM Standard",ClearBuffer(16,0,0xba25),{0x25,0xba});
    auto clear=CheckClearPixel("original16 UNORM Alternate",ClearBuffer(16,0,0xba25,0,ClearSwap::Alternate),
        {0x25,0xfa});
    Require(clear.rtv[0]==1.f&&std::abs(clear.rtv[1]-17.f/63.f)<1.e-7f);
    CheckClearPixel("original16 UNORM StandardReverse",ClearBuffer(16,0,0xba25,0,ClearSwap::StandardReverse),
        {0x37,0x2a});
    CheckClearPixel("original16 UNORM AlternateReverse",ClearBuffer(16,0,0xba25,0,ClearSwap::AlternateReverse),
        {0x3f,0x2a});
    // Original17 fields [5/31,10/31,20/31,1]. Reversed swaps move values
    // across 1/5-bit channels, so quantization must follow the component map.
    CheckClearPixel("original17 UNORM Standard",ClearBuffer(17,0,0xd145),{0x45,0xd1});
    CheckClearPixel("original17 UNORM Alternate",ClearBuffer(17,0,0xd145,0,ClearSwap::Alternate),
        {0x54,0x95});
    clear=CheckClearPixel("original17 UNORM StandardReverse",ClearBuffer(17,0,0xd145,0,ClearSwap::StandardReverse),
        {0x9f,0x2a});
    Require(clear.rtv[2]==1.f&&std::abs(clear.rtv[3]-5.f/31.f)<1.e-7f);
    CheckClearPixel("original17 UNORM AlternateReverse",ClearBuffer(17,0,0xd145,0,ClearSwap::AlternateReverse),
        {0xbf,0xa8});
    // Original18 fields [1,5/31,10/31,20/31]. The original clear/swizzle
    // is retained even though attachment storage is remapped to original17.
    clear=CheckClearPixel("original18 UNORM Standard",ClearBuffer(18,0,0xa28b),{0xaa,0xfc});
    Require(clear.data_format==17&&clear.rtv[0]==1.f);
    CheckClearPixel("original18 UNORM Alternate",ClearBuffer(18,0,0xa28b,0,ClearSwap::Alternate),
        {0xbf,0xa8});
    CheckClearPixel("original18 UNORM StandardReverse",ClearBuffer(18,0,0xa28b,0,ClearSwap::StandardReverse),
        {0x45,0xd1});
    CheckClearPixel("original18 UNORM AlternateReverse",ClearBuffer(18,0,0xa28b,0,ClearSwap::AlternateReverse),
        {0xe5,0x53});
    // Original19 guest packing is A0/R4/G8/B12. The native transfer reverses
    // physical nibbles, so the RTV tuple remains the pinned logical RGBA.
    clear=CheckClearPixel("original19 UNORM Standard",ClearBuffer(19,0,0x4321),{0x21,0x43});
    Require(std::abs(clear.rtv[0]-2.f/15.f)<1.e-7f&&
        std::abs(clear.rtv[1]-3.f/15.f)<1.e-7f&&
        std::abs(clear.rtv[2]-4.f/15.f)<1.e-7f&&
        std::abs(clear.rtv[3]-1.f/15.f)<1.e-7f);
    CheckClearPixel("original19 UNORM Alternate",ClearBuffer(19,0,0x4321,0,ClearSwap::Alternate),
        {0x23,0x41});
    CheckClearPixel("original19 UNORM StandardReverse",ClearBuffer(19,0,0x4321,0,ClearSwap::StandardReverse),
        {0x34,0x12});
    CheckClearPixel("original19 UNORM AlternateReverse",ClearBuffer(19,0,0x4321,0,ClearSwap::AlternateReverse),
        {0x14,0x32});
    // Preserve the reference's distinct high-word rules: format16 takes an
    // unmasked uint8_t U5 input, format17 tests all high bits for alpha,
    // while format18 masks its upper five-bit field explicitly.
    CheckClearPixel("original16 upper U5 saturates",ClearBuffer(16,0,0x10000),{0,0xf8});
    CheckClearPixel("original17 high word alpha",ClearBuffer(17,0,0x10000),{0,0x80});
    CheckClearPixel("original18 upper U5 masks",ClearBuffer(18,0,0x10000),{0,0});
    CheckClearPixel("original19 upper U4 saturates",ClearBuffer(19,0,0x10000),{0,0xf0});
    CheckClearPixel("original19 upper U4 truncates to uint8",ClearBuffer(19,0,0x100000),{0,0});
    Require(!DecodeGuestColorClear(ClearBuffer(16,4,0xba25)));
    Require(!DecodeGuestColorClear(ClearBuffer(17,1,0xd145)));
    Require(!DecodeGuestColorClear(ClearBuffer(18,4,0xa28b)));
    Require(!DecodeGuestColorClear(ClearBuffer(19,4,0x4321)));
}
void CheckPackedFloatColorClearCodec(){
    // Original6 fields encode [1.5,2,4]. Original7 has the same logical
    // fields with UF10 first, then UF11/UF11; its remap swaps R/B afterward.
    auto clear=CheckClearPixel("original6 finite Standard",ClearBuffer(6,7,0x882003e0,0xdeadbeef),
        {0xe0,3,0x20,0x88});
    Require(clear.data_format==6&&clear.rtv[0]==1.5f&&clear.rtv[1]==2.f&&clear.rtv[2]==4.f);
    clear=CheckClearPixel("original6 finite Alternate alpha",ClearBuffer(6,7,0x882003e0,0,ClearSwap::Alternate),
        {0xe0,3,0x20,0x78});
    Require(clear.rtv[2]==1.f);
    CheckClearPixel("original6 finite StandardReverse",ClearBuffer(6,7,0x882003e0,0,ClearSwap::StandardReverse),
        {0x40,4,0x20,0x7c});
    CheckClearPixel("original6 finite AlternateReverse",ClearBuffer(6,7,0x882003e0,0,ClearSwap::AlternateReverse),
        {0xc0,3,0x20,0x7c});
    clear=CheckClearPixel("original7 finite Standard",ClearBuffer(7,7,0x881001f0),{0x40,4,0x20,0x7c});
    Require(clear.data_format==6&&clear.rtv[0]==4.f&&clear.rtv[1]==2.f&&clear.rtv[2]==1.5f);
    CheckClearPixel("original7 finite Alternate alpha",ClearBuffer(7,7,0x881001f0,0,ClearSwap::Alternate),
        {0xc0,3,0x20,0x7c});
    CheckClearPixel("original7 finite StandardReverse",ClearBuffer(7,7,0x881001f0,0,ClearSwap::StandardReverse),
        {0xe0,3,0x20,0x88});
    CheckClearPixel("original7 finite AlternateReverse",ClearBuffer(7,7,0x881001f0,0,ClearSwap::AlternateReverse),
        {0xe0,3,0x20,0x78});

    clear=CheckClearPixel("original6 packed subnormals",ClearBuffer(6,7,0x00401001),{1,0x10,0x40,0});
    Require(clear.rtv[0]==0x1p-20f&&clear.rtv[1]==0x1p-19f&&clear.rtv[2]==0x1p-19f);
    clear=CheckClearPixel("original7 exact subnormal widening",ClearBuffer(7,7,0x00200801),{1,0x10,0x40,0});
    Require(clear.rtv[0]==0x1p-20f&&clear.rtv[2]==0x1p-19f);
    CheckClearPixel("original6 exact subnormal narrowing",ClearBuffer(6,7,2,0,ClearSwap::StandardReverse),
        {0,0,0x40,0});
    CheckClearPixel("original6 exact normal narrowing",ClearBuffer(6,7,0x3c2,0,ClearSwap::StandardReverse),
        {0,0,0x40,0x78});
    clear=CheckClearPixel("original6 maximum finite UF11",ClearBuffer(6,7,0x7bf),{0xbf,7,0,0});
    Require(clear.rtv[0]==65024.f);
    clear=CheckClearPixel("original6 exact maximum UF10",ClearBuffer(6,7,0x7be,0,ClearSwap::StandardReverse),
        {0,0,0xc0,0xf7});
    Require(clear.rtv[2]==64512.f);
    CheckClearPixel("original6 zero",ClearBuffer(6,7,0),{0,0,0,0});

    // These are explicit open cases, not a claim that historical fallback
    // already implements reference rounding or exceptional-value semantics.
    Require(!DecodeGuestColorClear(ClearBuffer(6,7,1,0,ClearSwap::StandardReverse)));
    Require(!DecodeGuestColorClear(ClearBuffer(6,7,0x3c1,0,ClearSwap::StandardReverse)));
    Require(!DecodeGuestColorClear(ClearBuffer(6,7,0x7bf,0,ClearSwap::StandardReverse)));
    Require(!DecodeGuestColorClear(ClearBuffer(7,7,0x00200801,0,ClearSwap::StandardReverse)));
    Require(!DecodeGuestColorClear(ClearBuffer(6,7,0x7c0)));
    Require(!DecodeGuestColorClear(ClearBuffer(6,7,0x7c1)));
    Require(!DecodeGuestColorClear(ClearBuffer(7,7,0x3e0)));
    Require(!DecodeGuestColorClear(ClearBuffer(7,7,0x3e1)));
    Require(!DecodeGuestColorClear(ClearBuffer(6,4,0x882003e0)));
    Require(!DecodeGuestColorClear(ClearBuffer(34,7,1)));
}
void CheckCpuClearPixel(AmdGpu::ColorBuffer b,std::initializer_list<uint8_t> pixel){
    b.base_address=0x10000;b.cmask_base_address=0x20000;b.info.fast_clear=1;
    b.attrib.tile_mode_index=AmdGpu::TileMode(13);b.pitch.tile_max=63;b.slice.tile_max=2047;
    b.view.slice_start=1;b.view.slice_max=2;b.cmask_slice.tile_max=15;
    AmdGpu::CbDbExtent extent{};extent.width=17;extent.height=9;
    const auto desc=DescribeGuestColorBuffer(b,extent);const auto layout=LayoutGuestTexture(desc);
    Require(pixel.size()==layout.bytes_per_pixel);
    std::vector<std::byte> color(layout.guest_bytes,std::byte{0xee}),expected=color;
    std::vector<std::byte> metadata(GuestCmaskBytes(b),std::byte{0xff});
    std::fill(metadata.begin()+2048,metadata.begin()+4096,std::byte{0});
    for(uint32_t y=0;y<desc.height;++y)for(uint32_t x=0;x<desc.width;++x){
        auto offset=GuestPixelOffset(desc,layout,0,1,x,y);
        for(auto value:pixel)expected[offset++]=std::byte(value);
    }
    Require(MaterializeGuestColorClear(b,desc,layout,color,metadata)&&color==expected);
    Require(std::all_of(metadata.begin(),metadata.end(),[](auto value){return value==std::byte{0xff};}));
    Require(!MaterializeGuestColorClear(b,desc,layout,color,metadata));
}
void CheckColorClearCpuIntegration(){
    CheckCpuClearPixel(ClearBuffer(5,0,0x24681357,0,ClearSwap::Alternate),{0x57,0x13,0xff,0xff});
    CheckCpuClearPixel(ClearBuffer(5,1,0x7fff8000),{1,0x80,0xff,0x7f});
    CheckCpuClearPixel(ClearBuffer(4,4,0x01000001),{1,0,0,1});
    CheckCpuClearPixel(ClearBuffer(4,7,1),{1,0,0,0});
    CheckCpuClearPixel(ClearBuffer(10,6,0x80402001),{13,99,137,128});
    // Undefined narrow SINT reference inputs retain the historical raw fallback.
    CheckCpuClearPixel(ClearBuffer(3,5,0xff7f),{0x7f,0xff});
    CheckCpuClearPixel(ClearBuffer(8,4,1),{0,0,0,0x40});
    CheckCpuClearPixel(ClearBuffer(9,4,4,0,ClearSwap::StandardReverse),{0,0,0,0});
    CheckCpuClearPixel(ClearBuffer(8,0,0xffc00555,0,ClearSwap::Alternate),{0xff,0x57,0x55,0x15});
    CheckCpuClearPixel(ClearBuffer(16,0,0xba25,0,ClearSwap::Alternate),{0x25,0xfa});
    CheckCpuClearPixel(ClearBuffer(17,0,0xd145,0,ClearSwap::StandardReverse),{0x9f,0x2a});
    CheckCpuClearPixel(ClearBuffer(18,0,0xa28b),{0xaa,0xfc});
    CheckCpuClearPixel(ClearBuffer(16,0,0x10000),{0,0xf8});
    CheckCpuClearPixel(ClearBuffer(19,0,0x4321,0,ClearSwap::Alternate),{0x23,0x41});
    CheckCpuClearPixel(ClearBuffer(19,0,0x10000),{0,0xf0});
    CheckCpuClearPixel(ClearBuffer(6,7,0x882003e0,0,ClearSwap::StandardReverse),{0x40,4,0x20,0x7c});
    CheckCpuClearPixel(ClearBuffer(7,7,0x881001f0,0,ClearSwap::Alternate),{0xc0,3,0x20,0x7c});
    CheckCpuClearPixel(ClearBuffer(7,7,0x00200801),{1,0x10,0x40,0});
}
void CheckRawColorClearGpu(ID3D12Device* device,ID3D12CommandQueue* queue){
    struct Case {
        const char* name;AmdGpu::ColorBuffer buffer;std::array<uint8_t,16> pixel;uint32_t bytes;
    };
    const Case cases[]={
        {"R32_FLOAT NaN payload",ClearBuffer(4,7,0x7fc01234),{0x34,0x12,0xc0,0x7f},4},
        {"R32_FLOAT denormal",ClearBuffer(4,7,1),{1,0,0,0},4},
        {"R32_UINT maximum",ClearBuffer(4,4,0xffffffff),{0xff,0xff,0xff,0xff},4},
        {"R32_SINT minimum",ClearBuffer(4,5,0x80000000),{0,0,0,0x80},4},
        {"RGBA16_FLOAT raw special values",ClearBuffer(12,7,0x7e018001,0x80007c00),
            {1,0x80,1,0x7e,0,0x7c,0,0x80},8},
    };
    GuestRawColorClear raw_clear(device);
    for(const auto& c:cases)for(const UINT samples:{1u,4u}){
        GuestTextureDesc desc;desc.address=0x100000;desc.width=19;desc.height=11;desc.pitch=32;
        desc.storage_height=16;desc.layers=desc.view_layers=2;desc.samples=samples;
        desc.data_format=uint32_t(c.buffer.GetDataFmt());desc.number_format=uint32_t(c.buffer.GetNumberFmt());
        desc.render_target=true;desc.tile_mode=8;
        const auto layout=LayoutGuestTexture(desc);
        const auto raw_format=desc.data_format==12?DXGI_FORMAT_R16G16B16A16_UINT:DXGI_FORMAT_R32_UINT;
        bool supported=true;
        for(const auto format:{layout.format,raw_format}){
            D3D12_FEATURE_DATA_FORMAT_SUPPORT support{format};
            Check(DX12_API_CALL(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support))));
            supported&=bool(support.Support1&D3D12_FORMAT_SUPPORT1_RENDER_TARGET);
            if(samples>1){
                D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS quality{
                    format,samples,D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE,0};
                Check(DX12_API_CALL(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,&quality,sizeof(quality))));
                supported&=quality.NumQualityLevels!=0;
                if(format==raw_format)supported&=bool(support.Support1&D3D12_FORMAT_SUPPORT1_MULTISAMPLE_LOAD);
            }
        }
        if(!supported){
            if(samples==1)throw std::runtime_error(std::string("Raw CMASK clear required format unsupported: ")+c.name);
            fprintf(stderr,"SKIP: raw CMASK clear %s at 4 samples: adapter format capability\n",c.name);
            continue;
        }
        const auto clear=DecodeGuestColorClear(c.buffer);
        Require(clear&&clear->pixel_bytes==c.bytes&&!clear->rtv_compatible);
        for(UINT byte=0;byte<c.bytes;++byte)Require(clear->pixel[byte]==std::byte(c.pixel[byte]));
        ComPtr<ID3D12CommandAllocator> allocator;
        Check(DX12_API_CALL(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))));
        ComPtr<ID3D12GraphicsCommandList> list;
        Check(DX12_API_CALL(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list))));
        std::vector<std::byte> initial(layout.guest_bytes,std::byte{0xee}),expected=initial;
        GuestTextureCache cache(device);auto& image=cache.Upload(desc,initial,list.Get(),0);
        const auto program=raw_clear.Prepare(image.texture.handle->GetDesc(),desc.data_format);
        if(!program)throw std::runtime_error(std::string("Raw CMASK clear program unavailable: ")+c.name);
        ComPtr<ID3D12DescriptorHeap> heap;D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
        heap_desc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;heap_desc.NumDescriptors=1;
        Check(DX12_API_CALL(device->CreateDescriptorHeap(&heap_desc,IID_PPV_ARGS(&heap))));
        auto view_desc=desc;view_desc.first_layer=1;view_desc.view_layers=1;
        auto rtv=image.Rtv(view_desc);rtv.Format=program->format;
        const auto handle=heap->GetCPUDescriptorHandleForHeapStart();
        DX12_API_CALL(device->CreateRenderTargetView(image.texture.handle.Get(),&rtv,handle));
        // One interior rectangle and one clipped logical edge on layer 1.
        // Layer 0, every other pixel and physical padding retain the sentinel.
        const D3D12_RECT rectangles[]={{3,2,13,7},{16,8,19,11}};
        image.texture.Transition(list.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET);
        GuestRawColorClear::Record(list.Get(),*program,handle,desc.width,desc.height,rectangles,*clear);
        image.pending=true;
        for(UINT sample=0;sample<samples;++sample)
        for(UINT y=0;y<desc.height;++y)for(UINT x=0;x<desc.width;++x)
            if((x>=3&&x<13&&y>=2&&y<7)||(x>=16&&y>=8)){
                const auto offset=GuestPixelOffset(desc,layout,0,1,x,y,sample);
                memcpy(expected.data()+offset,c.pixel.data(),c.bytes);
            }
        GuestImageReadback readback(device,image,list.Get());
        Check(DX12_API_CALL(list->Close()));Timeline timeline(device);
        const auto fence=timeline.Submit(queue,list.Get());cache.Submitted(fence);readback.Submitted(fence);
        timeline.Wait(fence);auto restored=initial;readback.Resolve(timeline.Completed(),restored);
        if(restored!=expected)throw std::runtime_error(std::string("Raw CMASK clear exact regions/samples/layers: ")+c.name+
            " samples="+std::to_string(samples));
    }
}
void CheckCmaskAddressing() {
    using P=AmdGpu::PipeConfig;
    struct Golden {P pipe;uint32_t x,y;uint64_t byte;uint32_t bit;};
    // Fixed per-pipe vectors from the pinned AddrLib equations; they are not
    // generated with the native layout helper. Every surface here is 1024x512.
    const Golden vectors[]={
        {P::P2,8,0,256,0},{P::P2,16,0,3,4},{P::P2,0,16,1,0},
        {P::P8_32x32_16x16,8,0,256,0},{P::P8_32x32_16x16,16,0,769,4},
        {P::P8_32x32_16x16,0,8,256,4},
        {P::P8_32x32_8x16,8,0,512,0},{P::P8_32x32_8x16,16,0,257,4},
        {P::P8_32x32_8x16,0,16,512,4},
        {P::P16_32x32_8x16,8,0,512,0},{P::P16_32x32_8x16,16,0,257,4},
        {P::P16_32x32_8x16,0,8,256,0},{P::P16_32x32_8x16,32,0,1024,0},
    };
    for(bool linear:{false,true})for(const auto& v:vectors){
        const GuestCmaskAddress address(1024,512,v.pipe,linear);
        Require(address.slice_bytes==4096);
        for(uint32_t layer:{0u,1u,2u}){
            const auto [byte,bit]=address.At(layer,v.x,v.y);
            Require(byte==uint64_t(layer)*4096+v.byte&&bit==v.bit);
        }
    }
    struct Boundary {P pipe;bool linear;uint32_t x,y;uint64_t byte;uint32_t bit;};
    const Boundary boundaries[]={
        {P::P2,false,256,0,128,0},{P::P2,true,256,0,32,0},
        {P::P8_32x32_16x16,false,512,0,128,0},
        {P::P8_32x32_16x16,true,512,0,16,0},
        {P::P16_32x32_8x16,false,512,0,128,0},
        {P::P16_32x32_8x16,true,512,0,8,0},
        {P::P16_32x32_8x16,false,0,32,2056,0},
        {P::P16_32x32_8x16,true,0,32,2064,0},
    };
    for(const auto& v:boundaries){
        const GuestCmaskAddress address(1024,512,v.pipe,v.linear);
        Require(address.At(1,v.x,v.y)==std::pair<uint64_t,uint32_t>{4096+v.byte,v.bit});
    }
    for(P pipe:{P::P2,P::P8_32x32_8x16,P::P8_32x32_16x16,P::P16_32x32_8x16})
    for(bool linear:{false,true})for(uint32_t width:{17u,129u,1024u})
    for(uint32_t rows:{9u,65u,512u}){
        const GuestCmaskAddress address(width,rows,pipe,linear);
        std::vector<bool> used(size_t(address.slice_bytes*2));
        for(uint32_t y=0;y<address.height;y+=8)for(uint32_t x=0;x<address.pitch;x+=8){
            const auto [byte,bit]=address.At(0,x,y);
            Require(bit==0||bit==4);
            const auto nibble=size_t(byte*2+bit/4);
            Require(nibble<used.size()&&!used[nibble]);used[nibble]=true;
            Require(address.At(2,x,y).first==address.slice_bytes*2+byte);
        }
    }
}
void CheckCmaskPartialSurfaces() {
    // The two zero nibbles are deliberately placed using fixed addresses:
    // (16,0) and (0,16). All other pixels, samples, layers and padding survive.
    struct Case {AmdGpu::TileMode tile;bool alt;uint64_t first,second;uint32_t second_bit;};
    const Case cases[]={
        {AmdGpu::TileMode::Thin1DThin,false,769,512,0},
        {AmdGpu::TileMode::DisplayThinPrt,false,257,512,4},
        {AmdGpu::TileMode::Thin1DThin,true,257,512,4},
    };
    for(const auto& c:cases)for(bool linear:{false,true})for(uint32_t log2_samples:{0u,2u})
    for(const auto swap:{ClearSwap::Standard,ClearSwap::AlternateReverse}){
        AmdGpu::ColorBuffer b{};b.base_address=0x10000;b.cmask_base_address=0x20000;
        b.info.format=10;b.info.fast_clear=1;b.info.cmask_is_linear=linear;
        b.info.alt_tile_mode=c.alt;b.attrib.tile_mode_index=c.tile;
        b.attrib.num_fragments_log2=log2_samples;b.pitch.tile_max=127;b.slice.tile_max=8191;
        b.view.slice_start=1;b.view.slice_max=2;b.cmask_slice.tile_max=31;
        b.clear_word0=0x80402010;
        b.info.comp_swap=swap;
        const uint32_t expected_pixel=swap==ClearSwap::Standard?0x80402010:0x40201080;
        AmdGpu::CbDbExtent extent{};extent.width=41;extent.height=25;
        auto desc=DescribeGuestColorBuffer(b,extent);desc.alt_tile=c.alt;
        const auto layout=LayoutGuestTexture(desc);
        std::vector<std::byte> color(layout.guest_bytes,std::byte{0xee}),expected=color;
        std::vector<std::byte> metadata(GuestCmaskBytes(b),std::byte{0xff});
        metadata[size_t(4096+c.first)]&=std::byte{0x0f};
        metadata[size_t(4096+c.second)]&=std::byte(c.second_bit?0x0f:0xf0);
        for(uint32_t sample=0;sample<desc.samples;++sample)
        for(uint32_t y=0;y<desc.height;++y)for(uint32_t x=0;x<desc.width;++x)
            if((x>=16&&x<24&&y<8)||(x<8&&y>=16&&y<24)){
                const auto offset=GuestPixelOffset(desc,layout,0,1,x,y,sample);
                memcpy(expected.data()+offset,&expected_pixel,4);
            }
        Require(MaterializeGuestColorClear(b,desc,layout,color,metadata));
        Require(color==expected);
        Require(std::all_of(metadata.begin(),metadata.end(),[](auto v){return v==std::byte{0xff};}));
        Require(!GuestColorClearPending(b,desc,layout,metadata));
    }
}
int main(){try {
    CheckColorClearCodec();CheckPackedColorClearCodec();CheckPacked16ColorClearCodec();
    CheckPackedFloatColorClearCodec();CheckColorClearCpuIntegration();
    CheckCmaskAddressing();CheckCmaskPartialSurfaces();
    AmdGpu::ColorBuffer b{}; b.base_address=0x10000; b.cmask_base_address=0x20000;
    b.info.format=10; b.info.fast_clear=1; b.attrib.tile_mode_index=AmdGpu::TileMode(13);
    b.pitch.tile_max=63; b.slice.tile_max=2047; b.view.slice_start=1; b.view.slice_max=2;
    b.clear_word0=0x80402010; b.cmask_slice.tile_max=15;
    AmdGpu::CbDbExtent extent{}; extent.width=17; extent.height=9;
    auto desc=DescribeGuestColorBuffer(b,extent); auto layout=LayoutGuestTexture(desc);
    Require(GuestCmaskBytes(b)==6144);
    std::vector<std::byte> color(layout.guest_bytes,std::byte{0xee}), metadata(6144,std::byte{0xff});
    // Layer 1: first 8x8 tile is a clear (zero nibble), adjacent tile is expanded.
    metadata[2048]=std::byte{0xf0};
    Require(MaterializeGuestColorClear(b,desc,layout,color,metadata));
    const auto clear=GuestPixelOffset(desc,layout,0,1,0,0), expanded=GuestPixelOffset(desc,layout,0,1,8,0);
    Require(memcmp(color.data()+clear,&b.clear_word0,4)==0);
    Require(color[expanded]==std::byte{0xee});
    Require(metadata[2048]==std::byte{0xff});
    Require(!MaterializeGuestColorClear(b,desc,layout,color,metadata));
    // An untouched view layer remains untouched.
    std::fill(metadata.begin(),metadata.begin()+2048,std::byte{0});
    Require(!GuestColorClearPending(b,desc,layout,metadata));
    std::fill(metadata.begin()+2048,metadata.begin()+4096,std::byte{0xff});
    for(uint32_t log2_samples=1;log2_samples<=4;++log2_samples){
        auto msaa=b;msaa.attrib.num_fragments_log2=log2_samples;
        const auto msaa_desc=DescribeGuestColorBuffer(msaa,extent);
        const auto msaa_layout=LayoutGuestTexture(msaa_desc);
        std::vector<std::byte> samples(msaa_layout.guest_bytes,std::byte{0xee});
        std::vector<std::byte> masks(6144,std::byte{0xff});masks[2048]=std::byte{0xf0};
        Require(MaterializeGuestColorClear(msaa,msaa_desc,msaa_layout,samples,masks));
        Require(std::count_if(samples.begin(),samples.end(),[](auto value){return value!=std::byte{0xee};})==
            8*8*4*msaa_desc.samples);
        for(uint32_t layer=0;layer<3;++layer){
            const auto decoded=DecodeGuestSubresource(msaa_desc,msaa_layout,samples,0,layer);
            for(uint32_t sample=0;sample<msaa_desc.samples;++sample)
            for(uint32_t y=0;y<9;++y)for(uint32_t x=0;x<17;++x){
                uint32_t actual{};memcpy(&actual,decoded.data()+((sample*9+y)*17+x)*4,4);
                Require(actual==(layer==1&&x<8&&y<8?msaa.clear_word0:0xeeeeeeee));
            }
        }
        Require(!MaterializeGuestColorClear(msaa,msaa_desc,msaa_layout,samples,masks));
        std::fill(samples.begin(),samples.end(),std::byte{0xee});
        std::fill(masks.begin()+2048,masks.begin()+4096,std::byte{0});
        Require(MaterializeGuestColorClear(msaa,msaa_desc,msaa_layout,samples,masks));
        Require(std::count_if(samples.begin(),samples.end(),[](auto value){return value!=std::byte{0xee};})==
            17*9*4*msaa_desc.samples);
        const auto decoded=DecodeGuestSubresource(msaa_desc,msaa_layout,samples,0,1);
        for(size_t offset=0;offset<decoded.size();offset+=4){
            uint32_t actual{};memcpy(&actual,decoded.data()+offset,4);Require(actual==msaa.clear_word0);
        }
    }
    ComPtr<IDXGIFactory6> factory; Check(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))));
    ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1>a;auto hr=DX12_API_CALL(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&a)));if(hr==DXGI_ERROR_NOT_FOUND)break;Check(hr);DXGI_ADAPTER_DESC1 ad{};Check(DX12_API_CALL(a->GetDesc1(&ad)));if(ad.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;if(SUCCEEDED(DX12_API_CALL(D3D12CreateDevice(a.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)))))break;}
    Require(device!=nullptr); ComPtr<ID3D12CommandQueue> queue; D3D12_COMMAND_QUEUE_DESC q{}; Check(DX12_API_CALL(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))));
    ComPtr<ID3D12CommandAllocator> allocator; Check(DX12_API_CALL(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))));
    ComPtr<ID3D12GraphicsCommandList> list; Check(DX12_API_CALL(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list))));
    GuestTextureCache cache(device.Get()); auto& entry=cache.Upload(desc,color,list.Get(),0); GuestImageReadback readback(device.Get(),entry,list.Get());
    Check(DX12_API_CALL(list->Close())); Timeline timeline(device.Get()); auto fence=timeline.Submit(queue.Get(),list.Get()); cache.Submitted(fence); readback.Submitted(fence); timeline.Wait(fence); std::vector<std::byte> restored=color; readback.Resolve(timeline.Completed(),restored);
    Require(restored==color);
    CheckRawColorClearGpu(device.Get(),queue.Get());
    puts("PASS: mixed CMASK tile equation, exact changed/unchanged pixels, idempotence, untouched layer and D3D12 readback"); return 0;
} catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
