// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstdint>
#include <SDL3/SDL_events.h>
#include <imgui.h>

#include "common/path_util.h"
#include "core/debug_state.h"
#include "core/devtools/layer.h"
#include "core/emulator_settings.h"
#include "font_data.h"
#include "font_stack.h"
#include "imgui/imgui_layer.h"
#include "imgui_core.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_dx12.h"
#include "video_core/renderer_d3d12/resources.h"
#include "imgui_internal.h"
#include "sdl_window.h"
#include "texture_manager.h"



static std::vector<ImGui::Layer*> layers;

// Update layers before rendering to allow layer changes to be applied during rendering.
// Using deque to keep the order of changes in case a Layer is removed then added again between
// frames.
static std::deque<std::pair<bool, ImGui::Layer*>> change_layers{};
static std::mutex change_layers_mutex{};

static ImGuiID dock_id;
static std::recursive_mutex ui_mutex;
static NativeDX12::ComPtr<ID3D12DescriptorHeap> srv_heap, rtv_heap;
static std::array<bool, 2048> srv_used{};
static UINT srv_stride{};
static bool initialized{};
static std::atomic<std::uint32_t> force_gamepad_input_capture_count{0};

namespace ImGui {

namespace Core {

void AcquireGamepadInputCapture() {
    force_gamepad_input_capture_count.fetch_add(1, std::memory_order_relaxed);
}

void ReleaseGamepadInputCapture() {
    std::uint32_t expected = force_gamepad_input_capture_count.load(std::memory_order_relaxed);
    while (expected != 0) {
        if (force_gamepad_input_capture_count.compare_exchange_weak(
                expected, expected - 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
            return;
        }
    }
    LOG_WARNING(ImGui, "ReleaseGamepadInputCapture called with no active capture");
}

bool IsGamepadInputCaptured() {
    return force_gamepad_input_capture_count.load(std::memory_order_relaxed) > 0;
}

void Initialize(ID3D12Device* device, ID3D12CommandQueue* queue, const Frontend::WindowSDL& window) {
    std::lock_guard guard(ui_mutex);
    if (initialized) throw std::logic_error("Native UI already initialized");

    const auto config_path = GetUserPath(Common::FS::PathType::UserDir) / "imgui.ini";
    const auto log_path = GetUserPath(Common::FS::PathType::LogDir) / "imgui_log.txt";

    CreateContext();
    ImGuiIO& io = GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.DisplaySize = ImVec2((float)window.GetWidth(), (float)window.GetHeight());
    PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f); // Makes the window edges rounded

    auto path = config_path.u8string();
    char* config_file_buf = new char[path.size() + 1]();
    std::memcpy(config_file_buf, path.c_str(), path.size());
    io.IniFilename = config_file_buf;

    path = log_path.u8string();
    char* log_file_buf = new char[path.size() + 1]();
    std::memcpy(log_file_buf, path.c_str(), path.size());
    io.LogFilename = log_file_buf;

    ImFontConfig font_cfg{};
    font_cfg.OversampleH = 2;
    font_cfg.OversampleV = 1;
    io.Fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
    const int console_language = EmulatorSettings.GetConsoleLanguage();
    io.FontDefault = FontStack::AddPrimaryUiFont(io.Fonts, 32.0f, console_language, font_cfg, true);

    io.Fonts->AddFontFromMemoryCompressedTTF(imgui_font_proggyvector_regular_compressed_data,
                                             imgui_font_proggyvector_regular_compressed_size,
                                             32.0f);

    // Keep the large-font fallback atlas within the device texture limit.
    FontStack::AddPrimaryUiFont(io.Fonts, 128.0f, console_language, font_cfg, false);

    // Big Picture
    FontStack::AddPrimaryUiFont(ImGui::GetIO().Fonts, 64.0f, EmulatorSettings.GetConsoleLanguage(),
                                font_cfg, true);

    // Let the atlas size grow to the largest image the device can create,
    // floored to the power of two the packer requires.
    const u32 max_dim = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    const int atlas_max = static_cast<int>(std::bit_floor(std::max<u32>(max_dim, 512u)));
    io.Fonts->TexMaxWidth = atlas_max;
    io.Fonts->TexMaxHeight = atlas_max;

    io.FontGlobalScale = 0.5f;

    StyleColorsDark();

    ::Core::Devtools::Layer::SetupSettings();
    Sdl::Init(window.GetSDLWindow());

    D3D12_DESCRIPTOR_HEAP_DESC heap{};
    heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap.NumDescriptors = UINT(srv_used.size()); heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    NativeDX12::Check(device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&srv_heap)));
    srv_stride = device->GetDescriptorHandleIncrementSize(heap.Type);
    heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; heap.NumDescriptors = 1; heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    NativeDX12::Check(device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&rtv_heap)));
    ImGui_ImplDX12_InitInfo info{}; info.Device=device; info.CommandQueue=queue;
    info.NumFramesInFlight=3; info.RTVFormat=DXGI_FORMAT_R8G8B8A8_UNORM; info.SrvDescriptorHeap=srv_heap.Get();
    info.SrvDescriptorAllocFn=[](auto*, auto* cpu, auto* gpu) {
        for (UINT i=0;i<srv_used.size();++i) if (!srv_used[i]) {
            srv_used[i]=true; *cpu=srv_heap->GetCPUDescriptorHandleForHeapStart(); cpu->ptr+=SIZE_T(i)*srv_stride;
            *gpu=srv_heap->GetGPUDescriptorHandleForHeapStart(); gpu->ptr+=UINT64(i)*srv_stride; return;
        }
        throw std::runtime_error("Native UI descriptor budget exhausted");
    };
    info.SrvDescriptorFreeFn=[](auto*, auto cpu, auto) {
        const auto i=(cpu.ptr-srv_heap->GetCPUDescriptorHandleForHeapStart().ptr)/srv_stride;
        if (i>=srv_used.size() || !srv_used[i]) throw std::runtime_error("Invalid native UI descriptor release");
        srv_used[i]=false;
    };
    if (!ImGui_ImplDX12_Init(&info)) throw std::runtime_error("Native DX12 UI initialization failed");
    initialized=true;

    TextureManager::StartWorker();

    char label[32];
    ImFormatString(label, IM_ARRAYSIZE(label), "WindowOverViewport_%08X", GetMainViewport()->ID);
    dock_id = ImHashStr(label);

    if (const auto dpi = SDL_GetWindowDisplayScale(window.GetSDLWindow()); dpi > 0.0f) {
        GetIO().FontGlobalScale *= dpi;
    }

    std::at_quick_exit([] { if (GetCurrentContext()) SaveIniSettingsToDisk(GetIO().IniFilename); });
}

