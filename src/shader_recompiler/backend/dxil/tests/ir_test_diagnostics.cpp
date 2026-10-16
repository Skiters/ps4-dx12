// SPDX-License-Identifier: GPL-2.0-or-later
// Standalone fixtures use this diagnostic sink. The emulator keeps its logger.
#include "common/logging/log.h"
#include <fmt/format.h>
#include <cstdio>
#include <cstdlib>
namespace Common::Log {
void VLog(Class, Level, const char* file, int line, const char*, fmt::string_view format, fmt::format_args args) {
    auto message = fmt::vformat(format,args);
    std::fprintf(stderr,"%s:%d: %s\n",file,line,message.c_str());
}
}
void assert_fail_impl() { std::abort(); }
[[noreturn]] void unreachable_impl() { std::abort(); }
