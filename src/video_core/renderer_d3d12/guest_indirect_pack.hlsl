// SPDX-License-Identifier: GPL-2.0-or-later
ByteAddressBuffer source : register(t0);
ByteAddressBuffer source_count : register(t1);
ByteAddressBuffer runtime_template : register(t2);
ByteAddressBuffer index_source : register(t3);
RWByteAddressBuffer commands : register(u0);
RWByteAddressBuffer runtimes : register(u1);
RWByteAddressBuffer packed_count : register(u2);
RWByteAddressBuffer dispatch_arguments : register(u3);
RWByteAddressBuffer decoded_indices : register(u4);
cbuffer Parameters : register(b0) {
    uint capacity,source_stride,indexed,runtime_stride;
    uint command_stride,runtime_low,runtime_high,has_count;
    uint row_pitch,index_available,index_swap,index_views;
    uint index_offset,index_low,index_high,index_stride;
    uint index_groups;
};
[numthreads(1,1,1)]
void build_dispatch() {
    uint active=has_count?min(capacity,source_count.Load(0)):capacity;
    packed_count.Store(0,active);
    uint groups=(active+127)/128;
    uint rows=max(1,(groups+65534)/65535);
    uint columns=(groups+rows-1)/rows;
    // Indirect root constant plus DISPATCH. Balanced rows avoid launching an
    // almost empty65535-group tail row at the first X-limit boundary.
    dispatch_arguments.Store4(0,uint4(columns*128,columns,rows,1));
    groups=active*index_groups;
    rows=max(1,(groups+65534)/65535);
    columns=(groups+rows-1)/rows;
    dispatch_arguments.Store4(16,uint4(columns*128,columns,rows,1));
}
[numthreads(128,1,1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint active=packed_count.Load(0);
    uint command=id.x+id.y*row_pitch;
    if(command>=active)return;
    uint input_offset=command*source_stride;
    uint output_offset=command*command_stride;
    uint runtime_offset=command*runtime_stride;
    uint prefix=runtime_stride?8:0;
    if(runtime_stride){
        uint low=runtime_low+runtime_offset;
        commands.Store2(output_offset,uint2(low,runtime_high+(low<runtime_low?1:0)));
    }
    if(index_views){
        uint first=source.Load(input_offset+8);
        uint available=min(source.Load(input_offset),index_available-min(first,index_available));
        uint address=index_low+command*index_stride;
        // Limit each IBV to readable indices. The IA supplies zero outside that
        // view; never clamp the guest draw count or read the next command's slot.
        // Direct3D functional specification8.19.2 (Addressing Bounds).
        commands.Store4(output_offset+prefix,uint4(address,index_high+(address<index_low?1:0),available*4,42));
        prefix+=16; // D3D12_INDEX_BUFFER_VIEW, R32_UINT
    }
    uint words=indexed?5:4;
    for(uint command_word=0;command_word<words;++command_word)
        commands.Store(output_offset+prefix+command_word*4,source.Load(input_offset+command_word*4));
    if(index_views)commands.Store(output_offset+prefix+8,0); // private view starts at guest firstIndex
    // Copy all viewport/flip/depth fields and padding without float conversion.
    if(runtime_stride){
    for(uint runtime_word=0;runtime_word<runtime_stride/4;++runtime_word)
        runtimes.Store(runtime_offset+runtime_word*4,runtime_template.Load(runtime_word*4));
    runtimes.Store(runtime_offset,source.Load(input_offset+(indexed?12:8)));
    runtimes.Store(runtime_offset+4,source.Load(input_offset+(indexed?16:12)));
    runtimes.Store(runtime_offset+8,indexed);
    runtimes.Store(runtime_offset+16,runtime_template.Load(16)+command);
    }
}

// Each command has a bounded private IBV. In particular, an odd final halfword
// is not paired with the first halfword outside that command's readable range.
[numthreads(128,1,1)]
void decode_indices(uint3 id : SV_DispatchThreadID) {
    uint flat=id.x+id.y*row_pitch;
    uint lanes=index_groups*128;
    uint command=flat/lanes,element=flat%lanes;
    if(command>=packed_count.Load(0))return;
    uint input_offset=command*source_stride;
    uint first=source.Load(input_offset+8);
    uint available=min(source.Load(input_offset),index_available-min(first,index_available));
    if(element>=available)return;
    uint relative=element*2;
    uint value=0;
    for(uint byte_index=0;byte_index<2;++byte_index){
        uint original=relative+byte_index;
        uint selected=((original&~3u)+4<=available*2)?original^(index_swap==2?3u:2u):original;
        uint address=index_offset+first*2+selected;
        value|=((index_source.Load(address&~3u)>>((address&3)*8))&255)<<(byte_index*8);
    }
    decoded_indices.Store(command*index_stride+element*4,value);
}
