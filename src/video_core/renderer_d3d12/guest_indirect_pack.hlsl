// SPDX-License-Identifier: GPL-2.0-or-later
ByteAddressBuffer source : register(t0);
ByteAddressBuffer source_count : register(t1);
ByteAddressBuffer runtime_template : register(t2);
RWByteAddressBuffer commands : register(u0);
RWByteAddressBuffer runtimes : register(u1);
RWByteAddressBuffer packed_count : register(u2);
RWByteAddressBuffer dispatch_arguments : register(u3);
cbuffer Parameters : register(b0) {
    uint capacity,source_stride,indexed,runtime_stride;
    uint command_stride,runtime_low,runtime_high,has_count;
};
[numthreads(1,1,1)]
void build_dispatch() {
    uint active=has_count?min(capacity,source_count.Load(0)):capacity;
    packed_count.Store(0,active);
    dispatch_arguments.Store3(0,uint3((active+127)/128,1,1));
}
[numthreads(128,1,1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint active=packed_count.Load(0);
    if(id.x>=active)return;
    uint input_offset=id.x*source_stride;
    uint output_offset=id.x*command_stride;
    uint runtime_offset=id.x*runtime_stride;
    uint low=runtime_low+runtime_offset;
    commands.Store2(output_offset,uint2(low,runtime_high+(low<runtime_low?1:0)));
    uint words=indexed?5:4;
    for(uint command_word=0;command_word<words;++command_word)
        commands.Store(output_offset+8+command_word*4,source.Load(input_offset+command_word*4));
    // Copy all viewport/flip/depth fields and padding without float conversion.
    for(uint runtime_word=0;runtime_word<runtime_stride/4;++runtime_word)
        runtimes.Store(runtime_offset+runtime_word*4,runtime_template.Load(runtime_word*4));
    runtimes.Store(runtime_offset,source.Load(input_offset+(indexed?12:8)));
    runtimes.Store(runtime_offset+4,source.Load(input_offset+(indexed?16:12)));
    runtimes.Store(runtime_offset+8,indexed);
    runtimes.Store(runtime_offset+16,runtime_template.Load(16)+id.x);
}
