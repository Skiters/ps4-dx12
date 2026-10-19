// SPDX-License-Identifier: GPL-2.0-or-later
ByteAddressBuffer stream_filled_bytes : register(t0);
RWByteAddressBuffer draw_arguments : register(u0);
cbuffer StreamRange : register(b0) { uint vertex_stride; uint first_vertex; uint vertex_limit; };
// BufferFilledSize is a hardware-produced32-bit byte counter. The owner proves
// its stream capacity below4GiB and passes complete primitive-aligned ranges.
// Every nonempty range is replayed, in order; this is bounded partitioning,
// not clamping the total guest workload or replacing it with CPU counts.
[numthreads(1,1,1)]
void main() {
    uint vertices=stream_filled_bytes.Load(0)/vertex_stride;
    uint available=vertices>first_vertex?vertices-first_vertex:0;
    draw_arguments.Store4(0,uint4(min(available,vertex_limit),1,first_vertex,0));
}
