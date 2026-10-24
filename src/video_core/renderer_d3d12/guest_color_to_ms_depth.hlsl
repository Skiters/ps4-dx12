// SPDX-License-Identifier: GPL-2.0-or-later
// Pinned color_to_ms_depth.frag: typed color channels become depth samples.
// Keep native format conversion (including sRGB), default component mapping
// and per-sample invocations. Upstream undecorated UV interpolation has an
// implementation-defined position; choose the native sample position here.
// This is not a raw storage transfer.
#if RAW_COLOR
Texture2DArray<uint> Color : register(t0);
float4 DecodeColor(uint word) {
#if DATA_FORMAT == 4
    return float4(asfloat(word), 0, 0, 1);
#elif DATA_FORMAT == 34
    return float4(float3(word & 511, (word >> 9) & 511, (word >> 18) & 511) *
        exp2(float(int(word >> 27) - 24)), 1);
#elif DATA_FORMAT == 9
#if NUMBER_FORMAT == 1
    int4 value = int4(int(word << 22) >> 22, int(word << 12) >> 22,
        int(word << 2) >> 22, int(word) >> 30);
    return max(float4(value) / float4(511, 511, 511, 1), -1.0);
#else
    return float4(word & 1023, (word >> 10) & 1023, (word >> 20) & 1023,
        word >> 30) / float4(1023, 1023, 1023, 3);
#endif
#elif DATA_FORMAT == 10
#if NUMBER_FORMAT == 1
    return max(float4(int(word << 24) >> 24, int(word << 16) >> 24,
        int(word << 8) >> 24, int(word) >> 24) / 127.0, -1.0);
#else
    float4 value = float4(word & 255, (word >> 8) & 255, (word >> 16) & 255, word >> 24) / 255.0;
#if NUMBER_FORMAT == 9
    // The raw UAV stores encoded sRGB bytes. The upstream float sampler
    // decodes RGB only; alpha stays linear and guest swizzles are not used.
    [unroll] for (uint c = 0; c < 3; ++c)
        value[c] = value[c] <= 0.04045 ? value[c] / 12.92 : pow((value[c] + 0.055) / 1.055, 2.4);
#endif
    return value;
#endif
#else
#error Unsupported raw color depth codec
#endif
}
#else
Texture2DArray<float4> Color : register(t0);
#endif
struct Input {
    float4 position : SV_Position;
    sample noperspective float2 uv : TEXCOORD0;
};
float main(Input input, uint sample_id : SV_SampleIndex) : SV_Depth {
    // Upstream vec4[SampleID] is undefined above channel three. Still execute
    // the original MSAA draw; choose a defined zero instead of indexing beyond
    // the vector on hosts which support 8/16 samples.
    if (sample_id >= 4) return 0;
    uint width, height, layers, levels;
    Color.GetDimensions(0, width, height, layers, levels);
    uint2 coord = uint2(input.uv * float2(width, height));
#if RAW_COLOR
    return DecodeColor(Color.Load(int4(coord, 0, 0)))[sample_id];
#else
    return Color.Load(int4(coord, 0, 0))[sample_id];
#endif
}
