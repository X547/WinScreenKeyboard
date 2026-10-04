#pragma once

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <windows.h>

#include "KeyboardHandler.h"
#include "KeyboardLayout.h"
#include "TrayIcon.h"
#include "Win32Utils.h"


class KeyboardWindow {
private:
	// Pointer contact that went down on a key. The key stays bound to the contact; it is
	// released while the contact slides off it and pressed again when it slides back.
	struct PointerTrack {
		size_t key {};
		bool inside {};
	};

	// Everything the key captions depend on.
	struct LabelState {
		HKL layout = nullptr;
		bool shift {};
		bool altGr {};
		bool capsLock {};
		bool scrollLock {};

		bool operator==(const LabelState &other) const = default;
	};

	static constexpr UINT_PTR kPollTimerId = 2;
	static constexpr UINT kTrayMessage = WM_APP + 1;
	static constexpr UINT kMenuToggle = 1;
	static constexpr UINT kMenuExit = 2;

	KeyboardLayout fLayout;
	HWND fWindow = nullptr;
	std::optional<KeyboardHandler> fHandler;
	std::map<UINT32, PointerTrack> fPointers;
	std::optional<TrayIcon> fTrayIcon;
	UINT fTaskbarCreatedMessage {};

	std::optional<LabelState> fLabelState;
	std::vector<std::wstring> fLabels;

	int fClientWidth {};
	int fClientHeight {};
	float fScaleX {};
	float fScaleY {};
	int fGap {};
	FontRef fCharFont;
	FontRef fNamedFont;

	static LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

	void HandlePointerDown(WPARAM wParam, LPARAM lParam);
	void HandlePointerUpdate(WPARAM wParam, LPARAM lParam);
	void HandlePointerUp(WPARAM wParam);
	void HandleTrayMessage(WPARAM wParam, LPARAM lParam);
	void ShowTrayMenu(POINT pos);
	void HandleSize(int width, int height);
	void HandlePaint();
	void HandleDestroy();

	POINT PointerClientPos(LPARAM lParam) const;
	std::optional<size_t> KeyAt(POINT pos) const;
	bool KeyContains(size_t key, POINT pos) const;
	float ToUnitX(int x) const;
	float ToUnitY(int y) const;
	RECT ToPixels(const KeyRect &rect) const;

	LabelState QueryLabelState() const;
	std::wstring CharacterLabel(const Key &key, const LabelState &state) const;
	bool UpdateLabels();
	void Refresh();

	void Draw(HDC dc) const;
	void DrawKey(HDC dc, size_t key) const;

public:
	explicit KeyboardWindow(LayoutType layoutType);
	~KeyboardWindow();

	KeyboardWindow(const KeyboardWindow &other) = delete;
	KeyboardWindow &operator=(const KeyboardWindow &other) = delete;

	bool IsVisible() const {return IsWindowVisible(fWindow) != FALSE;}
	void SetVisible(bool visible);
};
