// SPDX-License-Identifier: GPL-2.0-or-later
Buffer<VALUE_TYPE> source : register(t0);
cbuffer Params : register(b0) { uint width; uint height; uint samples; uint pitch; };
#if NATIVE_STENCIL
uint main(float4 position : SV_Position, uint sample : SV_SampleIndex) : SV_StencilRef {
    return source[(sample * height + uint(position.y)) * pitch + uint(position.x)].x;
}
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
