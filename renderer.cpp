#include "renderer.h"

#include "app.h"
#include "settings.h"
#include "dependencies/fmt/fmt/format.h"
#include "dependencies/imgui/imgui.h"
#include "dependencies/imgui/imgui_impl_dx9.h"
#include "dependencies/imgui/imgui_impl_win32.h"

bool Renderer::Init(const HWND hwnd)
{
	if (!CreateDevice(hwnd))
		return false;

	ImGui::CreateContext();

	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX9_Init(d3dDevice);

	ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;
	ImGui::GetIO().IniFilename = nullptr;

	// Apply custom style from settings
	ApplyStyle();

	LoadFonts();

	return true;
}

bool Renderer::CreateDevice(const HWND hwnd)
{
	Direct3DCreate9Ex(D3D_SDK_VERSION, &d3D9Interface);
	if (d3D9Interface == nullptr)
		return false;

	d3d9Parameters = {
		.BackBufferFormat = D3DFMT_A8R8G8B8,
		.BackBufferCount = 0,
		.MultiSampleType = D3DMULTISAMPLE_NONE,
		.MultiSampleQuality = 0,
		.SwapEffect = D3DSWAPEFFECT_DISCARD,
		.hDeviceWindow = hwnd,
		.Windowed = 1,
		.EnableAutoDepthStencil = 1,
		.AutoDepthStencilFormat = D3DFMT_D16,
		.Flags = 0,
		.FullScreen_RefreshRateInHz = 0,
		.PresentationInterval = D3DPRESENT_INTERVAL_ONE,
	};

	if (d3D9Interface->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, nullptr, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3d9Parameters, nullptr, &d3dDevice) != D3D_OK)
		return false;

	return true;
}

void Renderer::Shutdown()
{
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CleanupDevice();
}

bool Renderer::BeginFrame()
{
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	return true;
}

bool Renderer::EndFrame()
{
	ImGui::EndFrame();

	const auto d3dresult = d3dDevice->TestCooperativeLevel();
	if (d3dresult == D3DERR_DEVICELOST)
		return false;

	if (resetRequested || d3dresult == D3DERR_DEVICENOTRESET)
	{
		ResetDevice();
		return  false;
	}

	if (d3dDevice->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		d3dDevice->EndScene();
	}
	const auto result = d3dDevice->Present(nullptr, nullptr, nullptr, nullptr);

	// Handle loss of D3D9 device
	if (result == D3DERR_DEVICELOST && d3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
		ResetDevice();

	return true;
}

void Renderer::CleanupDevice()
{
	if (d3dDevice)
	{
		d3dDevice->Release();
		d3dDevice = nullptr;
	}

	if (d3D9Interface)
	{
		d3D9Interface->Release();
		d3D9Interface = nullptr;
	}
}

void Renderer::ResetDevice()
{
	resetRequested = false;

	if (d3dDevice == nullptr)
		return;

	ImGui_ImplDX9_InvalidateDeviceObjects();
	const auto hr = d3dDevice->Reset(&d3d9Parameters);
	if (hr == D3DERR_INVALIDCALL)
		IM_ASSERT(0);

	LoadFonts();
}

void Renderer::LoadFonts()
{
	auto io = ImGui::GetIO();

	static const ImWchar rangesAll[] = { 0x0020, 0xFFEF, 0, };

	auto fontCfg = ImFontConfig::ImFontConfig();
	fontCfg.OversampleH = fontCfg.OversampleV = 1;
	fontCfg.PixelSnapH = true;
	fontCfg.GlyphRanges = &rangesAll[0];

	io.Fonts->AddFontDefault(&fontCfg);

#pragma warning(suppress : 4996)
	io.Fonts->AddFontFromFileTTF(format(FMT_STRING("{}\\Fonts\\arialbd.ttf"), std::getenv("WINDIR")).c_str(), 13.f, &fontCfg);

	ImGui_ImplDX9_CreateDeviceObjects();
}

void Renderer::Resize(const unsigned width, const unsigned height)
{
	if (d3d9Parameters.BackBufferWidth != width || d3d9Parameters.BackBufferHeight != 0)
	{
		d3d9Parameters.BackBufferWidth = width;
		d3d9Parameters.BackBufferHeight = height;

		//the actual Reset() is deferred until BeginScene(), because we might be in the middle of imgui rendering loop
		resetRequested = true;
	}
}

void Renderer::ApplyDefaultDarkBlueTheme()
{
	Settings::menuStyle = MenuStyleSettings{};
	ApplyStyle();
}

void Renderer::ApplyStyle()
{
	auto& style = ImGui::GetStyle();
	auto* colors = style.Colors;

	const auto& ms = Settings::menuStyle;

	// Dark style (inverted from Pacôme Danhiez light theme)
	style.Alpha = 1.0f;
	style.FrameRounding = ms.frameRounding;
	style.WindowRounding = ms.windowRounding;
	style.FrameBorderSize = ms.frameBorderSize;
	style.ItemSpacing = ImVec2(ms.itemSpacing, ms.itemSpacing);

	// Text
	colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);

	// Window
	colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);

	// Border
	colors[ImGuiCol_Border] = ImVec4(1.00f, 1.00f, 1.00f, 0.39f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.10f);

	// Frame
	colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.16f, 0.94f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);

	// Title
	colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);

	// Menu
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);

	// Scrollbar
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);

	// Check/Slider
	colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

	// Button
	colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);

	// Header
	colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

	// Separator
	colors[ImGuiCol_Separator] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.78f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

	// Resize Grip
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);

	// Tab
	colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.18f, 0.18f, 0.93f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
	colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.41f, 0.68f, 1.00f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);

	// Plot
	colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);

	// Misc
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.70f, 0.70f, 0.70f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
}
