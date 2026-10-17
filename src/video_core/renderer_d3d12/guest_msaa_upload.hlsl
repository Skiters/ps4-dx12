// SPDX-License-Identifier: GPL-2.0-or-later
Buffer<VALUE_TYPE> source : register(t0);
cbuffer Params : register(b0) {
    uint width; uint height; uint samples; uint pitch;
#if STENCIL_BIT_UPLOAD
    uint stencil_bit;
#endif
};
#if NATIVE_STENCIL
#if STENCIL_BIT_UPLOAD
// A fixed-function write mask supplies the bit. Sample-frequency discard
// selects exactly the samples whose source bit is one, without SV_StencilRef.
// Mask zero is the rectangle-only initialization pass for partial uploads.
void main(float4 position : SV_Position, uint sample : SV_SampleIndex) {
    uint value=source[(sample * height + uint(position.y)) * pitch + uint(position.x)].x;
    if(stencil_bit!=0 && (value & stencil_bit)==0) discard;
}
#else
uint main(float4 position : SV_Position, uint sample : SV_SampleIndex) : SV_StencilRef {
    return source[(sample * height + uint(position.y)) * pitch + uint(position.x)].x;
}
#endif
#elif NATIVE_DEPTH
float main(float4 position : SV_Position, uint sample : SV_SampleIndex) : SV_Depth {
    return source[(sample * height + uint(position.y)) * pitch + uint(position.x)].x;
}
#else
TEXTURE_TYPE main(float4 position : SV_Position, uint sample : SV_SampleIndex) : SV_Target {
    VALUE_TYPE value = source[(sample * height + uint(position.y)) * pitch + uint(position.x)];
#if PACKED_MODE == 1
    uint word = value.x;
    return float4(float((word >> 11) & 31) / 31, float((word >> 5) & 63) / 63, float(word & 31) / 31, 1);
#elif PACKED_MODE == 2
    uint word = value.x;
    return float4(float((word >> 10) & 31) / 31, float((word >> 5) & 31) / 31, float(word & 31) / 31, float(word >> 15));
#elif PACKED_MODE == 3
    uint word = value.x;
    return float4(float((word >> 8) & 15) / 15, float((word >> 4) & 15) / 15, float(word & 15) / 15, float(word >> 12));
#else
    return value;
#endif
}
#endif
