#pragma once

#include <string>

namespace mdmate {

// Loads text with UTF-8, UTF-16, and ANSI fallback support.
bool LoadTextFile(const std::wstring& path, std::wstring& outText);

// Saves text as UTF-8 without a BOM.
bool SaveTextFileUtf8(const std::wstring& path, const std::wstring& text);

}
