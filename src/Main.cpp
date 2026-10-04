#include <stdlib.h>

#include <exception>
#include <stdexcept>
#include <string_view>

#include <windows.h>

#include "KeyboardLayout.h"
#include "KeyboardWindow.h"
#include "Win32Utils.h"


static LayoutType ParseLayoutType()
{
	LayoutType type = KeyboardLayout::DetectType();
	for (int i = 1; i < __argc; ++i) {
		const std::wstring_view arg = __wargv[i];
		if (arg == L"--ansi") {
			type = LayoutType::Ansi;
		} else if (arg == L"--jis") {
			type = LayoutType::Jis;
		} else {
			throw std::invalid_argument("usage: ScreenKeyboard [--ansi | --jis]");
		}
	}
	return type;
}


static void Run()
{
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	// Deliver mouse input as WM_POINTER* too, so touch, pen and mouse share one code path.
	CheckThrow(EnableMouseInPointer(TRUE));

	KeyboardWindow window(ParseLayoutType());
	window.SetVisible(true);

	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
}


int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prevInstance, PWSTR cmdLine, int showCmd)
{
	(void)instance;
	(void)prevInstance;
	(void)cmdLine;
	(void)showCmd;

	try {
		Run();
		return 0;
	} catch (const std::exception &e) {
		MessageBoxA(nullptr, e.what(), "Screen Keyboard", MB_ICONERROR | MB_OK);
		return 1;
	}
}
