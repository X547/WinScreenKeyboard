#include "KeyboardWindow.h"

#include <math.h>
#include <stdio.h>

#include <algorithm>
#include <exception>

#include <shellapi.h>
#include <windowsx.h>

#include "KanaLayout.h"


static constexpr wchar_t kClassName[] = L"WinScreenKeyboard";
static constexpr wchar_t kTitle[] = L"Screen Keyboard";
static constexpr wchar_t kFontFace[] = L"Segoe UI";

static constexpr COLORREF kBackgroundColor = RGB(0xd0, 0xd0, 0xd0);
static constexpr COLORREF kKeyColor = RGB(0xff, 0xff, 0xff);
static constexpr COLORREF kPressedKeyColor = RGB(0x90, 0x90, 0x90);
static constexpr COLORREF kCaptionColor = RGB(0x00, 0x00, 0x00);

static constexpr float kMaxHeightRatio = 0.4f;
static constexpr float kGapRatio = 0.06f;
static constexpr float kCharFontRatio = 0.42f;
static constexpr float kNamedFontRatio = 0.26f;
static constexpr float kKanaFontRatio = 0.28f;
static constexpr size_t kMaxCharLabelLength = 2;
static constexpr UINT kPollIntervalMs = 100;

// ToUnicodeEx flag: do not change the keyboard state (Windows 10 1607+).
static constexpr UINT kToUnicodeNoStateChange = 1 << 2;

static constexpr uint16_t kLeftShiftCode = 0x2a;
static constexpr uint16_t kRightShiftCode = 0x36;
static constexpr uint16_t kRightAltCode = 0xe038;

// Touch and pen feedback (contact ripples, press-and-hold right click) only gets in
// the way of a keyboard that is held down with several fingers.
static constexpr FEEDBACK_TYPE kDisabledFeedback[] = {
	FEEDBACK_TOUCH_CONTACTVISUALIZATION,
	FEEDBACK_PEN_BARRELVISUALIZATION,
	FEEDBACK_PEN_TAP,
	FEEDBACK_PEN_DOUBLETAP,
	FEEDBACK_PEN_PRESSANDHOLD,
	FEEDBACK_PEN_RIGHTTAP,
	FEEDBACK_TOUCH_TAP,
	FEEDBACK_TOUCH_DOUBLETAP,
	FEEDBACK_TOUCH_PRESSANDHOLD,
	FEEDBACK_TOUCH_RIGHTTAP,
	FEEDBACK_GESTURE_PRESSANDTAP,
};


static bool IsAsyncKeyDown(int vk)
{
	return (GetAsyncKeyState(vk) & 0x8000) != 0;
}


static std::wstring TranslateKey(UINT vk, UINT scanCode, const BYTE *keyState, HKL layout)
{
	wchar_t buf[8];
	int count = ToUnicodeEx(vk, scanCode, keyState, buf, static_cast<int>(std::size(buf)), kToUnicodeNoStateChange, layout);
	if (count < 0) {
		// Dead key: the buffer holds its spacing form.
		count = 1;
	}
	if (count == 0 || buf[0] < L' ' || buf[0] == 0x7f) {
		return {};
	}
	return std::wstring(buf, static_cast<size_t>(count));
}


//#pragma mark - KeyboardWindow

KeyboardWindow::KeyboardWindow(LayoutType layoutType):
	fLayout(layoutType)
{
	const HINSTANCE instance = GetModuleHandleW(nullptr);

	WNDCLASSEXW windowClass {};
	windowClass.cbSize = sizeof(windowClass);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WndProc;
	windowClass.hInstance = instance;
	windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	windowClass.lpszClassName = kClassName;
	CheckThrow(RegisterClassExW(&windowClass));

	RECT workArea {};
	CheckThrow(SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0));
	const int width = workArea.right - workArea.left;
	const int height = std::min(
		static_cast<int>(lround(width * fLayout.Height() / fLayout.Width())),
		static_cast<int>(lround((workArea.bottom - workArea.top) * kMaxHeightRatio))
	);

	CheckThrow(CreateWindowExW(
		WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
		kClassName,
		kTitle,
		WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
		workArea.left, workArea.bottom - height,
		width,         height,
		nullptr,
		nullptr,
		instance,
		this
	));

	const BOOL enabled = FALSE;
	for (const FEEDBACK_TYPE feedback : kDisabledFeedback) {
		SetWindowFeedbackSetting(fWindow, feedback, 0, sizeof(enabled), &enabled);
	}

	fTaskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");
	fTrayIcon.emplace(fWindow, kTrayMessage);

	UpdateLabels();
	CheckThrow(SetTimer(fWindow, kPollTimerId, kPollIntervalMs, nullptr));
}


