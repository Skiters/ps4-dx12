// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <chrono>
#ifdef _WIN32
#include <wepoll.h>
#else
#include <sys/epoll.h>
#endif

namespace Libraries::Net {
template<class Handle>
int WaitNativeEpoll(Handle handle, epoll_event* events, int maxevents, int timeout_us,
                    bool pending_resolvers) {
    // Resolver work is immediately ready. Poll sockets without delaying it.
    // An empty native epoll must still wait: skipping it makes the guest spin.
    if (pending_resolvers) timeout_us = 0;
#ifdef __linux__
    const timespec timeout{.tv_sec = timeout_us < 0 ? 0 : timeout_us / 1000000,
                           .tv_nsec = timeout_us < 0 ? 0 : (timeout_us % 1000000) * 1000L};
    return epoll_pwait2(handle, events, maxevents, timeout_us < 0 ? nullptr : &timeout, nullptr);
#else
    // Round positive sub-millisecond waits up; truncation reintroduces polling.
    const int timeout_ms = timeout_us < 0 ? -1 : timeout_us / 1000 + (timeout_us % 1000 != 0);
    return epoll_wait(handle, events, maxevents, timeout_ms);
#endif
}
template<class Handle,class Ready>
int WaitEpollWithResolvers(Handle handle, epoll_event* events, int maxevents, int timeout_us,
                           Ready&& resolver_ready) {
    const auto start=std::chrono::steady_clock::now();
    // Resolvers have no native socket. Recheck their protected queue during long
    // waits so a resolver queued by another thread also wakes an infinite wait.
    for (;;) {
        const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now()-start).count();
        const int remaining=timeout_us<0?50000:
            static_cast<int>(std::max<long long>(0,timeout_us-elapsed));
        const int result=WaitNativeEpoll(handle,events,maxevents,std::min(remaining,50000),
                                         resolver_ready());
        if(result!=0 || resolver_ready() || timeout_us==0)return result;
        if(timeout_us>0 && std::chrono::steady_clock::now()-start>=
                              std::chrono::microseconds(timeout_us))return 0;
    }
}
}
