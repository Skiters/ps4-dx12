// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Adapted from the pinned upstream host_shaders/tiling.comp.

cbuffer Tiling : register(b0) {
    uint MicroMode, Thickness, ArrayMode, PipeConfig;
    uint NumPipes, BankWidth, BankHeight, NumBanks;
    uint Aspect, TileSplit, GuestBytes, Samples;
    uint Pitch, Height, MipOffset, FirstSlice;
    uint IsMacro, Width, Rows, RowPitch;
    uint LinearOffset, Direction, BankSwizzle, LinearBytes;
    uint SpatialDepth, Depth16, LinearGeneral, AllocationBytes;
    uint AlphaOne, PackedFormat;
};
ByteAddressBuffer Input : register(t0);
RWByteAddressBuffer Output : register(u0);
RWByteAddressBuffer Written : register(u1);

uint Bit(uint v,uint n){return (v>>n)&1;}
uint Pixel(uint x,uint y,uint z){
    uint pixel=0;
    if(MicroMode==0){
        if(GuestBytes==1)pixel=Bit(x,0)|(Bit(x,1)<<1)|(Bit(x,2)<<2)|(Bit(y,1)<<3)|(Bit(y,0)<<4)|(Bit(y,2)<<5);
        else if(GuestBytes==2)pixel=(x&7)|((y&7)<<3);
        else if(GuestBytes==4)pixel=Bit(x,0)|(Bit(x,1)<<1)|(Bit(y,0)<<2)|(Bit(x,2)<<3)|(Bit(y,1)<<4)|(Bit(y,2)<<5);
        else if(GuestBytes==8)pixel=Bit(x,0)|(Bit(y,0)<<1)|(Bit(x,1)<<2)|(Bit(x,2)<<3)|(Bit(y,1)<<4)|(Bit(y,2)<<5);
        else if(GuestBytes==16)pixel=Bit(x,0)|(Bit(y,0)<<1)|(Bit(x,1)<<2)|(Bit(y,1)<<3)|(Bit(x,2)<<4)|(Bit(y,2)<<5);
    }else if(MicroMode==1||MicroMode==2){
        pixel=Bit(x,0)|(Bit(y,0)<<1)|(Bit(x,1)<<2)|(Bit(y,1)<<3)|(Bit(x,2)<<4)|(Bit(y,2)<<5);
    }else{
        pixel=(Bit(x,2)<<6)|(Bit(y,2)<<7);
        if(GuestBytes!=12)pixel|=Bit(x,0)|(Bit(y,0)<<1);
        if(GuestBytes<=2)pixel|=(Bit(x,1)<<2)|(Bit(y,1)<<3)|(Bit(z,0)<<4)|(Bit(z,1)<<5);
        else if(GuestBytes==4)pixel|=(Bit(x,1)<<2)|(Bit(z,0)<<3)|(Bit(y,1)<<4)|(Bit(z,1)<<5);
        else if(GuestBytes==8||GuestBytes==16)pixel|=(Bit(z,0)<<2)|(Bit(x,1)<<3)|(Bit(y,1)<<4)|(Bit(z,1)<<5);
        if(Thickness==8)pixel|=Bit(z,2)<<8;
    }
    return pixel;
}
uint Pipe(uint x,uint y,uint slice){
    uint tx=x/8,ty=y/8,p;
    if(PipeConfig==0)p=Bit(tx,0)^Bit(ty,0);
    else if(PipeConfig==10)p=(Bit(tx,1)^Bit(ty,0)^Bit(tx,2))|((Bit(tx,0)^Bit(ty,1))<<1)|((Bit(tx,2)^Bit(ty,2))<<2);
    else p=(Bit(tx,0)^Bit(ty,0)^Bit(tx,1))|((Bit(tx,1)^Bit(ty,1))<<1)|((Bit(tx,2)^Bit(ty,2))<<2);
    uint rotation=0;
    if(ArrayMode==12||ArrayMode==13||ArrayMode==14)rotation=max(1u,NumPipes/2-1)*(slice/Thickness);
    return p^(rotation&(NumPipes-1));
}
uint Bank(uint x,uint y,uint slice,uint split_slice){
    uint tx=x/8/(BankWidth*NumPipes),ty=y/8/BankHeight,bank;
    if(NumBanks==16)bank=(Bit(tx,0)^Bit(ty,3))|((Bit(tx,1)^Bit(ty,2)^Bit(ty,3))<<1)|((Bit(tx,2)^Bit(ty,1))<<2)|((Bit(tx,3)^Bit(ty,0))<<3);
    else if(NumBanks==8)bank=(Bit(tx,0)^Bit(ty,2))|((Bit(tx,1)^Bit(ty,1)^Bit(ty,2))<<1)|((Bit(tx,2)^Bit(ty,0))<<2);
    else if(NumBanks==4)bank=(Bit(tx,0)^Bit(ty,1))|((Bit(tx,1)^Bit(ty,0))<<1);
    else bank=Bit(tx,0)^Bit(ty,0);
    uint rotation=0,split_rotation=0;
    if(ArrayMode==4||ArrayMode==7||ArrayMode==8)rotation=(NumBanks/2-1)*(slice/Thickness);
    else if(ArrayMode==12||ArrayMode==13||ArrayMode==14)rotation=max(1u,NumPipes/2-1)*(slice/Thickness)/NumPipes;
    if(ArrayMode==4||ArrayMode==12||ArrayMode==6||ArrayMode==11)split_rotation=(NumBanks/2+1)*split_slice;
    return (bank^(BankSwizzle+rotation)^split_rotation)&(NumBanks-1);
}
uint GuestOffset(uint x,uint y,uint slice,uint sample_index){
    uint address=MipOffset;
    if(LinearGeneral!=0){
        address+=((slice*Samples+sample_index)*Pitch*Height+y*Pitch+x)*GuestBytes;
    }else{
        uint pixel=Pixel(x,y,slice);
        uint element=(MicroMode==2?pixel*Samples+sample_index:sample_index*64*Thickness+pixel)*GuestBytes;
        uint micro_bytes=64*Thickness*GuestBytes*Samples;
        if(IsMacro==0){
            address+=(slice/Thickness)*Pitch*Height*Thickness*GuestBytes*Samples+
                ((y/8)*(Pitch/8)+x/8)*micro_bytes+element;
        }else{
            uint splits=1,split_slice=0;
            if(micro_bytes>TileSplit&&Thickness==1){
                splits=micro_bytes/TileSplit;split_slice=element/TileSplit;element%=TileSplit;micro_bytes=TileSplit;
            }
            uint mw=8*BankWidth*NumPipes*Aspect,mh=8*BankHeight*NumBanks/Aspect;
            uint macro_bytes=micro_bytes*(mw/8)*(mh/8)/(NumPipes*NumBanks);
            uint per_row=Pitch/mw,per_slice=per_row*(Height/mh);
            uint total=per_slice*macro_bytes*(split_slice+splits*(slice/Thickness))+
                ((y/mh)*per_row+x/mw)*macro_bytes+
                ((y/8%BankHeight)*BankWidth+(x/8/NumPipes)%BankWidth)*micro_bytes+element;
            uint pipe_x=x,pipe_y=y;
            if(ArrayMode==5||ArrayMode==6||ArrayMode==9||ArrayMode==10||ArrayMode==11||ArrayMode==15){pipe_x%=mw;pipe_y%=mh;}
            uint pipe_bits=firstbithigh(NumPipes),bank_bits=firstbithigh(NumBanks);
            address+=(total&255)|(Pipe(pipe_x,pipe_y,slice)<<8)|(Bank(pipe_x,pipe_y,slice,split_slice)<<(8+pipe_bits))|
                ((total>>8)<<(8+pipe_bits+bank_bits));
        }
    }
    return LinearGeneral==0&&GuestBytes==12?(address/12)*12:address;
}
uint LoadByte(uint offset){return (Input.Load(offset&~3u)>>((offset&3u)*8))&255;}
uint NativePacked16(uint value,bool retile){
    uint result=value;
    if((PackedFormat&255u)==18u){
        if(retile)result=((value<<1)|(value>>15))&65535u;
        else result=((value>>1)|(value<<15))&65535u;
    }else if((PackedFormat&255u)==19u)result=((value&15u)<<12)|((value&240u)<<4)|
        ((value&3840u)>>4)|((value&61440u)>>12);
    return result;
}
void StoreByte(uint offset,uint value){
    uint aligned=offset&~3u,shift=(offset&3u)*8,mask=255u<<shift;
    uint observed=Output.Load(aligned),expected;
    [allow_uav_condition]do{
        expected=observed;
        Output.InterlockedCompareExchange(aligned,expected,(expected&~mask)|(value<<shift),observed);
    }while(observed!=expected);
    uint previous;
    Written.InterlockedOr((offset>>5)*4,1u<<(offset&31u),previous);
}

