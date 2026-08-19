#pragma once

#include <windows.h>

#include <string>
#include <string_view>

namespace mdmate {

// Removes leading whitespace.
std::wstring TrimLeft(std::wstring_view text);
// Removes trailing whitespace.
std::wstring TrimRight(std::wstring_view text);
// Removes leading and trailing whitespace.
std::wstring Trim(std::wstring_view text);

// Checks whether text starts with prefix.
bool StartsWith(std::wstring_view text, std::wstring_view prefix);
// Converts text using the active C++ locale.
std::wstring ToUpper(std::wstring_view text);

// Extracts the final path component.
std::wstring GetFileNameFromPath(std::wstring_view path);

// Reads a Win32 control's text.
std::wstring ReadControlText(HWND control);
void SetControlText(HWND control, const std::wstring& text);

}
