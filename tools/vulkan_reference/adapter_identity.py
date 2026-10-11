"""Read-only DXGI adapter descriptors; map PDH LUIDs without opening a device."""
import ctypes as C,json,uuid
from ctypes import wintypes as W
class Luid(C.Structure):_fields_=[('low',W.DWORD),('high',W.LONG)]
class Desc(C.Structure):
    _fields_=[('description',W.WCHAR*128),('vendor',W.UINT),('device',W.UINT),('subsystem',W.UINT),('revision',W.UINT),('dedicated_video',C.c_size_t),('dedicated_system',C.c_size_t),('shared_system',C.c_size_t),('luid',Luid),('flags',W.UINT)]
def method(obj,slot,result,*args):
    address=C.cast(obj,C.POINTER(C.POINTER(C.c_void_p))).contents[slot]
    return C.WINFUNCTYPE(result,C.c_void_p,*args)(address)
def adapters():
    dxgi=C.WinDLL('dxgi');create=dxgi.CreateDXGIFactory1
    create.argtypes=[C.c_void_p,C.POINTER(C.c_void_p)];create.restype=C.c_long
    guid=(C.c_ubyte*16).from_buffer_copy(uuid.UUID('770aae78-f26f-4dba-a829-253c83d1b387').bytes_le)
    factory=C.c_void_p();result=create(C.byref(guid),C.byref(factory))
    if result<0:raise RuntimeError('CreateDXGIFactory1: '+hex(result&0xffffffff))
    rows=[]
    try:
        for index in range(16):
            adapter=C.c_void_p();result=method(factory,12,C.c_long,W.UINT,C.POINTER(C.c_void_p))(factory,index,C.byref(adapter))
            if (result&0xffffffff)==0x887a0002:break
            if result<0:raise RuntimeError('EnumAdapters1: '+hex(result&0xffffffff))
            try:
                desc=Desc();result=method(adapter,10,C.c_long,C.POINTER(Desc))(adapter,C.byref(desc))
                if result<0:raise RuntimeError('GetDesc1: '+hex(result&0xffffffff))
                rows.append({'index':index,'description':desc.description,'vendor_id':desc.vendor,'device_id':desc.device,'luid_pdh':'luid_0x%08X_0x%08X'%(desc.luid.high&0xffffffff,desc.luid.low),'dedicated_video_bytes':desc.dedicated_video,'shared_system_bytes':desc.shared_system,'flags':desc.flags})
            finally:method(adapter,2,W.ULONG)(adapter)
    finally:method(factory,2,W.ULONG)(factory)
    return rows
if __name__=='__main__':print(json.dumps(adapters(),indent=2))
