// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/diagnostics.h"
#include <stdexcept>
#include <string_view>
int main() {
    using namespace NativeDX12::Diagnostics;
    _putenv_s("SHADPS4_NATIVE_DIAGNOSTICS", "1");
    auto& hub=Hub::Get();
    hub.worker.request_stop();hub.wake.notify_all();hub.worker.join();
    auto current=[&]() -> Active {
        std::lock_guard lock(hub.mutex);
        for(const auto& a:hub.active)if(a.thread==GetCurrentThreadId())return a;
        return {};
    };
    {
        Scope parent(Kind::Draw,"draw-stage");parent.Detail("draw=42");
        try {Scope nested(Kind::FenceWait,"gpu-completion");
            if(current().kind!=Kind::FenceWait) return 1;
            throw std::runtime_error("expected-test-failure");
        }catch(const std::runtime_error&){}
        if(current().kind!=Kind::Draw||std::string_view(current().detail)!="draw=42")return 2;
        std::jthread other([&]{Scope independent(Kind::Dispatch,"compute-stage");
            if(current().kind!=Kind::Dispatch)std::terminate();});
        other.join();
        if(current().kind!=Kind::Draw)return 3;
    }
    if(current().thread)return 4;
    if(hub.totals[size_t(Kind::FenceWait)].failed!=1||hub.totals[size_t(Kind::Draw)].calls!=1||
        hub.totals[size_t(Kind::Dispatch)].calls!=1)return 5;
    return 0;
}
