// SPDX-License-Identifier: GPL-2.0-or-later
RWByteAddressBuffer records : register(u0, space33);
cbuffer ClearParameters : register(b0, space33) { uint record_count; uint record_bytes; };
// Only validity tags need clearing. Payload is consumed after the producer
// publishes a nonzero tag and the whole UAV pass has completed.
[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x < record_count) records.Store(id.x * record_bytes, 0);
}
