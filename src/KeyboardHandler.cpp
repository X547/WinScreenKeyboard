#include "KeyboardHandler.h"

#include <math.h>
#include <stdio.h>

#include "KeyboardLayout.h"


static UINT RepeatDelayMs()
{
	// SPI_GETKEYBOARDDELAY: 0 (250 ms) .. 3 (1 s).
	int delay = 1;
	SystemParametersInfoW(SPI_GETKEYBOARDDELAY, 0, &delay, 0);
	return static_cast<UINT>(delay + 1) * 250;
}

static UINT RepeatIntervalMs()
{
	// SPI_GETKEYBOARDSPEED: 0 (~2.5 repeats/s) .. 31 (~30 repeats/s).
	DWORD speed = 31;
	SystemParametersInfoW(SPI_GETKEYBOARDSPEED, 0, &speed, 0);
	const double rate = 2.5 + speed * (30.0 - 2.5) / 31.0;
	return static_cast<UINT>(lround(1000.0 / rate));
}


KeyboardHandler::KeyboardHandler(HWND window, const KeyboardLayout *layout):
	fWindow(window),
	fLayout(layout),
	fHoldCounts(layout->Keys().size())
{
}


KeyboardHandler::~KeyboardHandler()
{
	ReleaseAll();
}


void KeyboardHandler::Send(size_t key, bool isDown) const
{
	const Key &def = fLayout->Keys()[key];

	INPUT input {};
	input.type = INPUT_KEYBOARD;
	if ((def.flags & KeyFlagVirtualKey) != 0) {
		input.ki.wVk = def.code;
		input.ki.wScan = static_cast<WORD>(MapVirtualKeyW(def.code, MAPVK_VK_TO_VSC));
	} else {
		// Scan codes let the target window's keyboard layout decide the meaning of the key,
		// exactly as it does for a physical keyboard.
		input.ki.wScan = def.code & 0xff;
		input.ki.dwFlags = KEYEVENTF_SCANCODE;
		if (def.IsExtended()) {
			input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
		}
	}
	if (!isDown) {
		input.ki.dwFlags |= KEYEVENTF_KEYUP;
	}

	if (SendInput(1, &input, sizeof(input)) != 1) {
		// Typically UIPI blocking input to an elevated foreground window.
		fprintf(stderr, "[ScreenKeyboard] SendInput failed: %lu\n", GetLastError());
	}
}


void KeyboardHandler::StartRepeat(size_t key)
{
	fRepeatKey = key;
	SetTimer(fWindow, kRepeatTimerId, RepeatDelayMs(), nullptr);
}


void KeyboardHandler::StopRepeat()
{
	if (!fRepeatKey.has_value()) {
		return;
	}
	fRepeatKey.reset();
	KillTimer(fWindow, kRepeatTimerId);
}


void KeyboardHandler::Hold(size_t key)
{
	if (fHoldCounts[key]++ > 0) {
		return;
	}
	Send(key, true);

	// Like a physical keyboard, pressing any key ends the typematic repeat of the previous one.
	if ((fLayout->Keys()[key].flags & KeyFlagNoRepeat) != 0) {
		StopRepeat();
	} else {
		StartRepeat(key);
	}
}


void KeyboardHandler::Unhold(size_t key)
{
	if (fHoldCounts[key] == 0 || --fHoldCounts[key] > 0) {
		return;
	}
	if (fRepeatKey == key) {
		StopRepeat();
	}
	Send(key, false);
}


void KeyboardHandler::ReleaseAll()
{
	StopRepeat();
	for (size_t key = 0; key < fHoldCounts.size(); ++key) {
		if (fHoldCounts[key] > 0) {
			fHoldCounts[key] = 0;
			Send(key, false);
		}
	}
}


bool KeyboardHandler::IsCodeDown(uint16_t scanCode) const
{
	const std::vector<Key> &keys = fLayout->Keys();
	for (size_t key = 0; key < keys.size(); ++key) {
		if ((keys[key].flags & KeyFlagVirtualKey) == 0 && keys[key].code == scanCode && fHoldCounts[key] > 0) {
			return true;
		}
	}
	return false;
}


void KeyboardHandler::HandleRepeatTimer()
{
	if (!fRepeatKey.has_value()) {
		KillTimer(fWindow, kRepeatTimerId);
		return;
	}
	Send(*fRepeatKey, true);
	SetTimer(fWindow, kRepeatTimerId, RepeatIntervalMs(), nullptr);
}
