// SPDX-License-Identifier: GPL-2.0-or-later
ByteAddressBuffer input_count : register(t0);
RWByteAddressBuffer draw_arguments : register(u0);
cbuffer StreamRange : register(b0) { uint vertex_stride; uint first_vertex; uint vertex_limit; uint output_multiplier; uint primitive_width; };
// BufferFilledSize is a hardware-produced32-bit byte counter. The owner proves
// its stream capacity below4GiB and passes complete primitive-aligned ranges.
// Every nonempty range is replayed, in order; this is bounded partitioning,
// not clamping the total guest workload or replacing it with CPU counts.
[numthreads(1,1,1)]
void main() {
    // Replay conversion instead reads an indirect vertex count, divides by
    // primitive width and multiplies by invocation/emission slots. CPU-side
    // plan validation proves the complete output product fits uint32.
    uint vertices=input_count.Load(0)/vertex_stride;
    uint available=vertices>first_vertex?vertices-first_vertex:0;
    draw_arguments.Store4(0,uint4(min(available,vertex_limit)*output_multiplier,1,first_vertex,0));
}
[numthreads(1,1,1)]
void partition() {
    uint vertices=input_count.Load(0)/vertex_stride;
    uint available=vertices>first_vertex?vertices-first_vertex:0;
    uint active=min(available,vertex_limit);
    draw_arguments.Store4(0,uint4(active,1,first_vertex,0));
    uint records=(active/primitive_width)*output_multiplier;
    draw_arguments.Store4(16,uint4(records,1,0,0));
    draw_arguments.Store3(32,uint3((records+255)/256,1,1));
}
