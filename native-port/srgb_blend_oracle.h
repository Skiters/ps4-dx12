// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace NativeDX12::Tests {

inline constexpr std::array<float,4> SrgbHdrSource{2.f,-2.f,.5f,.25f};
inline constexpr std::uint8_t SrgbBlendBefore[2][4]{{64,128,192,64},{2,10,11,129}};
inline constexpr std::uint8_t SrgbBlendAfter[4][3][4]{
    {{143,92,140,255},{55,205,102,255},{44,92,226,255}},
    {{137,10,11,129},{2,188,11,255},{1,5,188,255}},
    {{167,65,101,255},{47,232,51,255},{29,65,241,255}},
    {{143,92,209,64},{194,92,166,64},{192,92,190,64}},
};

// Independent double-precision API equations validate the literal code tables.
// No renderer packing routine, shader emitter or GPU observation is used here.
inline void CheckSrgbBlendOracle() {
    const auto clamp=[](double x){return std::clamp(x,0.,1.);};
    const auto decode=[](double x){return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);};
    const auto encode=[](double x){return x<=.0031308?x*12.92:1.055*std::pow(x,1./2.4)-.055;};
    const auto code=[&](double x){
        const double scaled=clamp(x)*255.,lower=std::floor(scaled),fraction=scaled-lower;
        return unsigned(lower+(fraction>.5||(fraction==.5&&(unsigned(lower)&1))));
    };
    for(unsigned byte=0;byte<256;++byte)
        if(code(encode(decode(double(byte)/255.)))!=byte)
            throw std::runtime_error("Independent sRGB byte roundtrip failed");
    unsigned missing_decode{}, final_only{}, alpha_gamma{};
    for(unsigned point=0;point<4;++point)for(unsigned target=0;target<3;++target){
        const auto evaluate=[&](bool skip_decode,bool skip_intermediate,bool encode_alpha){
            std::array<unsigned,4> result{};std::array<double,4> destination{};
            for(unsigned channel=0;channel<4;++channel){
                result[channel]=SrgbBlendBefore[point==1][channel];
                const double value=double(result[channel])/255.;
                destination[channel]=(channel<3&&!skip_decode)?decode(value):value;
            }
            const unsigned fragments=point==2?2:1;
            for(unsigned fragment=0;fragment<fragments;++fragment)for(unsigned channel=0;channel<4;++channel){
                if(point==1&&!(std::array<unsigned,3>{1,10,15}[target]&(1u<<channel)))continue;
                const double source=point==3?SrgbHdrSource[channel]:channel==3||channel==target?1.:0.;
                const double constant=std::array<double,4>{.25,.5,.75,.5}[channel];
                const double src_factor=channel==3?1.:target==0?constant:.5;
                const double dst_factor=channel==3?0.:target==1?1.-constant:.5;
                const double linear=clamp(clamp(source)*src_factor+destination[channel]*dst_factor);
                result[channel]=code((channel<3||encode_alpha)?encode(linear):linear);
                const double stored=double(result[channel])/255.;
                destination[channel]=skip_intermediate?linear:
                    channel<3&&!skip_decode?decode(stored):stored;
            }
            return result;
        };
        const auto correct=evaluate(false,false,false),wrong_decode=evaluate(true,false,false),
            wrong_store=evaluate(false,true,false),wrong_alpha=evaluate(false,false,true);
        for(unsigned channel=0;channel<4;++channel){
            if(correct[channel]!=SrgbBlendAfter[point][target][channel])
                throw std::runtime_error("sRGB literal blend oracle disagrees with independent API equation");
            missing_decode+=wrong_decode[channel]!=correct[channel];
            final_only+=wrong_store[channel]!=correct[channel];
            alpha_gamma+=wrong_alpha[channel]!=correct[channel];
        }
    }
    if(!missing_decode||!final_only||!alpha_gamma)
        throw std::runtime_error("sRGB oracle lost a decode/per-fragment-store/linear-alpha negative control");
}

} // namespace NativeDX12::Tests
