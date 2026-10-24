ByteAddressBuffer indices : register(t0);
RWByteAddressBuffer expanded : register(u0);
cbuffer Parameters : register(b0) {
    uint mode,input_count,output_count,available;
    uint element,byte_offset,row_pitch,generated_first;
    uint swap_mode;
};
#include "guest_index_read.hlsli"
uint read_index(uint index){return element==0?generated_first+index:read_guest_index(index);}

[numthreads(128,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    uint output=id.x+id.y*row_pitch;
    if(output>=output_count)return;
    uint index;
    if(mode==7)index=output;
    else if(mode==0){uint primitive=output/3,corner=output%3;index=corner==0?0:primitive+corner;}
    else if(mode==1){uint edge=output/2;index=(edge+output%2)%input_count;}
    else if(mode==2){uint quad=output/6,corner=output%6;const uint offsets[6]={0,1,2,2,1,3};index=quad*2+offsets[corner];}
    else {uint primitive=output/3,corner=output%3,odd=primitive&1;
        uint offset=mode==4?(corner==2?2:corner==0?odd:1-odd):(corner==0?0:corner==1?1+odd:2-odd);
        index=primitive+offset;}
    expanded.Store(output*4,read_index(index));
}
