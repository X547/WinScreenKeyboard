#pragma once

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <vector>


enum class LayoutType {
	Ansi,
	Jis,
};

enum KeyFlags: uint32_t {
	KeyFlagNoRepeat        = 1 << 0,
	// Key::code holds a virtual key code instead of a scan code.
	KeyFlagVirtualKey      = 1 << 1,
	KeyFlagCapsIndicator   = 1 << 2,
	KeyFlagScrollIndicator = 1 << 3,
};

// Geometry in key units (1 unit = width of a regular letter key).
struct KeyRect {
	float x {};
	float y {};
	float w {};
	float h {};

	bool Contains(float px, float py) const {return px >= x && px < x + w && py >= y && py < y + h;}
};

struct Key {
	// Set 1 scan code (0xE0xx for extended keys), or a virtual key code with KeyFlagVirtualKey.
	uint16_t code {};
	uint32_t flags {};
	// nullptr: caption is taken from the active keyboard layout.
	const wchar_t *label = nullptr;
	KeyRect rect {};
	// Second part of an L-shaped key (JIS Enter).
	std::optional<KeyRect> lowerRect;

	bool Contains(float x, float y) const;
	bool IsExtended() const {return (code & 0xff00) == 0xe000;}
};


class KeyboardLayout {
private:
	std::vector<Key> fKeys;
	float fWidth {};
	float fHeight {};

public:
	explicit KeyboardLayout(LayoutType type);

	static LayoutType DetectType();

	const std::vector<Key> &Keys() const {return fKeys;}
	float Width() const {return fWidth;}
	float Height() const {return fHeight;}

	std::optional<size_t> KeyAt(float x, float y) const;
};
