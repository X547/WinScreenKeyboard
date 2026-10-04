#pragma once

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <vector>

#include <windows.h>

class KeyboardLayout;


// Turns presses of on-screen keys into system keyboard input, emulating a physical
// keyboard: a key is down while at least one contact holds it, and the most recently
// pressed key auto-repeats with the system typematic delay and rate.
class KeyboardHandler {
private:
	HWND fWindow;
	const KeyboardLayout *fLayout;
	std::vector<uint32_t> fHoldCounts;
	std::optional<size_t> fRepeatKey;

	void Send(size_t key, bool isDown) const;
	void StartRepeat(size_t key);
	void StopRepeat();

public:
	static constexpr UINT_PTR kRepeatTimerId = 1;

	// Repeat timer messages are posted to `window`, which must forward them to HandleRepeatTimer().
	KeyboardHandler(HWND window, const KeyboardLayout *layout);
	~KeyboardHandler();

	KeyboardHandler(const KeyboardHandler &other) = delete;
	KeyboardHandler &operator=(const KeyboardHandler &other) = delete;

	void Hold(size_t key);
	void Unhold(size_t key);
	void ReleaseAll();

	bool IsDown(size_t key) const {return fHoldCounts[key] > 0;}
	bool IsCodeDown(uint16_t scanCode) const;

	void HandleRepeatTimer();
};
