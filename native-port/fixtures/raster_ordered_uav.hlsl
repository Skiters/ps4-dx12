[[vk::binding(0, 0)]] RWTexture2D<uint> ordered_target;

float4 main(float4 position : SV_Position) : SV_Target0 {
    const uint2 pixel = uint2(position.xy);
    const uint previous = ordered_target[pixel];
    ordered_target[pixel] = previous ^ 0x12345678u;
    return 0.0f;
}
