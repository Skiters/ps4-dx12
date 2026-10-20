// SPDX-License-Identifier: GPL-2.0-or-later
RWByteAddressBuffer records : register(u0, space33);
ByteAddressBuffer actual_count : register(t0, space33);
cbuffer ClearParameters : register(b0, space33) { uint record_count; uint record_bytes; uint indirect_count; uint count_offset; };
// The hardware count determines dispatched work, including an empty slice.
// This entry binds the argument packet as u0 instead of the capture arena.
[numthreads(1, 1, 1)]
void build_dispatch() {
    uint count=min(record_count,actual_count.Load(count_offset));
    records.Store3(0,uint3((count+255)/256,1,1));
}
// Only validity tags need clearing. Payload is consumed after the producer
// publishes a nonzero tag and the whole UAV pass has completed.
[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint count=indirect_count?min(record_count,actual_count.Load(count_offset)):record_count;
    if (id.x < count) records.Store(id.x * record_bytes, 0);
}
