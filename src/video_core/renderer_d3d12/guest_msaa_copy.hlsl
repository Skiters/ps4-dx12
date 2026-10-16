// SPDX-License-Identifier: GPL-2.0-or-later
#if SOURCE_MSAA
Texture2DMSArray<VALUE_TYPE> source : register(t0);
#else
Texture2DArray<VALUE_TYPE> source : register(t0);
#endif
VALUE_TYPE main(float4 position : SV_Position, uint sample : SV_SampleIndex) : SV_Target {
#if SOURCE_MSAA
    return source.Load(int3(uint2(position.xy), 0), sample);
#else
    return source.Load(int4(uint2(position.xy), 0, 0));
#endif
}