void OnResize() {
    std::lock_guard guard(ui_mutex); if (!initialized) return;
    Sdl::OnResize();
}

void Shutdown() {
    std::lock_guard guard(ui_mutex);
    if (!initialized) return;
    TextureManager::StopWorker();
    TextureManager::Submit();
    ImGui_ImplDX12_Shutdown(); Sdl::Shutdown();
    delete[] GetIO().IniFilename; delete[] GetIO().LogFilename;
    DestroyContext(); initialized=false; srv_heap.Reset(); rtv_heap.Reset(); srv_used.fill(false);
}

bool ProcessEvent(SDL_Event* event) {
    std::lock_guard guard(ui_mutex); if (!initialized) return false;
    Sdl::ProcessEvent(event);
    switch (event->type) {
    // Don't block release/up events
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_WHEEL:
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        const auto& io = GetIO();
        return io.WantCaptureMouse && io.Ctx->NavWindow != nullptr &&
               (io.Ctx->NavWindow->Flags & ImGuiWindowFlags_NoNav) == 0;
    }
    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_KEY_DOWN: {
        if (IsGamepadInputCaptured()) {
            // Keep keyboard events flowing through the regular input-binding path while IME/OSK
            // captures gamepad input, so keyboard equivalents of pad buttons still update the
            // virtual controller state.
            return false;
        }
        const auto& io = GetIO();
        return io.WantCaptureKeyboard && io.Ctx->NavWindow != nullptr &&
               io.Ctx->NavWindow->ID != dock_id;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
    case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
    case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
    case SDL_EVENT_GAMEPAD_SENSOR_UPDATE: {
        if (IsGamepadInputCaptured()) {
            // Let controller events continue to input bindings so OSK shortcuts and virtual
            // controller state keep updating; game-side pad reads are blocked in libScePad.
            return false;
        }
        const auto& io = GetIO();
        return io.NavActive && io.Ctx->NavWindow != nullptr && io.Ctx->NavWindow->ID != dock_id;
    }
    default:
        return false;
    }
}

ImGuiID NewFrame(bool is_reusing_frame) {
    {
        std::scoped_lock lock{change_layers_mutex};
        while (!change_layers.empty()) {
            const auto [to_be_added, layer] = change_layers.front();
            if (to_be_added) {
                layers.push_back(layer);
            } else {
                const auto [begin, end] = std::ranges::remove(layers, layer);
                layers.erase(begin, end);
            }
            change_layers.pop_front();
        }
    }

    ImGui_ImplDX12_NewFrame();
    TextureManager::Submit();
    Sdl::NewFrame(is_reusing_frame);
    ImGui::NewFrame();
    SetKeyOwner(ImGuiKey_GamepadFaceUp, ImHashStr("shadps4/pad"));

    ImGuiWindowFlags flags =
        ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_AutoHideTabBar;
    if (!DebugState.IsShowingDebugMenuBar()) {
        flags |= ImGuiDockNodeFlags_NoTabBar;
    }
    ImGuiID dockId = DockSpaceOverViewport(0, GetMainViewport(), flags);

    for (auto* layer : layers) {
        layer->Draw();
    }

    return dockId;
}

void Render(ID3D12GraphicsCommandList* commands, ID3D12Resource* target, bool reusing) {
    std::lock_guard guard(ui_mutex); if (!initialized) return;
    NewFrame(reusing); ImGui::Render();
    auto device=NativeDX12::ComPtr<ID3D12Device>{}; NativeDX12::Check(target->GetDevice(IID_PPV_ARGS(&device)));
    const auto handle=rtv_heap->GetCPUDescriptorHandleForHeapStart();
    device->CreateRenderTargetView(target,nullptr,handle);
    commands->OMSetRenderTargets(1,&handle,FALSE,nullptr);
    ID3D12DescriptorHeap* heaps[]{srv_heap.Get()}; commands->SetDescriptorHeaps(1,heaps);
    ImGui_ImplDX12_RenderDrawData(GetDrawData(),commands);
}

bool MustKeepDrawing() {
    std::lock_guard guard(ui_mutex); if (!initialized) return false;
    if (std::ranges::any_of(layers, [](Layer* layer) { return layer->ShouldKeepDrawing(); })) return true;
    std::lock_guard pending_guard(change_layers_mutex);
    return !change_layers.empty();
}

} // namespace Core

void Layer::AddLayer(Layer* layer) {
    std::scoped_lock lock{change_layers_mutex};
    change_layers.emplace_back(true, layer);
}

void Layer::RemoveLayer(Layer* layer) {
    std::scoped_lock lock{change_layers_mutex};
    change_layers.emplace_back(false, layer);
}

} // namespace ImGui