KeyboardWindow::~KeyboardWindow()
{
	if (fWindow != nullptr) {
		DestroyWindow(fWindow);
	}
	UnregisterClassW(kClassName, GetModuleHandleW(nullptr));
}


void KeyboardWindow::SetVisible(bool visible)
{
	if (visible) {
		ShowWindow(fWindow, SW_SHOWNOACTIVATE);
		return;
	}

	// Keys held at the moment of hiding would otherwise stay down forever.
	fPointers.clear();
	fHandler->ReleaseAll();
	ShowWindow(fWindow, SW_HIDE);
}


LRESULT CALLBACK KeyboardWindow::WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	KeyboardWindow *self = nullptr;
	if (message == WM_NCCREATE) {
		self = static_cast<KeyboardWindow*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
		SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
		self->fWindow = window;
		self->fHandler.emplace(window, &self->fLayout);
	} else {
		self = reinterpret_cast<KeyboardWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
	}

	if (self == nullptr) {
		return DefWindowProcW(window, message, wParam, lParam);
	}

	if (message == WM_NCDESTROY) {
		SetWindowLongPtrW(window, GWLP_USERDATA, 0);
		self->fHandler.reset();
		self->fWindow = nullptr;
		return DefWindowProcW(window, message, wParam, lParam);
	}

	try {
		return self->HandleMessage(message, wParam, lParam);
	} catch (const std::exception &e) {
		fprintf(stderr, "[ScreenKeyboard] message 0x%04x: %s\n", message, e.what());
	} catch (...) {
		fprintf(stderr, "[ScreenKeyboard] message 0x%04x: unknown exception\n", message);
	}
	return DefWindowProcW(window, message, wParam, lParam);
}


LRESULT KeyboardWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	bool handled = true;
	LRESULT result = 0;

	switch (message) {
		case WM_POINTERDOWN:
			HandlePointerDown(wParam, lParam);
			break;
		case WM_POINTERUPDATE:
			HandlePointerUpdate(wParam, lParam);
			break;
		case WM_POINTERUP:
		case WM_POINTERCAPTURECHANGED:
			HandlePointerUp(wParam);
			break;
		case WM_POINTERACTIVATE:
			// Keep keyboard focus in the window that receives the typed keys.
			result = PA_NOACTIVATE;
			break;
		case WM_MOUSEACTIVATE:
			result = MA_NOACTIVATE;
			break;
		case WM_TIMER:
			if (wParam == KeyboardHandler::kRepeatTimerId) {
				fHandler->HandleRepeatTimer();
			} else if (wParam == kPollTimerId) {
				if (UpdateLabels()) {
					InvalidateRect(fWindow, nullptr, FALSE);
				}
			}
			break;
		case WM_SIZE:
			HandleSize(LOWORD(lParam), HIWORD(lParam));
			break;
		case WM_DPICHANGED: {
			const RECT &suggested = *reinterpret_cast<const RECT*>(lParam);
			SetWindowPos(
				fWindow,
				nullptr,
				suggested.left,                   suggested.top,
				suggested.right - suggested.left, suggested.bottom - suggested.top,
				SWP_NOZORDER | SWP_NOACTIVATE
			);
			break;
		}
		case WM_ERASEBKGND:
			result = 1;
			break;
		case WM_PAINT:
			HandlePaint();
			break;
		case kTrayMessage:
			HandleTrayMessage(wParam, lParam);
			break;
		case WM_CLOSE:
			// The application lives in the tray; it is closed from the tray menu.
			SetVisible(false);
			break;
		case WM_DESTROY:
			HandleDestroy();
			break;
		default:
			if (message == fTaskbarCreatedMessage && fTrayIcon.has_value()) {
				fTrayIcon->HandleTaskbarCreated();
			} else {
				handled = false;
			}
			break;
	}

	return handled ? result : DefWindowProcW(fWindow, message, wParam, lParam);
}


//#pragma mark - Pointer input

