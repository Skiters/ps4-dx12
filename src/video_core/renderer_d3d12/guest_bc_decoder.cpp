// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_bc_decoder.h"
#include "common/runtime_module_trace.h"
#include <filesystem>
#include <stdexcept>
namespace NativeDX12 {
GuestBcDecoder::GuestBcDecoder(const std::wstring& converter){
    if(converter.empty()||!std::filesystem::path(converter).is_absolute())
        throw std::invalid_argument("BC volume decoder requires the configured native compiler path");
    module_=Common::RuntimeModuleTrace::Load(converter.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!module_)throw std::runtime_error("Cannot load native BC volume decoder, Win32 "+std::to_string(GetLastError()));
    decode_=reinterpret_cast<Decode>(GetProcAddress(module_,"spirv_to_dxil_decode_bc"));
    if(!decode_){FreeLibrary(module_);module_=nullptr;throw std::runtime_error("Configured native compiler lacks the BC volume decoder");}
}
GuestBcDecoder::~GuestBcDecoder(){if(module_)FreeLibrary(module_);}
std::vector<std::byte> GuestBcDecoder::Expand(uint32_t format,uint32_t number,uint32_t width,uint32_t height,
    std::span<const std::byte> blocks)const{
    if(!width||!height||width>16384||height>16384||format<35||format>41)
        throw std::invalid_argument("Invalid native BC image slice");
    const uint32_t pixel_bytes=format>=38&&format<=40?16:4;
    std::vector<std::byte> result(size_t(width)*height*pixel_bytes);
    if(!decode_(format,number,reinterpret_cast<const uint8_t*>(blocks.data()),blocks.size(),width,height,
        reinterpret_cast<uint8_t*>(result.data()),result.size()))throw std::runtime_error("Pinned Mesa BC volume decoding failed");
    return result;
}
}
