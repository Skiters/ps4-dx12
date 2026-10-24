// SPDX-License-Identifier: GPL-2.0-or-later
ByteAddressBuffer indices:register(t0);
ByteAddressBuffer scan_input:register(t1);
RWByteAddressBuffer scan_output:register(u0);
RWByteAddressBuffer expanded:register(u1);
RWByteAddressBuffer arguments:register(u2);
cbuffer Parameters:register(b0){
    uint phase,step,mode,input_count;
    uint available,element,byte_offset,row_pitch;
    uint marker,instances,base_vertex,first_instance;
    uint group_width,swap_mode;
};
uint read_index(uint i){
    if(i>=available)return 0;
    uint relative=i*element;
    if(swap_mode){
        uint value=0;
        for(uint byte_index=0;byte_index<element;++byte_index){
            uint original=relative+byte_index;
            // CPU decoding swaps only complete words in the available byte
            // range; a final16-bit element is unchanged by Swap32/SwapWord.
            uint lane_xor=swap_mode==1?1:swap_mode==2?3:2;
            uint selected=swap_mode==1||((original&~3u)+4<=available*element)?original^lane_xor:original;
            uint address=byte_offset+selected;
            uint byte_value=(indices.Load(address&~3u)>>((address&3)*8))&255;
            value|=byte_value<<(byte_index*8);
        }
        return value;
    }
    uint address=byte_offset+relative,value=indices.Load(address&~3u);
    return element==2?(value>>((address&2)*8))&65535:value;
}
uint emitted(uint i,uint start){
    if(i<start)return 0; // This element itself is a restart marker.
    uint local=i-start;
    if(mode==6)return (local+1)%group_width==0?group_width:0;
    if(mode==5)return local>=1?2:0;
    if(mode==1)return local>=1?(2+((i+1==input_count||read_index(i+1)==marker)?2:0)):0;
    if(mode==2)return local>=3&&(local&1)?6:0;
    return local>=2?3:0;
}
void put(uint destination,uint source){expanded.Store(destination*4,read_index(source));}
[numthreads(128,1,1)]
void main(uint3 id:SV_DispatchThreadID){
    uint i=id.x+id.y*row_pitch;if(i>=input_count)return;
    if(phase==0){scan_output.Store2(i*8,uint2(read_index(i)==marker?i+1:0,0));return;}
    uint2 value=scan_input.Load2(i*8);
    if(phase==1){if(i>=step)value.x=max(value.x,scan_input.Load((i-step)*8));}
    else if(phase==2)value.y=emitted(i,value.x);
    else if(phase==3){if(i>=step)value.y+=scan_input.Load((i-step)*8+4);}
    else{
        uint n=emitted(i,value.x),at=value.y-n;
        if(n){
            if(mode==6){for(uint v=0;v<group_width;++v)put(at+v,i+1-group_width+v);}
            else if(mode==5){put(at,i-1);put(at+1,i);}
            else if(mode==0){put(at,value.x);put(at+1,i-1);put(at+2,i);}
            else if(mode==1){put(at,i-1);put(at+1,i);if(n==4){put(at+2,i);put(at+3,value.x);}}
            else if(mode==2){put(at,i-3);put(at+1,i-2);put(at+2,i-1);put(at+3,i-1);put(at+4,i-2);put(at+5,i);}
            else{uint odd=(i-value.x-2)&1;
                if(mode==4){put(at,i-2+odd);put(at+1,i-1-odd);put(at+2,i);}
                else{put(at,i-2);put(at+1,i-1+odd);put(at+2,i-odd);}}
        }
        if(i+1==input_count){arguments.Store4(0,uint4(value.y,instances,0,base_vertex));arguments.Store(16,first_instance);}
        return;
    }
    scan_output.Store2(i*8,value);
}