void KeyboardWindow::HandlePointerDown(WPARAM wParam, LPARAM lParam)
{
	const UINT32 pointerId = GET_POINTERID_WPARAM(wParam);
	const std::optional<size_t> key = KeyAt(PointerClientPos(lParam));
	if (!key.has_value() || fPointers.contains(pointerId)) {
		return;
	}

	fPointers[pointerId] = {*key, true};
	fHandler->Hold(*key);
	Refresh();
}


void KeyboardWindow::HandlePointerUpdate(WPARAM wParam, LPARAM lParam)
{
	auto it = fPointers.find(GET_POINTERID_WPARAM(wParam));
	if (it == fPointers.end()) {
		return;
	}
	if (!IS_POINTER_INCONTACT_WPARAM(wParam)) {
		// The up message was lost, for example a mouse button released outside of the window.
		HandlePointerUp(wParam);
		return;
	}

	PointerTrack &track = it->second;
	const bool inside = KeyContains(track.key, PointerClientPos(lParam));
	if (inside == track.inside) {
		return;
	}

	track.inside = inside;
	if (inside) {
		fHandler->Hold(track.key);
	} else {
		fHandler->Unhold(track.key);
	}
	Refresh();
}


void KeyboardWindow::HandlePointerUp(WPARAM wParam)
{
	auto it = fPointers.find(GET_POINTERID_WPARAM(wParam));
	if (it == fPointers.end()) {
		return;
	}

	const PointerTrack track = it->second;
	fPointers.erase(it);
	if (track.inside) {
		fHandler->Unhold(track.key);
	}
	Refresh();
}


POINT KeyboardWindow::PointerClientPos(LPARAM lParam) const
{
	POINT pos {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
	ScreenToClient(fWindow, &pos);
	return pos;
}


std::optional<size_t> KeyboardWindow::KeyAt(POINT pos) const
{
	return fLayout.KeyAt(ToUnitX(pos.x), ToUnitY(pos.y));
}


bool KeyboardWindow::KeyContains(size_t key, POINT pos) const
{
	return fLayout.Keys()[key].Contains(ToUnitX(pos.x), ToUnitY(pos.y));
}


//#pragma mark - Tray icon

void KeyboardWindow::HandleTrayMessage(WPARAM wParam, LPARAM lParam)
{
	switch (LOWORD(lParam)) {
		case NIN_SELECT:
		case NIN_KEYSELECT:
			SetVisible(!IsVisible());
			break;
		case WM_CONTEXTMENU:
			ShowTrayMenu({GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam)});
			break;
		default:
			break;
	}
}


void KeyboardWindow::ShowTrayMenu(POINT pos)
{
	MenuRef menu(CheckThrow(CreatePopupMenu()));
	CheckThrow(AppendMenuW(menu.get(), MF_STRING, kMenuToggle, IsVisible() ? L"Hide" : L"Show"));
	CheckThrow(AppendMenuW(menu.get(), MF_SEPARATOR, 0, nullptr));
	CheckThrow(AppendMenuW(menu.get(), MF_STRING, kMenuExit, L"Exit"));

	// Without being foreground the menu does not close when clicking elsewhere.
	SetForegroundWindow(fWindow);
	const UINT command = static_cast<UINT>(TrackPopupMenuEx(
		menu.get(),
		TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
		pos.x, pos.y,
		fWindow,
		nullptr
	));
	PostMessageW(fWindow, WM_NULL, 0, 0);

	switch (command) {
		case kMenuToggle:
			SetVisible(!IsVisible());
			break;
		case kMenuExit:
			DestroyWindow(fWindow);
			break;
		default:
			break;
	}
}


//#pragma mark - Geometry

void KeyboardWindow::HandleSize(int width, int height)
{
	fClientWidth = width;
	fClientHeight = height;
	if (width <= 0 || height <= 0) {
		return;
	}

	const float unit = std::min(width / fLayout.Width(), height / fLayout.Height());
	fGap = std::max(2, static_cast<int>(lround(unit * kGapRatio)));
	fScaleX = (width - fGap) / fLayout.Width();
	fScaleY = (height - fGap) / fLayout.Height();

	const float fontBase = std::min(fScaleX, fScaleY);
	auto createFont = [](float size) {
		return FontRef(CheckThrow(CreateFontW(
			-static_cast<int>(lround(size)),
			0,
			0,
			0,
			FW_NORMAL,
			FALSE,
			FALSE,
			FALSE,
			DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS,
			CLEARTYPE_QUALITY,
			DEFAULT_PITCH | FF_DONTCARE,
			kFontFace
		)));
	};
	fCharFont = createFont(fontBase * kCharFontRatio);
	fNamedFont = createFont(fontBase * kNamedFontRatio);
	fKanaFont = createFont(fontBase * kKanaFontRatio);

	InvalidateRect(fWindow, nullptr, FALSE);
}


