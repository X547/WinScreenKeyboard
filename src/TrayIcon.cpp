#include "TrayIcon.h"

#include <stdio.h>
#include <wchar.h>

#include <algorithm>
#include <vector>

#include <shellapi.h>


static constexpr wchar_t kTip[] = L"Screen Keyboard";

static constexpr COLORREF kOutlineColor = RGB(0x40, 0x40, 0x40);
static constexpr COLORREF kBodyColor = RGB(0xd0, 0xd0, 0xd0);
static constexpr COLORREF kKeyColor = RGB(0xff, 0xff, 0xff);


static void FillSolidRect(HDC dc, const RECT &rect, COLORREF color)
{
	SetDCBrushColor(dc, color);
	FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}


// Miniature of the keyboard window: light-gray body with white keys and a space bar.
static IconRef CreateKeyboardIcon(int size)
{
	const int bodyHeight = size * 11 / 16;
	const int bodyTop = (size - bodyHeight) / 2;
	const RECT body {0, bodyTop, size, bodyTop + bodyHeight};
	const int line = std::max(1, size / 16);

	BITMAPINFO info {};
	info.bmiHeader.biSize = sizeof(info.bmiHeader);
	info.bmiHeader.biWidth = size;
	info.bmiHeader.biHeight = -size;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;
	void *colorBits = nullptr;
	// Zero-initialized: black where the mask is transparent, and no alpha channel in use.
	BitmapRef color(CheckThrow(CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &colorBits, nullptr, 0)));

	const std::vector<BYTE> maskBits(static_cast<size_t>((size + 15) / 16 * 2 * size), 0xff);
	BitmapRef mask(CheckThrow(CreateBitmap(size, size, 1, 1, maskBits.data())));

	DcRef dc(CheckThrow(CreateCompatibleDC(nullptr)));
	{
		SelectObjectScope maskScope(dc.get(), mask.get());
		FillSolidRect(dc.get(), body, RGB(0, 0, 0));
	}
	{
		SelectObjectScope colorScope(dc.get(), color.get());
		FillSolidRect(dc.get(), body, kOutlineColor);

		RECT inner = body;
		InflateRect(&inner, -line, -line);
		FillSolidRect(dc.get(), inner, kBodyColor);

		RECT keys = inner;
		InflateRect(&keys, -line, -line);
		constexpr int kColumns = 4;
		constexpr int kRows = 3;
		auto keyRect = [&keys, line](int column, int row, int span) {
			const int width = keys.right - keys.left + line;
			const int height = keys.bottom - keys.top + line;
			return RECT {
				keys.left + column * width / kColumns,                 keys.top + row * height / kRows,
				keys.left + (column + span) * width / kColumns - line, keys.top + (row + 1) * height / kRows - line
			};
		};
		for (int row = 0; row < kRows - 1; ++row) {
			for (int column = 0; column < kColumns; ++column) {
				FillSolidRect(dc.get(), keyRect(column, row, 1), kKeyColor);
			}
		}
		FillSolidRect(dc.get(), keyRect(1, kRows - 1, 2), kKeyColor);
	}

	ICONINFO iconInfo {};
	iconInfo.fIcon = TRUE;
	iconInfo.hbmMask = mask.get();
	iconInfo.hbmColor = color.get();
	return IconRef(CheckThrow(CreateIconIndirect(&iconInfo)));
}


TrayIcon::TrayIcon(HWND window, UINT callbackMessage):
	fWindow(window),
	fCallbackMessage(callbackMessage),
	fIcon(CreateKeyboardIcon(GetSystemMetricsForDpi(SM_CXSMICON, GetDpiForWindow(window))))
{
	Add();
}


TrayIcon::~TrayIcon()
{
	NOTIFYICONDATAW data {};
	data.cbSize = sizeof(data);
	data.hWnd = fWindow;
	data.uID = kIconId;
	Shell_NotifyIconW(NIM_DELETE, &data);
}


void TrayIcon::Add()
{
	NOTIFYICONDATAW data {};
	data.cbSize = sizeof(data);
	data.hWnd = fWindow;
	data.uID = kIconId;
	data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
	data.uCallbackMessage = fCallbackMessage;
	data.hIcon = fIcon.get();
	wcsncpy(data.szTip, kTip, std::size(data.szTip) - 1);

	if (!Shell_NotifyIconW(NIM_ADD, &data)) {
		// The taskbar may not exist yet, for example early during logon. The icon is
		// added when the "TaskbarCreated" message arrives.
		fprintf(stderr, "[ScreenKeyboard] Shell_NotifyIcon(NIM_ADD) failed\n");
		return;
	}

	data.uVersion = NOTIFYICON_VERSION_4;
	Shell_NotifyIconW(NIM_SETVERSION, &data);
}


void TrayIcon::HandleTaskbarCreated()
{
	Add();
}
