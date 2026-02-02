#pragma once
#include <array>
#include <string>
#include <Windows.h>

class App;

class Window
{
public:
	enum class Style : std::uint8_t
	{
		Standalone,
		Attached,
		Overlay,
		Count
	};

	// Alias for backward compatibility
	using Styles = Style;

	// Delete copy operations
	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;

	explicit Window(App& appInstance, const char* windowTitle);
	~Window();

	[[nodiscard]] LRESULT MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) const;
	[[nodiscard]] HWND GetHwnd() const noexcept { return mainWindow_; }
	[[nodiscard]] Style GetStyle() const noexcept { return currentStyle_; }
	[[nodiscard]] std::pair<int, int> GetSize() const;

	void SetPosition(int x, int y) const;
	void SetSize(int width, int height) const;
	void SetStyle(Style newStyle);

private:
	struct StyleDef
	{
		DWORD style;
		DWORD exStyle;
	};

	static constexpr std::array<StyleDef, static_cast<size_t>(Style::Count)> kStyles = {{
		{ WS_POPUP, 0 },                                                      // Standalone (borderless)
		{ WS_POPUP, WS_EX_TOPMOST },                                          // Attached
		{ WS_POPUP, WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT }       // Overlay (clickthrough)
	}};

	void Init(const char* windowTitle);

	HWND mainWindow_ = nullptr;
	App& app_;
	Style currentStyle_ = Style::Count;  // Initialize to invalid state so first SetStyle always applies
	std::string windowClass_;  // Store for cleanup
};
