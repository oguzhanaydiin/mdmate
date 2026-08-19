#include "StringUtils.h"

#include <algorithm>
#include <cwctype>

namespace mdmate {

std::wstring TrimLeft(std::wstring_view text) {
    size_t i = 0;
    while (i < text.size() && std::iswspace(text[i])) {
        ++i;
    }
    return std::wstring(text.substr(i));
}

std::wstring TrimRight(std::wstring_view text) {
    size_t i = text.size();
    while (i > 0 && std::iswspace(text[i - 1])) {
        --i;
    }
    return std::wstring(text.substr(0, i));
}

std::wstring Trim(std::wstring_view text) {
    return TrimRight(TrimLeft(text));
}

bool StartsWith(std::wstring_view text, std::wstring_view prefix) {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

std::wstring ToUpper(std::wstring_view text) {
    std::wstring upper(text);
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towupper(c)); });
    return upper;
}

std::wstring GetFileNameFromPath(std::wstring_view path) {
    const size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring_view::npos) {
        return std::wstring(path);
    }
    return std::wstring(path.substr(slash + 1));
}

std::wstring ReadControlText(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring text(static_cast<size_t>(length), L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    return text;
}

void SetControlText(HWND control, const std::wstring& text) {
    SetWindowTextW(control, text.c_str());
}

}