float KeyboardWindow::ToUnitX(int x) const
{
	return (x - fGap * 0.5f) / fScaleX;
}


float KeyboardWindow::ToUnitY(int y) const
{
	return (y - fGap * 0.5f) / fScaleY;
}


RECT KeyboardWindow::ToPixels(const KeyRect &rect) const
{
	auto edgeX = [this](float x) {return static_cast<int>(lround(fGap * 0.5f + x * fScaleX));};
	auto edgeY = [this](float y) {return static_cast<int>(lround(fGap * 0.5f + y * fScaleY));};
	const int lead = fGap / 2;
	const int trail = fGap - lead;

	return {
		edgeX(rect.x) + lead,          edgeY(rect.y) + lead,
		edgeX(rect.x + rect.w) - trail, edgeY(rect.y + rect.h) - trail
	};
}


//#pragma mark - Labels

KeyboardWindow::LabelState KeyboardWindow::QueryLabelState() const
{
	auto isCodeDown = [this](uint16_t scanCode) {
		return fHandler.has_value() && fHandler->IsCodeDown(scanCode);
	};

	// Follow the layout of the window that receives the keys, not of this thread.
	const HWND foreground = GetForegroundWindow();
	const DWORD foregroundThread = (foreground != nullptr) ? GetWindowThreadProcessId(foreground, nullptr) : 0;

	LabelState state;
	state.layout = GetKeyboardLayout(foregroundThread);
	state.shift = IsAsyncKeyDown(VK_SHIFT) || isCodeDown(kLeftShiftCode) || isCodeDown(kRightShiftCode);
	state.altGr = IsAsyncKeyDown(VK_RMENU) || isCodeDown(kRightAltCode);
	state.capsLock = (GetKeyState(VK_CAPITAL) & 1) != 0;
	state.scrollLock = (GetKeyState(VK_SCROLL) & 1) != 0;
	return state;
}


std::wstring KeyboardWindow::CharacterLabel(const Key &key, const LabelState &state) const
{
	const UINT scanCode = key.code & 0xff;
	const UINT vk = MapVirtualKeyExW(scanCode, MAPVK_VSC_TO_VK_EX, state.layout);

	if (vk != 0) {
		BYTE keyState[256] {};
		if (state.shift) {
			keyState[VK_SHIFT] = 0x80;
		}
		if (state.capsLock) {
			keyState[VK_CAPITAL] = 0x01;
		}

		if (state.altGr) {
			keyState[VK_CONTROL] = 0x80;
			keyState[VK_MENU] = 0x80;
			std::wstring label = TranslateKey(vk, scanCode, keyState, state.layout);
			if (!label.empty()) {
				return label;
			}
			// Not an AltGr layout, or nothing on the AltGr level of this key.
			keyState[VK_CONTROL] = 0;
			keyState[VK_MENU] = 0;
		}

		std::wstring label = TranslateKey(vk, scanCode, keyState, state.layout);
		if (!label.empty()) {
			return label;
		}
	}

	wchar_t name[32];
	if (GetKeyNameTextW(static_cast<LONG>(scanCode << 16), name, static_cast<int>(std::size(name))) > 0) {
		return name;
	}
	return {};
}


bool KeyboardWindow::UpdateLabels()
{
	const LabelState state = QueryLabelState();
	if (fLabelState == state) {
		return false;
	}
	fLabelState = state;

	// Whether the IME takes kana or romaji input cannot be read from another process, so
	// Japanese layouts show both, like printed JIS keycaps.
	const LANGID language = LOWORD(reinterpret_cast<uintptr_t>(state.layout));
	const bool isJapanese = PRIMARYLANGID(language) == LANG_JAPANESE;

	const std::vector<Key> &keys = fLayout.Keys();
	fLabels.clear();
	fLabels.reserve(keys.size());
	fKanaLabels.clear();
	fKanaLabels.reserve(keys.size());
	for (const Key &key : keys) {
		const bool isCharacter = key.label == nullptr;
		fLabels.push_back(isCharacter ? CharacterLabel(key, state) : std::wstring(key.label));
		fKanaLabels.push_back((isCharacter && isJapanese) ? KanaLabel(key.code, state.shift) : std::wstring());
	}
	return true;
}


