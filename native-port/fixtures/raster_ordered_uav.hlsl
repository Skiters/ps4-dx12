[[vk::binding(0, 0)]] RWTexture2D<uint> ordered_target;

void main(float4 position : SV_Position) {
    const uint2 pixel = uint2(position.xy);
    const uint previous = ordered_target[pixel];
    ordered_target[pixel] = previous + 1u;
}
