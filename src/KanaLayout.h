#pragma once

#include <stdint.h>

#include <string>


// Hiragana of a key in JIS kana input, or an empty string for keys without kana.
std::wstring KanaLabel(uint16_t scanCode, bool shift);
