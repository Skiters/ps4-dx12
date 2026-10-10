// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstring>
#include "common/types.h"
#include "common/api_trace.h"
#ifdef _WIN32
#include <malloc.h>
#endif

namespace Xbyak {
class CodeGenerator;
}

namespace Libraries::Fiber {
struct OrbisFiberContext;
}

namespace Core {

union DtvEntry {
    std::size_t counter;
    u8* pointer;
};

struct Tcb {
    Tcb* tcb_self;
    DtvEntry* tcb_dtv;
    void* tcb_thread;
    void* tcb_spare[2];
    u64 tcb_canary;
    ::Libraries::Fiber::OrbisFiberContext* tcb_fiber;
};

#ifdef _WIN32
/// Gets the thread local storage key for the TCB block.
u32 GetTcbKey();
#endif

/// Sets the data pointer to the TCB block.
void SetTcbBase(void* image_address);

/// Retrieves Tcb structure for the calling thread.
Tcb* GetTcbBase();

/// Makes sure TLS is initialized for the thread before entering guest.
void InitializeTLS();

template <auto f>
struct HostCallWrapperImpl;

template <class ReturnType, class... Args, PS4_SYSV_ABI ReturnType (*func)(Args...)>
struct HostCallWrapperImpl<func> {
    static inline std::atomic<const char*> trace_name{"unregistered HLE function"};
    // Some guest kernel/signal thunks enter with an 8-byte stack alignment.
    // Realign before any C++ logger/CRT code uses aligned SIMD stack accesses.
    static ReturnType PS4_SYSV_ABI __attribute__((force_align_arg_pointer)) wrap(Args... args) {
        return ApiTrace::Call("PS4",trace_name.load(std::memory_order_relaxed),[&]()->ReturnType{return func(args...);},args...);
    }
};

template<auto func> auto TracedHostCall(const char* name, const char* nid = nullptr) {
    HostCallWrapperImpl<func>::trace_name.store(name,std::memory_order_relaxed);
    if (ApiTrace::Enabled()) {
        ApiTrace::Scope registration("PS4-MAP", name,
            std::string("nid=") + (nid ? nid : "test") +
            ";entry=" + ApiTrace::Value(&HostCallWrapperImpl<func>::wrap) +
            ";implementation=" + ApiTrace::Value(func));
        registration.Return("registered");
    }
    return HostCallWrapperImpl<func>::wrap;
}

#define HOST_CALL(func) (Core::HostCallWrapperImpl<func>::wrap)

} // namespace Core
