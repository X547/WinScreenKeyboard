# WinScreenKeyboard

Multitouch on-screen keyboard for Windows with physical keyboard semantics: every
on-screen key is a real key that stays down while a finger holds it. Shortcuts are
typed like on hardware, e.g. hold `Ctrl` with one finger and tap `C` with another.

- Tenkeyless layout (no numpad), ANSI or JIS; JIS is chosen automatically for
  Japanese keyboards, override with `--ansi` / `--jis`.
- Input is sent as scan codes, so the target application's keyboard layout and IME
  interpret it exactly like a hardware keyboard.
- Captions follow the keyboard layout of the foreground window, Shift/Caps Lock/AltGr.
- Typematic repeat of the last pressed key, using the system delay and rate settings.
- Pointer API (`WM_POINTER*`) for touch, pen and mouse; GDI drawing.
- Tray icon: click toggles the keyboard, right click menu has Show/Hide and Exit.
  The window close button only hides the keyboard.

## Build

Requires MinGW-w64 GCC and Meson:

```
meson setup build
meson compile -C build
build\ScreenKeyboard.exe
```

## Limitations

- Keys cannot be sent to elevated (administrator) windows due to UIPI, unless the
  keyboard itself runs elevated.
- `Ctrl+Alt+Del` cannot be injected by design of Windows.
