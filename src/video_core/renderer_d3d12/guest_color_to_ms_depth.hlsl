// SPDX-License-Identifier: GPL-2.0-or-later
// Pinned color_to_ms_depth.frag: typed color channels become depth samples.
// Keep native format conversion (including sRGB), default component mapping
// and per-sample invocations. Upstream undecorated UV interpolation has an
// implementation-defined position; choose the native sample position here.
// This is not a raw storage transfer.
Texture2DArray<float4> Color : register(t0);
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
    return Color.Load(int4(coord, 0, 0))[sample_id];
}
