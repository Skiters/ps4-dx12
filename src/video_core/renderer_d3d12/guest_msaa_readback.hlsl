// SPDX-License-Identifier: GPL-2.0-or-later
Texture2DMSArray<TEXTURE_TYPE> source : register(t0);
RWBuffer<VALUE_TYPE> output : register(u0);
cbuffer Params : register(b0) { uint width; uint height; uint samples; uint pitch; };
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x < width && id.y < height && id.z < samples) {
        TEXTURE_TYPE value = source.Load(int3(id.xy, 0), id.z);
#if PACKED_MODE == 1
        uint3 bits = (uint3)round(saturate(value.rgb) * float3(31, 63, 31));
        output[(id.z * height + id.y) * pitch + id.x] = uint4((bits.r << 11) | (bits.g << 5) | bits.b, 0, 0, 0);
#elif PACKED_MODE == 2
        uint4 bits = (uint4)round(saturate(value) * float4(31, 31, 31, 1));
        output[(id.z * height + id.y) * pitch + id.x] = uint4((bits.a << 15) | (bits.r << 10) | (bits.g << 5) | bits.b, 0, 0, 0);
#elif PACKED_MODE == 3
        uint4 bits = (uint4)round(saturate(value) * 15);
        output[(id.z * height + id.y) * pitch + id.x] = uint4((bits.a << 12) | (bits.r << 8) | (bits.g << 4) | bits.b, 0, 0, 0);
#else
        output[(id.z * height + id.y) * pitch + id.x] = value;
#endif
    }
}
