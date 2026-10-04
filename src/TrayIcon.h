#pragma once

#include <windows.h>

#include "Win32Utils.h"


// Notification area icon. Mouse and keyboard activity on it is reported to the owner
// window as `callbackMessage` in NOTIFYICON_VERSION_4 format.
class TrayIcon {
private:
	static constexpr UINT kIconId = 1;

	HWND fWindow;
	UINT fCallbackMessage;
	IconRef fIcon;

	void Add();

public:
	TrayIcon(HWND window, UINT callbackMessage);
	~TrayIcon();

	TrayIcon(const TrayIcon &other) = delete;
	TrayIcon &operator=(const TrayIcon &other) = delete;

	// Explorer forgets all icons when it restarts; call on the "TaskbarCreated" message.
	void HandleTaskbarCreated();
};
