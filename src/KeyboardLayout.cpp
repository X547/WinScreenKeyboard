#include "KeyboardLayout.h"

#include <algorithm>
#include <span>

#include <windows.h>


namespace {


enum class SpecKind {
	Key,
	Gap,
	// Lower part of an L-shaped key whose upper part is on the previous row.
	JoinAbove,
};

struct KeySpec {
	SpecKind kind;
	uint16_t code;
	float width;
	const wchar_t *label;
	uint32_t flags;
};

struct RowSpec {
	float y;
	std::span<const KeySpec> main;
	std::span<const KeySpec> cluster;
};


constexpr KeySpec Char(uint16_t code, float width = 1)
{
	return {SpecKind::Key, code, width, nullptr, 0};
}

constexpr KeySpec Named(uint16_t code, float width, const wchar_t *label, uint32_t flags = 0)
{
	return {SpecKind::Key, code, width, label, flags};
}

constexpr KeySpec Modifier(uint16_t code, float width, const wchar_t *label)
{
	return {SpecKind::Key, code, width, label, KeyFlagNoRepeat};
}

constexpr KeySpec Gap(float width)
{
	return {SpecKind::Gap, 0, width, nullptr, 0};
}

constexpr KeySpec JoinAbove(uint16_t code, float width)
{
	return {SpecKind::JoinAbove, code, width, nullptr, 0};
}


constexpr float kClusterX = 15.25f;
constexpr float kMainY = 1.25f;


//#pragma mark - Shared rows

constexpr KeySpec kFunctionRow[] = {
	Named(0x01, 1, L"Esc"),
	Gap(1),
	Named(0x3b, 1, L"F1"),
	Named(0x3c, 1, L"F2"),
	Named(0x3d, 1, L"F3"),
	Named(0x3e, 1, L"F4"),
	Gap(0.5f),
	Named(0x3f, 1, L"F5"),
	Named(0x40, 1, L"F6"),
	Named(0x41, 1, L"F7"),
	Named(0x42, 1, L"F8"),
	Gap(0.5f),
	Named(0x43, 1, L"F9"),
	Named(0x44, 1, L"F10"),
	Named(0x57, 1, L"F11"),
	Named(0x58, 1, L"F12"),
};

constexpr KeySpec kSystemCluster[] = {
	Named(0xe037, 1, L"PrtSc"),
	Named(0x46, 1, L"ScrLk", KeyFlagNoRepeat | KeyFlagScrollIndicator),
	// Pause has no usable make/break scan code sequence for SendInput.
	Named(VK_PAUSE, 1, L"Pause", KeyFlagNoRepeat | KeyFlagVirtualKey),
};

constexpr KeySpec kNavCluster1[] = {
	Named(0xe052, 1, L"Ins"),
	Named(0xe047, 1, L"Home"),
	Named(0xe049, 1, L"PgUp"),
};

constexpr KeySpec kNavCluster2[] = {
	Named(0xe053, 1, L"Del"),
	Named(0xe04f, 1, L"End"),
	Named(0xe051, 1, L"PgDn"),
};

constexpr KeySpec kArrowCluster1[] = {
	Gap(1),
	Named(0xe048, 1, L"↑"),
};

constexpr KeySpec kArrowCluster2[] = {
	Named(0xe04b, 1, L"←"),
	Named(0xe050, 1, L"↓"),
	Named(0xe04d, 1, L"→"),
};


//#pragma mark - ANSI

constexpr KeySpec kAnsiRow1[] = {
	Char(0x29),
	Char(0x02), Char(0x03), Char(0x04), Char(0x05), Char(0x06),
	Char(0x07), Char(0x08), Char(0x09), Char(0x0a), Char(0x0b),
	Char(0x0c), Char(0x0d),
	Named(0x0e, 2, L"Backspace"),
};

constexpr KeySpec kAnsiRow2[] = {
	Named(0x0f, 1.5f, L"Tab"),
	Char(0x10), Char(0x11), Char(0x12), Char(0x13), Char(0x14),
	Char(0x15), Char(0x16), Char(0x17), Char(0x18), Char(0x19),
	Char(0x1a), Char(0x1b),
	Char(0x2b, 1.5f),
};

constexpr KeySpec kAnsiRow3[] = {
	Named(0x3a, 1.75f, L"Caps Lock", KeyFlagNoRepeat | KeyFlagCapsIndicator),
	Char(0x1e), Char(0x1f), Char(0x20), Char(0x21), Char(0x22),
	Char(0x23), Char(0x24), Char(0x25), Char(0x26), Char(0x27),
	Char(0x28),
	Named(0x1c, 2.25f, L"Enter"),
};

constexpr KeySpec kAnsiRow4[] = {
	Modifier(0x2a, 2.25f, L"Shift"),
	Char(0x2c), Char(0x2d), Char(0x2e), Char(0x2f), Char(0x30),
	Char(0x31), Char(0x32), Char(0x33), Char(0x34), Char(0x35),
	Modifier(0x36, 2.75f, L"Shift"),
};

constexpr KeySpec kAnsiRow5[] = {
	Modifier(0x1d, 1.25f, L"Ctrl"),
	Modifier(0xe05b, 1.25f, L"Win"),
	Modifier(0x38, 1.25f, L"Alt"),
	Named(0x39, 6.25f, L""),
	Modifier(0xe038, 1.25f, L"Alt"),
	Modifier(0xe05c, 1.25f, L"Win"),
	Named(0xe05d, 1.25f, L"Menu"),
	Modifier(0xe01d, 1.25f, L"Ctrl"),
};

constexpr RowSpec kAnsiRows[] = {
	{0,          kFunctionRow, kSystemCluster},
	{kMainY + 0, kAnsiRow1,    kNavCluster1},
	{kMainY + 1, kAnsiRow2,    kNavCluster2},
	{kMainY + 2, kAnsiRow3,    {}},
	{kMainY + 3, kAnsiRow4,    kArrowCluster1},
	{kMainY + 4, kAnsiRow5,    kArrowCluster2},
};


//#pragma mark - JIS

constexpr KeySpec kJisRow1[] = {
	Named(0x29, 1, L"半/全", KeyFlagNoRepeat),
	Char(0x02), Char(0x03), Char(0x04), Char(0x05), Char(0x06),
	Char(0x07), Char(0x08), Char(0x09), Char(0x0a), Char(0x0b),
	Char(0x0c), Char(0x0d), Char(0x7d),
	Named(0x0e, 1, L"BS"),
};

constexpr KeySpec kJisRow2[] = {
	Named(0x0f, 1.5f, L"Tab"),
	Char(0x10), Char(0x11), Char(0x12), Char(0x13), Char(0x14),
	Char(0x15), Char(0x16), Char(0x17), Char(0x18), Char(0x19),
	Char(0x1a), Char(0x1b),
	Named(0x1c, 1.5f, L"Enter"),
};

constexpr KeySpec kJisRow3[] = {
	Named(0x3a, 1.75f, L"英数", KeyFlagNoRepeat | KeyFlagCapsIndicator),
	Char(0x1e), Char(0x1f), Char(0x20), Char(0x21), Char(0x22),
	Char(0x23), Char(0x24), Char(0x25), Char(0x26), Char(0x27),
	Char(0x28), Char(0x2b),
	JoinAbove(0x1c, 1.25f),
};

constexpr KeySpec kJisRow4[] = {
	Modifier(0x2a, 2.25f, L"Shift"),
	Char(0x2c), Char(0x2d), Char(0x2e), Char(0x2f), Char(0x30),
	Char(0x31), Char(0x32), Char(0x33), Char(0x34), Char(0x35),
	Char(0x73),
	Modifier(0x36, 1.75f, L"Shift"),
};

constexpr KeySpec kJisRow5[] = {
	Modifier(0x1d, 1.25f, L"Ctrl"),
	Modifier(0xe05b, 1.25f, L"Win"),
	Modifier(0x38, 1.25f, L"Alt"),
	Named(0x7b, 1.25f, L"無変換", KeyFlagNoRepeat),
	Named(0x39, 3.75f, L""),
	Named(0x79, 1.25f, L"変換", KeyFlagNoRepeat),
	Named(0x70, 1.25f, L"かな", KeyFlagNoRepeat),
	Modifier(0xe038, 1.25f, L"Alt"),
	Named(0xe05d, 1.25f, L"Menu"),
	Modifier(0xe01d, 1.25f, L"Ctrl"),
};

constexpr RowSpec kJisRows[] = {
	{0,          kFunctionRow, kSystemCluster},
	{kMainY + 0, kJisRow1,     kNavCluster1},
	{kMainY + 1, kJisRow2,     kNavCluster2},
	{kMainY + 2, kJisRow3,     {}},
	{kMainY + 3, kJisRow4,     kArrowCluster1},
	{kMainY + 4, kJisRow5,     kArrowCluster2},
};


} // namespace