void KeyboardWindow::Refresh()
{
	UpdateLabels();
	InvalidateRect(fWindow, nullptr, FALSE);
}


//#pragma mark - Drawing

void KeyboardWindow::HandlePaint()
{
	PaintScope paint(fWindow);
	if (fClientWidth <= 0 || fClientHeight <= 0) {
		return;
	}

	// Draw off-screen so that rapid key state changes do not flicker.
	DcRef memDc(CheckThrow(CreateCompatibleDC(paint.Dc())));
	BitmapRef bitmap(CheckThrow(CreateCompatibleBitmap(paint.Dc(), fClientWidth, fClientHeight)));
	SelectObjectScope bitmapScope(memDc.get(), bitmap.get());

	Draw(memDc.get());
	BitBlt(paint.Dc(), 0, 0, fClientWidth, fClientHeight, memDc.get(), 0, 0, SRCCOPY);
}


void KeyboardWindow::Draw(HDC dc) const
{
	const RECT client {0, 0, fClientWidth, fClientHeight};
	SetDCBrushColor(dc, kBackgroundColor);
	FillRect(dc, &client, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));

	SetBkMode(dc, TRANSPARENT);
	SetTextColor(dc, kCaptionColor);

	for (size_t key = 0; key < fLayout.Keys().size(); ++key) {
		DrawKey(dc, key);
	}
}


void KeyboardWindow::DrawKey(HDC dc, size_t key) const
{
	const Key &def = fLayout.Keys()[key];
	const HBRUSH brush = static_cast<HBRUSH>(GetStockObject(DC_BRUSH));
	const bool isDown = fHandler.has_value() && fHandler->IsDown(key);

	const RECT upper = ToPixels(def.rect);
	SetDCBrushColor(dc, isDown ? kPressedKeyColor : kKeyColor);
	FillRect(dc, &upper, brush);
	if (def.lowerRect.has_value()) {
		RECT lower = ToPixels(*def.lowerRect);
		lower.top = upper.bottom;
		FillRect(dc, &lower, brush);
	}

	const bool indicatorOn = fLabelState.has_value() && (
		((def.flags & KeyFlagCapsIndicator) != 0 && fLabelState->capsLock) ||
		((def.flags & KeyFlagScrollIndicator) != 0 && fLabelState->scrollLock)
	);
	if (indicatorOn) {
		const int size = std::max(3, static_cast<int>(upper.bottom - upper.top) / 8);
		const RECT indicator {
			upper.right - 2 * size, upper.top + size,
			upper.right - size,     upper.top + 2 * size
		};
		SetDCBrushColor(dc, kCaptionColor);
		FillRect(dc, &indicator, brush);
	}

	if (key >= fLabels.size()) {
		return;
	}
	const std::wstring &label = fLabels[key];
	const std::wstring &kanaLabel = fKanaLabels[key];
	const int width = upper.right - upper.left;
	const int height = upper.bottom - upper.top;

	RECT labelRect = upper;
	if (!kanaLabel.empty()) {
		// Main caption towards the top left, kana in the bottom right corner.
		labelRect.right -= width / 4;
		labelRect.bottom -= height / 5;

		const int padding = std::max(2, height / 10);
		RECT kanaRect = upper;
		InflateRect(&kanaRect, -padding, -padding);
		SelectObjectScope fontScope(dc, fKanaFont.get());
		DrawTextW(dc, kanaLabel.c_str(), -1, &kanaRect, DT_RIGHT | DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX);
	}

	if (label.empty()) {
		return;
	}
	// Layout-provided labels can be key names too (GetKeyNameText fallback), not only characters.
	const bool isCharacter = def.label == nullptr && label.size() <= kMaxCharLabelLength;
	SelectObjectScope fontScope(dc, isCharacter ? fCharFont.get() : fNamedFont.get());
	DrawTextW(dc, label.c_str(), -1, &labelRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}


void KeyboardWindow::HandleDestroy()
{
	KillTimer(fWindow, kPollTimerId);
	fTrayIcon.reset();
	fPointers.clear();
	fHandler->ReleaseAll();
	PostQuitMessage(0);
}
