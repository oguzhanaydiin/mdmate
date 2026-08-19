#pragma once

#include <string>
#include <string_view>

namespace mdmate {

// Recognizes Markdown block-level syntax.
bool IsHorizontalRule(std::wstring_view line);
bool IsSetextUnderline(std::wstring_view line, wchar_t* markerOut = nullptr);

bool TryParseAtxHeading(std::wstring_view trimmedLeft, size_t* headingLevelOut, std::wstring& contentOut);
bool TryParseUnorderedListItem(std::wstring_view trimmedLeft, std::wstring& restOut);
bool TryParseOrderedListItem(std::wstring_view trimmedLeft, std::wstring& markerOut, std::wstring& restOut);
bool TryParseBlockquote(std::wstring_view trimmedLeft, std::wstring& restOut);
bool TryParseFence(std::wstring_view trimmedLeft, wchar_t& fenceChar, size_t& fenceLen);

int ComputeIndentLevel(std::wstring_view rawLine);

}