// Each detile invocation owns one complete output DWORD. Sub-DWORD retile
// stores use CAS and a byte mask, so neighbouring texels cannot lose writes.
// The mask lets CPU visibility preserve allocation padding exactly.
[numthreads(64,1,1)]
void main(uint3 id : SV_DispatchThreadID){
    uint column=id.x*4;
    if(column>=Width*LinearBytes||id.y>=Rows||id.z>=SpatialDepth*Samples)return;
    uint sample_index=id.z/SpatialDepth,slice=FirstSlice+id.z%SpatialDepth;
    uint linear_address=LinearOffset+(id.z*Rows+id.y)*RowPitch+column;
    if(Direction==2){
        if(linear_address>=AllocationBytes)return;
        uint bits=(Written.Load((linear_address>>5)*4)>>(linear_address&31u))&15u;
        if(bits==0)return;
        uint mask=((bits&1u)!=0?255u:0u)|((bits&2u)!=0?65280u:0u)|
            ((bits&4u)!=0?16711680u:0u)|((bits&8u)!=0?4278190080u:0u);
        Output.Store(linear_address,(Output.Load(linear_address)&~mask)|(Input.Load(linear_address)&mask));
        return;
    }
    if(Direction==0){
        uint value=0;
        if((PackedFormat&255u)==9u){
            uint guest=GuestOffset(column/LinearBytes,id.y,slice,sample_index);
            uint channel=(column%LinearBytes)/4,bits=channel==3u?2u:10u;
            uint word=guest+4<=AllocationBytes?Input.Load(guest):0u;
            uint raw=(word>>(channel*10u))&((1u<<bits)-1u),sign=1u<<(bits-1u);
            int signed_value=int(raw^sign)-int(sign);
            value=(PackedFormat>>8)==5u?asuint(signed_value):
                asuint(max(float(signed_value)/float(sign-1u),-1.0f));
        }else if((PackedFormat&0x10000u)!=0){
            // Native packed 16-bit resources lack the required volume view.
            // Decode the same packed/native channel ordering into float RGBA;
            // the retained guest backing supplies exact bits on later retile.
            uint guest=GuestOffset(column/LinearBytes,id.y,slice,sample_index);
            uint packed=guest+2<=AllocationBytes?LoadByte(guest)|(LoadByte(guest+1)<<8):0;
            packed=NativePacked16(packed,false);
            uint format=PackedFormat&255u,channel=(column%LinearBytes)/4;
            uint bits=5,shift=channel==0u?10u:channel==1u?5u:0u;
            if(format==16u){bits=channel==1u?6u:channel==3u?0u:5u;
                shift=channel==0u?11u:channel==1u?5u:0u;
            }else if(format==19u){bits=4u;shift=channel==0u?8u:channel==1u?4u:channel==2u?0u:12u;
            }else if(channel==3u){bits=1u;shift=15u;}
            uint mask=(1u<<bits)-1u;
            value=asuint(bits!=0u?float((packed>>shift)&mask)/float(max(mask,1u)):1.0f);
        }else if(PackedFormat!=0){
            if(LinearBytes==4){
                uint guest=GuestOffset(column/4,id.y,slice,sample_index);
                if(guest+2<=AllocationBytes)value=LoadByte(guest)|(LoadByte(guest+1)<<8);
            }else [unroll]for(uint pixel=0;pixel<2;++pixel)if(column+pixel*2<Width*2){
                uint guest=GuestOffset(column/LinearBytes+pixel,id.y,slice,sample_index);
                uint packed=guest+2<=AllocationBytes?LoadByte(guest)|(LoadByte(guest+1)<<8):0;
                value|=NativePacked16(packed,false)<<(pixel*16);
            }
        }else if(Depth16!=0){
            uint guest=GuestOffset(column/4,id.y,slice,sample_index);
            if(guest+2<=AllocationBytes)value=asuint(float(LoadByte(guest)|(LoadByte(guest+1)<<8))/65535.0f);
        }else{
            [unroll]for(uint byte=0;byte<4;++byte)if(column+byte<Width*LinearBytes){
                uint pos=column+byte,component=pos%LinearBytes;
                if(component<GuestBytes){
                    uint guest=GuestOffset(pos/LinearBytes,id.y,slice,sample_index)+component;
                    if(guest<AllocationBytes)value|=LoadByte(guest)<<(byte*8);
                }else value|=((AlphaOne>>((component-GuestBytes)*8))&255u)<<(byte*8);
            }
        }
        Output.Store(linear_address,value);
    }else{
        uint value=Input.Load(linear_address);
        if((PackedFormat&255u)==9u){
            if(column%LinearBytes!=0)return;
            uint packed=0,number=(PackedFormat>>8)&255u;
            [unroll]for(uint channel=0;channel<4;++channel){
                uint bits=channel==3u?2u:10u,mask=(1u<<bits)-1u,sign=1u<<(bits-1u);
                uint raw=Input.Load(linear_address+channel*4),component;
                if(number==4u)component=min(raw,mask);
                else if(number==5u)component=asuint(clamp(asint(raw),-int(sign),int(sign-1u)));
                else if(number==1u)component=asuint(int(round(clamp(asfloat(raw),-1.0f,1.0f)*float(sign-1u))));
                else component=uint(round(saturate(asfloat(raw))*float(mask)));
                packed|=(component&mask)<<(channel*10u);
            }
            uint guest=GuestOffset(column/LinearBytes,id.y,slice,sample_index);
            if(guest+4<=AllocationBytes){
                [unroll]for(uint byte=0;byte<4;++byte)StoreByte(guest+byte,(packed>>(byte*8u))&255u);
            }
        }else if((PackedFormat&0x10000u)!=0){
            // Each texel is assembled by one invocation; neighboring packed
            // texels share atomic byte stores and their exact written mask.
            if(column%LinearBytes!=0)return;
            uint format=PackedFormat&255u,packed=0;
            [unroll]for(uint channel=0;channel<4;++channel){
                uint bits=5,shift=channel==0u?10u:channel==1u?5u:0u;
                if(format==16u){bits=channel==1u?6u:channel==3u?0u:5u;
                    shift=channel==0u?11u:channel==1u?5u:0u;
                }else if(format==19u){bits=4u;shift=channel==0u?8u:channel==1u?4u:channel==2u?0u:12u;
                }else if(channel==3u){bits=1u;shift=15u;}
                uint mask=(1u<<bits)-1u;
                float component=asfloat(Input.Load(linear_address+channel*4));
                packed|=uint(round(saturate(component)*float(mask)))<<shift;
            }
            packed=NativePacked16(packed,true);
            uint guest=GuestOffset(column/LinearBytes,id.y,slice,sample_index);
            if(guest+2<=AllocationBytes){StoreByte(guest,packed&255u);StoreByte(guest+1,packed>>8);}
        }else if(PackedFormat!=0){
            if(LinearBytes==4){
                uint guest=GuestOffset(column/4,id.y,slice,sample_index);
                if(guest+2<=AllocationBytes){StoreByte(guest,value&255);StoreByte(guest+1,(value>>8)&255);}
            }else [unroll]for(uint pixel=0;pixel<2;++pixel)if(column+pixel*2<Width*2){
                uint guest=GuestOffset(column/LinearBytes+pixel,id.y,slice,sample_index);
                uint packed=NativePacked16((value>>(pixel*16))&65535u,true);
                if(guest+2<=AllocationBytes){StoreByte(guest,packed&255);StoreByte(guest+1,packed>>8);}
            }
        }else if(Depth16!=0){
            uint guest=GuestOffset(column/4,id.y,slice,sample_index);
            uint z=(uint)(saturate(asfloat(value))*65535.0f+0.5f);
            if(guest+2<=AllocationBytes){StoreByte(guest,z&255);StoreByte(guest+1,z>>8);}
        }else{
            if(GuestBytes>=4){
                if(column%LinearBytes>=GuestBytes)return;
                uint guest=GuestOffset(column/LinearBytes,id.y,slice,sample_index)+column%LinearBytes;
                if(guest+4<=AllocationBytes){
                    Output.Store(guest,value);
                    uint previous;
                    Written.InterlockedOr((guest>>5)*4,15u<<(guest&31u),previous);
                }
                return;
            }
            [unroll]for(uint byte=0;byte<4;++byte)if(column+byte<Width*LinearBytes){
                uint pos=column+byte,guest=GuestOffset(pos/GuestBytes,id.y,slice,sample_index)+pos%GuestBytes;
                if(guest<AllocationBytes)StoreByte(guest,(value>>(byte*8))&255);
            }
        }
    }
}
