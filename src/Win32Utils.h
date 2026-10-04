#pragma once

#include <memory>
#include <system_error>
#include <type_traits>

#include <windows.h>


[[noreturn]] inline void ThrowLastError()
{
	throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
}

// Throws on a zero/null result of a Win32 call that reports details via GetLastError().
template <typename T>
T CheckThrow(T res)
{
	if (res == T{}) {
		ThrowLastError();
	}
	return res;
}


//#pragma mark - GDI resource owners

struct GdiObjectDeleter {
	void operator()(HGDIOBJ obj) const {DeleteObject(obj);}
};

struct DcDeleter {
	void operator()(HDC dc) const {DeleteDC(dc);}
};

template <typename T>
using GdiObjectRef = std::unique_ptr<std::remove_pointer_t<T>, GdiObjectDeleter>;

using FontRef = GdiObjectRef<HFONT>;
using BitmapRef = GdiObjectRef<HBITMAP>;
using DcRef = std::unique_ptr<std::remove_pointer_t<HDC>, DcDeleter>;


//#pragma mark - USER resource owners

struct IconDeleter {
	void operator()(HICON icon) const {DestroyIcon(icon);}
};

struct MenuDeleter {
	void operator()(HMENU menu) const {DestroyMenu(menu);}
};

using IconRef = std::unique_ptr<std::remove_pointer_t<HICON>, IconDeleter>;
using MenuRef = std::unique_ptr<std::remove_pointer_t<HMENU>, MenuDeleter>;


//#pragma mark - Scope guards

class SelectObjectScope {
private:
	HDC fDc;
	HGDIOBJ fOldObject;

public:
	SelectObjectScope(HDC dc, HGDIOBJ obj):
		fDc(dc),
		fOldObject(SelectObject(dc, obj))
	{
	}

	~SelectObjectScope() {SelectObject(fDc, fOldObject);}

	SelectObjectScope(const SelectObjectScope &other) = delete;
	SelectObjectScope &operator=(const SelectObjectScope &other) = delete;
};


class PaintScope {
private:
	HWND fWindow;
	PAINTSTRUCT fPaint {};
	HDC fDc;

public:
	explicit PaintScope(HWND window):
		fWindow(window),
		fDc(BeginPaint(window, &fPaint))
	{
	}

	~PaintScope() {EndPaint(fWindow, &fPaint);}

	PaintScope(const PaintScope &other) = delete;
	PaintScope &operator=(const PaintScope &other) = delete;

	HDC Dc() const {return fDc;}
};
