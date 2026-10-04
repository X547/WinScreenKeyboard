#include "KanaLayout.h"


namespace {


struct KanaKey {
	uint16_t scanCode;
	wchar_t normal;
	// 0: same as normal.
	wchar_t shifted;
};

// JIS X 6002 kana assignment, by scan code so it applies to ANSI keyboards too.
constexpr KanaKey kKanaKeys[] = {
	{0x02, L'ぬ', 0},
	{0x03, L'ふ', 0},
	{0x04, L'あ', L'ぁ'},
	{0x05, L'う', L'ぅ'},
	{0x06, L'え', L'ぇ'},
	{0x07, L'お', L'ぉ'},
	{0x08, L'や', L'ゃ'},
	{0x09, L'ゆ', L'ゅ'},
	{0x0a, L'よ', L'ょ'},
	{0x0b, L'わ', L'を'},
	{0x0c, L'ほ', 0},
	{0x0d, L'へ', 0},
	{0x7d, L'ー', 0},

	{0x10, L'た', 0},
	{0x11, L'て', 0},
	{0x12, L'い', L'ぃ'},
	{0x13, L'す', 0},
	{0x14, L'か', 0},
	{0x15, L'ん', 0},
	{0x16, L'な', 0},
	{0x17, L'に', 0},
	{0x18, L'ら', 0},
	{0x19, L'せ', 0},
	{0x1a, L'゛', 0},
	{0x1b, L'゜', L'「'},

	{0x1e, L'ち', 0},
	{0x1f, L'と', 0},
	{0x20, L'し', 0},
	{0x21, L'は', 0},
	{0x22, L'き', 0},
	{0x23, L'く', 0},
	{0x24, L'ま', 0},
	{0x25, L'の', 0},
	{0x26, L'り', 0},
	{0x27, L'れ', 0},
	{0x28, L'け', 0},
	{0x2b, L'む', L'」'},

	{0x2c, L'つ', L'っ'},
	{0x2d, L'さ', 0},
	{0x2e, L'そ', 0},
	{0x2f, L'ひ', 0},
	{0x30, L'こ', 0},
	{0x31, L'み', 0},
	{0x32, L'も', 0},
	{0x33, L'ね', L'、'},
	{0x34, L'る', L'。'},
	{0x35, L'め', L'・'},
	{0x73, L'ろ', 0},
};

} // namespace


std::wstring KanaLabel(uint16_t scanCode, bool shift)
{
	for (const KanaKey &key : kKanaKeys) {
		if (key.scanCode == scanCode) {
			return std::wstring(1, (shift && key.shifted != 0) ? key.shifted : key.normal);
		}
	}
	return {};
}
