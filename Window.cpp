#include "Window.hpp"
#include "app.h"
#include "settings.h"
#include "dwmapi.h"
#include "resource.h"

#include <stdexcept>

#pragma comment(lib, "dwmapi")

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

namespace
{
	const Window* gWindow = nullptr;
}

LRESULT CALLBACK mainWndProc(const HWND hWnd, const UINT msg, const WPARAM wParam, const LPARAM lParam)
{
	// Forward messages from global window procedure to member window procedure
	// This is needed because we can't assign a member function to WNDCLASS::lpfnWndProc
	if (gWindow != nullptr)
		return gWindow->MsgProc(hWnd, msg, wParam, lParam);
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

Window::Window(App& appInstance, const char* windowTitle)
	: app_(appInstance)
{
	gWindow = this;
	Init(windowTitle);
}

Window::~Window()
{
	// RAII: Proper cleanup
	if (mainWindow_ != nullptr)
	{
		DestroyWindow(mainWindow_);
		mainWindow_ = nullptr;
	}

	// Unregister the window class
	if (!windowClass_.empty())
	{
		UnregisterClassA(windowClass_.c_str(), app_.GetAppInstance());
		windowClass_.clear();
	}

	gWindow = nullptr;
}

LRESULT Window::MsgProc(const HWND hWnd, const UINT msg, const WPARAM wParam, const LPARAM lParam) const
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return 1;

	switch (msg)
	{
	case WM_SIZE:
		app_.OnWindowChanged();
		break;
	case WM_DESTROY:
		app_.Shutdown();
		break;
	default:
		break;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

void Window::Init(const char* windowTitle)
{
	const auto& appInstance = app_.GetAppInstance();

	// Store the window class name for cleanup
	windowClass_ = windowTitle;

	WNDCLASSEX wndClass{
		.cbSize = sizeof(WNDCLASSEX),
		.style = CS_VREDRAW | CS_HREDRAW,
		.lpfnWndProc = mainWndProc,
		.cbClsExtra = 0,
		.cbWndExtra = 0,
		.hInstance = appInstance,
		.hIcon = LoadIcon(appInstance, MAKEINTRESOURCE(IDI_ICON1)),
		.hCursor = nullptr,
		.hbrBackground = nullptr,
		.lpszMenuName = nullptr,
		.lpszClassName = windowClass_.c_str(),
		.hIconSm = nullptr
	};

	if (!RegisterClassEx(&wndClass))
		throw std::runtime_error("Failed to register window class");

	// Use window settings for default size
	const int defaultWidth = Settings::window.defaultWidth;
	const int defaultHeight = Settings::window.defaultHeight;

	mainWindow_ = CreateWindowEx(
		0,
		wndClass.lpszClassName,
		windowTitle,
		0,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		defaultWidth,
		defaultHeight,
		nullptr,
		nullptr,
		appInstance,
		nullptr
	);

	if (mainWindow_ == nullptr)
	{
		UnregisterClassA(windowClass_.c_str(), appInstance);
		windowClass_.clear();
		throw std::runtime_error("Failed to create app window");
	}

	SetStyle(Style::Standalone);
}

void Window::SetPosition(const int x, const int y) const
{
	SetWindowPos(mainWindow_, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

std::pair<int, int> Window::GetSize() const
{
	RECT rect = {};
	GetClientRect(mainWindow_, &rect);
	return { rect.right - rect.left, rect.bottom - rect.top };
}

void Window::SetSize(const int width, const int height) const
{
	RECT rect = {};
	GetClientRect(mainWindow_, &rect);

	if (width == rect.right - rect.left && height == rect.bottom - rect.top)
		return;

	rect.right = rect.left + width;
	rect.bottom = rect.top + height;

	// Use SetWindowLongPtr for 64-bit compatibility
	AdjustWindowRect(&rect, static_cast<DWORD>(GetWindowLongPtr(mainWindow_, GWL_STYLE)), FALSE);
	SetWindowPos(mainWindow_, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Window::SetStyle(const Style newStyle)
{
	if (newStyle == currentStyle_ || newStyle == Style::Count)
		return;

	currentStyle_ = newStyle;

	const auto styleIndex = static_cast<size_t>(currentStyle_);
	const auto& styleDef = kStyles[styleIndex];

	// Use SetWindowLongPtr for 64-bit compatibility
	SetWindowLongPtr(mainWindow_, GWL_STYLE, styleDef.style);
	SetWindowLongPtr(mainWindow_, GWL_EXSTYLE, styleDef.exStyle);

	MARGINS margins{ -1, -1, -1, -1 };
	DwmExtendFrameIntoClientArea(mainWindow_, &margins);

	const auto zOrder = (styleDef.exStyle & WS_EX_TOPMOST) ? HWND_TOPMOST : HWND_NOTOPMOST;
	SetWindowPos(mainWindow_, zOrder, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
}
