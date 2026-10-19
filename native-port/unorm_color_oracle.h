// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
namespace NativeDX12::Tests {
struct Unorm8Color {
    uint8_t nearest,low,high;
    Unorm8Color(uint8_t exact):nearest(exact),low(exact),high(exact){}
    static Unorm8Color FromFloat(float value){
        if(!std::isfinite(value))throw std::invalid_argument("Non-finite color oracle input");
        const double scaled=double(std::clamp(value,0.f,1.f))*255.;
        Unorm8Color result(uint8_t(std::floor(scaled+.5)));
        // Pinned Vulkan uses fixed-function UNORM storage, not an explicit
        // roundEven shader conversion. Vulkan Fundamentals permits floor/ceil
        // (exact integers/endpoints stay exact); D3D FLOAT->UNORM further limits
        // integer-side error to 0.6 ULP. Intersect BOTH contracts, never +/-1.
        // https://docs.vulkan.org/spec/latest/chapters/fundamentals.html#fundamentals-fixedfpconv
        // https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#3.2.3.6%20FLOAT%20-%3E%20UNORM
        constexpr double tolerance=double(.6f);
        result.low=uint8_t(std::max(std::floor(scaled),std::ceil(scaled-tolerance)));
        result.high=uint8_t(std::min(std::ceil(scaled),std::floor(scaled+tolerance)));
        return result;
    }
    bool Accepts(uint8_t value)const{return value>=low&&value<=high;}
};
inline bool MatchesColorAllocation(std::span<const std::byte> actual,std::span<const std::byte> expected,
    std::span<const size_t> color_red_bytes,Unorm8Color color){
    if(actual.size()!=expected.size())return false;
    for(size_t i=0;i<actual.size();++i)if(actual[i]!=expected[i]){
        if(!color.Accepts(uint8_t(actual[i]))||
           std::find(color_red_bytes.begin(),color_red_bytes.end(),i)==color_red_bytes.end())return false;
    }
    return true;
}
inline void CheckUnormColorOracleNegatives(){
    const auto require=[](bool ok){if(!ok)throw std::runtime_error("UNORM oracle contract/negative failed");};
    const auto half=Unorm8Color::FromFloat(.5f),quarter=Unorm8Color::FromFloat(.25f);
    require(half.Accepts(127)&&half.Accepts(128)&&!half.Accepts(126)&&!half.Accepts(129));
    require(quarter.Accepts(64)&&!quarter.Accepts(63)&&!quarter.Accepts(65));
    require(Unorm8Color::FromFloat(0.f).low==0&&Unorm8Color::FromFloat(0.f).high==0);
    require(Unorm8Color::FromFloat(1.f).low==255&&Unorm8Color::FromFloat(1.f).high==255);
    std::byte expected[]{std::byte{128},std::byte{0},std::byte{0},std::byte{255},std::byte{0x39}};
    std::byte actual[]{std::byte{127},std::byte{0},std::byte{0},std::byte{255},std::byte{0x39}};
    const size_t red[]{0};require(MatchesColorAllocation(actual,expected,red,half));
    actual[4]=std::byte{0x38};require(!MatchesColorAllocation(actual,expected,red,half));
    actual[4]=expected[4];actual[3]=std::byte{254};require(!MatchesColorAllocation(actual,expected,red,half));
}
}
