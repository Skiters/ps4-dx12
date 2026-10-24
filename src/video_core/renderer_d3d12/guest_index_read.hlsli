// SPDX-License-Identifier: GPL-2.0-or-later
uint read_guest_index(uint i){
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
