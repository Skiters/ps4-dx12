// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstdint>
#include <optional>
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
#include "video_core/renderer_d3d12/presenter.h"
#include "common/elf_info.h"
#include "core/libraries/system/systemservice.h"
#include "imgui_internal.h"
#include "sdl_window.h"
#include "texture_manager.h"

bool ImGui_ImplDX12_SetSurfaceFormat(DXGI_FORMAT);



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
static NativeDX12::ComPtr<ID3D12Fence> submitted_fence;
static u64 submitted_value{};
static std::array<u64,3> render_fences{};
static UINT next_render_slot{},pending_render_slot{};
static bool pending_render{};
static bool prepared_display{};
static std::optional<ImGui::RefCountedTexture> splash_image;
static const std::function<void()>* game_renderer{};
static ID3D12GraphicsCommandList* game_commands{};
static std::atomic<std::uint32_t> force_gamepad_input_capture_count{0};

namespace ImGui {

namespace Core {

static void WaitSubmission(u64 value){
    if(!submitted_fence||!value)return;
    const auto completed=submitted_fence->GetCompletedValue();
    if(completed==UINT64_MAX)throw std::runtime_error("Native UI device removed");
    if(completed>=value)return;
    const HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!event)throw std::runtime_error("Native UI fence event allocation failed");
    const auto result=submitted_fence->SetEventOnCompletion(value,event);
    if(FAILED(result)){CloseHandle(event);NativeDX12::Check(result);}
    const auto waited=WaitForSingleObject(event,INFINITE);CloseHandle(event);
    if(waited!=WAIT_OBJECT_0)throw std::runtime_error("Native UI fence wait failed");
    if(submitted_fence->GetCompletedValue()==UINT64_MAX)throw std::runtime_error("Native UI device removed during wait");
}

void Submitted(ID3D12Fence* fence,unsigned long long value){
    std::lock_guard guard(ui_mutex);if(!initialized||!pending_render)return;
    if(!fence||!value||(submitted_fence&&submitted_fence.Get()!=fence))
        throw std::invalid_argument("Native UI submission changed fence domain");
    submitted_fence=fence;submitted_value=value;
    render_fences[pending_render_slot]=value;pending_render=false;
}

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

void OnSurfaceFormatChange(DXGI_FORMAT format){
    std::lock_guard guard(ui_mutex);if(!initialized)return;
    WaitSubmission(submitted_value);
    if(!ImGui_ImplDX12_SetSurfaceFormat(format))throw std::runtime_error("Native UI format pipeline creation failed");
}

void Shutdown() {
    std::lock_guard guard(ui_mutex);
    if (!initialized) return;
    WaitSubmission(submitted_value);
    splash_image.reset();
    TextureManager::StopWorker();
    TextureManager::Shutdown(submitted_fence.Get(),submitted_value);
    ImGui_ImplDX12_Shutdown(); Sdl::Shutdown();
    delete[] GetIO().IniFilename; delete[] GetIO().LogFilename;
    DestroyContext(); initialized=false; srv_heap.Reset(); rtv_heap.Reset(); srv_used.fill(false);
    submitted_fence.Reset();submitted_value=0;render_fences.fill(0);next_render_slot=0;pending_render=false;prepared_display=false;
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
    TextureManager::Submit(submitted_fence.Get(),submitted_value);
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

static void RenderPrepared(ID3D12GraphicsCommandList* commands,ID3D12Resource* target){
    ImGui::Render();prepared_display=false;
    auto device=NativeDX12::ComPtr<ID3D12Device>{}; NativeDX12::Check(target->GetDevice(IID_PPV_ARGS(&device)));
    const auto handle=rtv_heap->GetCPUDescriptorHandleForHeapStart();
    device->CreateRenderTargetView(target,nullptr,handle);
    commands->OMSetRenderTargets(1,&handle,FALSE,nullptr);
    ID3D12DescriptorHeap* heaps[]{srv_heap.Get()}; commands->SetDescriptorHeaps(1,heaps);
    auto* data=GetDrawData();
    if(data->DisplaySize.x>0&&data->DisplaySize.y>0){
        pending_render_slot=next_render_slot++%UINT(render_fences.size());
        WaitSubmission(render_fences[pending_render_slot]);pending_render=true;
        ImGui_ImplDX12_RenderDrawData(data,commands);
    }
}

NativeDX12::PresentRegion PrepareDisplay(unsigned width,unsigned height,bool reusing){
    std::lock_guard guard(ui_mutex);
    if(!initialized||!width||!height)throw std::invalid_argument("Invalid native UI game extent");
    if(prepared_display){ImGui::EndFrame();prepared_display=false;}
    const auto dock=NewFrame(reusing);prepared_display=true;
    NativeDX12::PresentRegion region{0,0,width,height,{0,0,LONG(width),LONG(height)},false};
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2{0,0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4{0,0,0,1});
    ImGui::SetNextWindowDockID(dock,ImGuiCond_Once);
    if(ImGui::Begin("Display##game_display",nullptr,ImGuiWindowFlags_NoNav)){
        ImTextureID splash_id{};unsigned image_width=width,image_height=height;
        if(Libraries::SystemService::IsSplashVisible()){
            if(!splash_image){
                splash_image.emplace();const auto& data=Common::ElfInfo::Instance().GetSplashData();
                if(!data.empty())splash_image=ImGui::RefCountedTexture::DecodePngTexture(data);
            }
            if(splash_image.value()){
                const auto image=splash_image->GetTexture();splash_id=image.im_id;
                image_width=image.width;image_height=image.height;
            }
        }
        const auto available=ImGui::GetContentRegionAvail();
        if(available.x>=1&&available.y>=1){
            unsigned fit_width=unsigned(available.x),fit_height=unsigned(available.y);
            const float aspect=float(image_width)/float(image_height);
            if(aspect>float(fit_width)/float(fit_height))fit_height=unsigned(float(fit_width)/aspect);
            else fit_width=unsigned(float(fit_height)*aspect);
            if(fit_width&&fit_height){
                const auto start=ImGui::GetCursorStartPos();
                ImGui::SetCursorPos({start.x+(unsigned(available.x)-fit_width)/2,
                    start.y+(unsigned(available.y)-fit_height)/2});
                const auto position=ImGui::GetCursorScreenPos();const auto origin=ImGui::GetMainViewport()->Pos;
                auto* draws=ImGui::GetWindowDrawList();const auto clip_min=draws->GetClipRectMin(),clip_max=draws->GetClipRectMax();
                region={LONG(position.x-origin.x),LONG(position.y-origin.y),fit_width,fit_height,
                    {LONG(clip_min.x-origin.x),LONG(clip_min.y-origin.y),LONG(clip_max.x-origin.x),LONG(clip_max.y-origin.y)},!splash_id};
                const ImVec2 extent{float(fit_width),float(fit_height)};
                if(splash_id)ImGui::Image(splash_id,extent);
                else{
                    draws->AddCallback([](const ImDrawList*,const ImDrawCmd*){
                        if(!game_renderer||!game_commands)throw std::logic_error("Native UI game callback has no renderer");
                        (*game_renderer)();
                        ID3D12DescriptorHeap* heaps[]{srv_heap.Get()};game_commands->SetDescriptorHeaps(1,heaps);
                    },nullptr);
                    ImGui::Dummy(extent);draws->AddCallback(ImDrawCallback_ResetRenderState,nullptr);
                }
                if(EmulatorSettings.IsNullGPU())::Core::Devtools::Layer::DrawNullGpuNotice();
            }
        }
    }
    ImGui::End();ImGui::PopStyleVar(3);ImGui::PopStyleColor();
    return region;
}

void RenderDisplay(ID3D12GraphicsCommandList* commands,ID3D12Resource* target,const std::function<void()>& game){
    std::lock_guard guard(ui_mutex);
    if(!initialized||!prepared_display||!game)throw std::logic_error("Native UI display was not prepared");
    game_renderer=&game;game_commands=commands;
    try{RenderPrepared(commands,target);}catch(...){game_renderer=nullptr;game_commands=nullptr;throw;}
    game_renderer=nullptr;game_commands=nullptr;
}

void Render(ID3D12GraphicsCommandList* commands, ID3D12Resource* target, bool reusing) {
    std::lock_guard guard(ui_mutex); if (!initialized) return;
    NewFrame(reusing);RenderPrepared(commands,target);
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
