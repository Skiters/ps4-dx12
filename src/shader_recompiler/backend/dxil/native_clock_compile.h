// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <windows.h>
#include <cstdint>
#include <stdexcept>
namespace Shader::Backend::DX12 {
class NativeClockCompileScope {
    using Select=uint32_t(*)(uint32_t);
    Select select_{};uint32_t previous_{};
public:
    NativeClockCompileScope(HMODULE module,uint32_t backend){
        if(backend>2)throw std::invalid_argument("Invalid native shader clock backend");
        select_=reinterpret_cast<Select>(GetProcAddress(module,"spirv_to_dxil_set_native_clock_backend"));
        if(!select_){if(backend)throw std::runtime_error("Converter lacks native shader clock selection");return;}
        previous_=select_(backend);
        if(previous_>2)throw std::runtime_error("Converter rejected native shader clock selection");
    }
    ~NativeClockCompileScope(){if(select_&&select_(previous_)>2)std::terminate();}
    NativeClockCompileScope(const NativeClockCompileScope&)=delete;
    NativeClockCompileScope& operator=(const NativeClockCompileScope&)=delete;
};
class RasterOrderedCompileScope {
    using Select=bool(*)(uint32_t,uint32_t,uint32_t,uint32_t*,uint32_t*,uint32_t*);
    Select select_{};uint32_t previous_space_{},previous_base_{},previous_count_{};
public:
    RasterOrderedCompileScope(HMODULE module,uint32_t space,uint32_t base,uint32_t count){
        select_=reinterpret_cast<Select>(GetProcAddress(module,"spirv_to_dxil_set_raster_ordered_uav"));
        if(!select_){if(count)throw std::runtime_error("Converter lacks raster-ordered UAV selection");return;}
        if(!select_(space,base,count,&previous_space_,&previous_base_,&previous_count_))
            throw std::invalid_argument("Invalid raster-ordered UAV binding range");
    }
    ~RasterOrderedCompileScope(){
        if(select_&&!select_(previous_space_,previous_base_,previous_count_,nullptr,nullptr,nullptr))
            std::terminate();
    }
    RasterOrderedCompileScope(const RasterOrderedCompileScope&)=delete;
    RasterOrderedCompileScope& operator=(const RasterOrderedCompileScope&)=delete;
};
}
