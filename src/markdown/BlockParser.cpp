#include "BlockParser.h"

#include <cwctype>

#include "../core/StringUtils.h"

namespace mdmate {

bool IsHorizontalRule(std::wstring_view line) {
    const std::wstring trimmed = Trim(line);
    if (trimmed.size() < 3) {
        return false;
    }

    const wchar_t marker = trimmed[0];
    if (marker != L'-' && marker != L'*' && marker != L'_') {
        return false;
    }

    for (wchar_t c : trimmed) {
        if (c != marker && !std::iswspace(c)) {
            return false;
        }
    }
    return true;
}

bool IsSetextUnderline(std::wstring_view line, wchar_t* markerOut) {
    const std::wstring trimmed = Trim(line);
    if (trimmed.empty()) {
        return false;
    }

    const wchar_t marker = trimmed[0];
    if (marker != L'=' && marker != L'-') {
        return false;
    }

    for (wchar_t c : trimmed) {
        if (c != marker && !std::iswspace(c)) {
            return false;
        }
    }

    if (markerOut != nullptr) {
        *markerOut = marker;
    }
    return true;
}

bool TryParseAtxHeading(std::wstring_view trimmedLeft, size_t* headingLevelOut, std::wstring& contentOut) {
    size_t headingLevel = 0;
    while (headingLevel < trimmedLeft.size() && trimmedLeft[headingLevel] == L'#') {
        ++headingLevel;
    }

    if (headingLevel == 0 || headingLevel > 6) {
        return false;
    }

    size_t contentStart = headingLevel;
    while (contentStart < trimmedLeft.size() && std::iswspace(trimmedLeft[contentStart])) {
        ++contentStart;
    }

    std::wstring content = Trim(trimmedLeft.substr(contentStart));

    size_t end = content.size();
    while (end > 0 && content[end - 1] == L'#') {
        --end;
    }
    if (end < content.size()) {
        size_t ws = end;
        while (ws > 0 && std::iswspace(content[ws - 1])) {
            --ws;
        }
        content = Trim(content.substr(0, ws));
    }

    contentOut = content;
    if (headingLevelOut != nullptr) {
        *headingLevelOut = headingLevel;
    }
    return true;
}

bool TryParseUnorderedListItem(std::wstring_view trimmedLeft, std::wstring& restOut) {
    if (trimmedLeft.size() < 2) {
        return false;
    }
    const wchar_t marker = trimmedLeft[0];
    if ((marker == L'-' || marker == L'*' || marker == L'+') && trimmedLeft[1] == L' ') {
        restOut = std::wstring(trimmedLeft.substr(2));
        return true;
    }
    return false;
}

bool TryParseOrderedListItem(std::wstring_view trimmedLeft, std::wstring& markerOut, std::wstring& restOut) {
    size_t i = 0;
    while (i < trimmedLeft.size() && std::iswdigit(trimmedLeft[i])) {
        ++i;
    }
    if (i == 0 || i + 1 >= trimmedLeft.size()) {
        return false;
    }
    if (trimmedLeft[i] != L'.' && trimmedLeft[i] != L')') {
        return false;
    }
    if (trimmedLeft[i + 1] != L' ') {
        return false;
    }

    markerOut = std::wstring(trimmedLeft.substr(0, i + 1)) + L" ";
    restOut = std::wstring(trimmedLeft.substr(i + 2));
    return true;
}

bool TryParseBlockquote(std::wstring_view trimmedLeft, std::wstring& restOut) {
    if (trimmedLeft.empty() || trimmedLeft[0] != L'>') {
        return false;
    }

    size_t i = 0;
    while (i < trimmedLeft.size() && trimmedLeft[i] == L'>') {
        ++i;
    }
    while (i < trimmedLeft.size() && trimmedLeft[i] == L' ') {
        ++i;
    }

    restOut = std::wstring(trimmedLeft.substr(i));
    return true;
}

bool TryParseFence(std::wstring_view trimmedLeft, wchar_t& fenceChar, size_t& fenceLen) {
    if (trimmedLeft.empty()) {
        return false;
    }

    const wchar_t marker = trimmedLeft[0];
    if (marker != L'`' && marker != L'~') {
        return false;
    }

    size_t count = 0;
    while (count < trimmedLeft.size() && trimmedLeft[count] == marker) {
        ++count;
    }

    if (count < 3) {
        return false;
    }

    fenceChar = marker;
    fenceLen = count;
    return true;
}

int ComputeIndentLevel(std::wstring_view rawLine) {
    int spaces = 0;
    for (wchar_t c : rawLine) {
        if (c == L' ') {
            ++spaces;
        } else if (c == L'\t') {
            spaces += 4;
        } else {
            break;
        }
    }
    return spaces / 2;
}

}
