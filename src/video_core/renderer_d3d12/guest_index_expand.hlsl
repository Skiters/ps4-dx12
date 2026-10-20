ByteAddressBuffer indices : register(t0);
RWByteAddressBuffer expanded : register(u0);
cbuffer Parameters : register(b0) {
    uint mode,input_count,output_count,available;
    uint element,byte_offset,row_pitch;
};
uint read_index(uint index) {
    if(index>=available)return 0;
    uint address=byte_offset+index*element;
    uint value=indices.Load(address&~3u);
    return element==2?(value>>((address&2)*8))&65535:value;
}
[numthreads(128,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    uint output=id.x+id.y*row_pitch;
    if(output>=output_count)return;
    uint index;
    if(mode==0){uint primitive=output/3,corner=output%3;index=corner==0?0:primitive+corner;}
    else if(mode==1){uint edge=output/2;index=(edge+output%2)%input_count;}
    else {uint quad=output/6,corner=output%6;const uint offsets[6]={0,1,2,2,1,3};index=quad*2+offsets[corner];}
    expanded.Store(output*4,read_index(index));
}
