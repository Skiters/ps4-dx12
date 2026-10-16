// SPDX-License-Identifier: MIT
#define A_GPU 1
#define A_HLSL 1
#define A_HLSL_6_2 1
#define A_HALF 1
#include "ffx_a.h"
cbuffer FsrConstants:register(b0){uint4 Const0;uint4 Const1;uint4 Const2;uint4 Const3;uint4 Sample;};
Texture2D<float4> InputTexture:register(t0);
RWTexture2D<float4> OutputTexture:register(u0);
SamplerState InputSampler:register(s0);
#if SAMPLE_EASU
#define FSR_EASU_H 1
AH4 FsrEasuRH(AF2 p){return AH4(InputTexture.GatherRed(InputSampler,p));}
AH4 FsrEasuGH(AF2 p){return AH4(InputTexture.GatherGreen(InputSampler,p));}
AH4 FsrEasuBH(AF2 p){return AH4(InputTexture.GatherBlue(InputSampler,p));}
#endif
#if SAMPLE_RCAS
#define FSR_RCAS_H 1
AH4 FsrRcasLoadH(ASW2 p){return AH4(InputTexture.Load(int3(int2(p),0)));}
void FsrRcasInputH(inout AH1 r,inout AH1 g,inout AH1 b){}
#endif
#include "ffx_fsr1.h"
void CurrFilter(AU2 pos){
    uint width,height;OutputTexture.GetDimensions(width,height);
    if(pos.x>=width||pos.y>=height)return;
    AH3 color;
#if SAMPLE_EASU
    FsrEasuH(color,pos,Const0,Const1,Const2,Const3);
#endif
#if SAMPLE_RCAS
    FsrRcasH(color.r,color.g,color.b,pos,Const0);
#endif
    if(Sample.x==1)color*=color;
    OutputTexture[pos]=float4(color,1);
}
[numthreads(64,1,1)]void main(uint local:SV_GroupIndex,uint3 group:SV_GroupID){
    AU2 pos=ARmp8x8(local)+AU2(group.x<<4,group.y<<4);
    CurrFilter(pos);pos.x+=8;CurrFilter(pos);pos.y+=8;CurrFilter(pos);pos.x-=8;CurrFilter(pos);
}