//#pragma mark - Key

bool Key::Contains(float x, float y) const
{
	return rect.Contains(x, y) || (lowerRect.has_value() && lowerRect->Contains(x, y));
}


//#pragma mark - KeyboardLayout

KeyboardLayout::KeyboardLayout(LayoutType type)
{
	const std::span<const RowSpec> rows = (type == LayoutType::Jis) ? std::span<const RowSpec>(kJisRows) : std::span<const RowSpec>(kAnsiRows);

	auto addSpecs = [this](std::span<const KeySpec> specs, float x, float y) {
		for (const KeySpec &spec : specs) {
			const KeyRect rect {x, y, spec.width, 1};
			x += spec.width;
			fWidth = std::max(fWidth, x);
			fHeight = std::max(fHeight, y + 1);

			switch (spec.kind) {
				case SpecKind::Key:
					fKeys.push_back({spec.code, spec.flags, spec.label, rect, std::nullopt});
					break;
				case SpecKind::JoinAbove: {
					auto it = std::find_if(fKeys.begin(), fKeys.end(), [&spec](const Key &key) {
						return key.code == spec.code;
					});
					if (it != fKeys.end()) {
						it->lowerRect = rect;
					}
					break;
				}
				case SpecKind::Gap:
					break;
			}
		}
	};

	for (const RowSpec &row : rows) {
		addSpecs(row.main, 0, row.y);
		addSpecs(row.cluster, kClusterX, row.y);
	}
}


LayoutType KeyboardLayout::DetectType()
{
	constexpr int kJapaneseKeyboardType = 7;
	return (GetKeyboardType(0) == kJapaneseKeyboardType) ? LayoutType::Jis : LayoutType::Ansi;
}


std::optional<size_t> KeyboardLayout::KeyAt(float x, float y) const
{
	for (size_t i = 0; i < fKeys.size(); ++i) {
		if (fKeys[i].Contains(x, y)) {
			return i;
		}
	}
	return std::nullopt;
}
