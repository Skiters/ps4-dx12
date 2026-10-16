// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <imgui.h>
#include <d3d12.h>
#include <functional>
union SDL_Event;
namespace Frontend { class WindowSDL; }
namespace NativeDX12 {struct PresentRegion;}
namespace ImGui::Core {
void Initialize(ID3D12Device*, ID3D12CommandQueue*, const Frontend::WindowSDL&);
void Shutdown();
void OnResize();
void OnSurfaceFormatChange(DXGI_FORMAT);
void Submitted(ID3D12Fence*,unsigned long long);
bool ProcessEvent(SDL_Event*);
void AcquireGamepadInputCapture();
void ReleaseGamepadInputCapture();
bool IsGamepadInputCaptured();
ImGuiID NewFrame(bool is_reusing_frame=false);
void Render(ID3D12GraphicsCommandList*, ID3D12Resource*, bool reusing=false);
NativeDX12::PresentRegion PrepareDisplay(unsigned width,unsigned height,bool reusing);
void RenderDisplay(ID3D12GraphicsCommandList*,ID3D12Resource*,const std::function<void()>&);
bool MustKeepDrawing();
}
