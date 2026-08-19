#include "PreviewRenderer.h"

#include <windows.h>
#include <richedit.h>

#include "../core/AppState.h"
#include "../core/StringUtils.h"
#include "Theme.h"

namespace mdmate {

namespace {

CHARFORMAT2W BuildCharFormat(const wchar_t* faceName, LONG pointSizeTwips, COLORREF color, bool bold = false) {
    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR | CFM_BOLD;
    format.yHeight = pointSizeTwips;
    format.crTextColor = color;
    format.dwEffects = bold ? CFE_BOLD : 0;
    wcsncpy_s(format.szFaceName, faceName, _TRUNCATE);
    return format;
}

CHARFORMAT2W ResolveBlockFormat(const PreviewSpan& span) {
    const ThemeColors& theme = CurrentTheme();
    switch (span.type) {
        case PreviewBlockType::Heading:
            switch (span.headingLevel) {
                case 1:
                    return BuildCharFormat(L"Segoe UI", 400, theme.heading[0], true);
                case 2:
                    return BuildCharFormat(L"Segoe UI", 340, theme.heading[1], true);
                case 3:
                    return BuildCharFormat(L"Segoe UI", 300, theme.heading[2], true);
                case 4:
                    return BuildCharFormat(L"Segoe UI", 260, theme.heading[3], true);
                case 5:
                    return BuildCharFormat(L"Segoe UI", 240, theme.heading[4], true);
                default:
                    return BuildCharFormat(L"Segoe UI", 220, theme.heading[5], true);
            }
        case PreviewBlockType::Quote:
            return BuildCharFormat(L"Segoe UI", 220, theme.quote);
        case PreviewBlockType::Code:
            return BuildCharFormat(L"Consolas", 210, theme.code);
        case PreviewBlockType::List:
            return BuildCharFormat(L"Segoe UI", 220, theme.list);
        case PreviewBlockType::Rule:
            return BuildCharFormat(L"Segoe UI", 220, theme.rule);
        case PreviewBlockType::Paragraph:
        default:
            return BuildCharFormat(L"Segoe UI", 220, theme.body);
    }
}

CHARFORMAT2W ResolveInlineOverlay(const PreviewInlineSpan& span) {
    const ThemeColors& theme = CurrentTheme();
    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_BOLD | CFM_ITALIC | CFM_STRIKEOUT;
    format.dwEffects =
        (span.bold ? CFE_BOLD : 0) | (span.italic ? CFE_ITALIC : 0) | (span.strike ? CFE_STRIKEOUT : 0);

    if (span.code) {
        format.dwMask |= CFM_FACE | CFM_COLOR;
        format.crTextColor = theme.inlineCode;
        wcsncpy_s(format.szFaceName, L"Consolas", _TRUNCATE);
    } else if (span.link) {
        format.dwMask |= CFM_COLOR | CFM_UNDERLINE;
        format.dwEffects |= CFE_UNDERLINE;
        format.crTextColor = theme.link;
    }

    return format;
}

}

void ApplyPreviewStyles(const PreviewDocument& doc) {
    SendMessageW(g_preview, WM_SETREDRAW, FALSE, 0);

    CHARRANGE previousSelection{};
    SendMessageW(g_preview, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&previousSelection));

    SetControlText(g_preview, doc.text);

    CHARFORMAT2W body = BuildCharFormat(L"Segoe UI", 220, CurrentTheme().body);
    CHARRANGE allRange{};
    allRange.cpMin = 0;
    allRange.cpMax = -1;
    SendMessageW(g_preview, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&allRange));
    SendMessageW(g_preview, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&body));

    for (const PreviewSpan& span : doc.blockSpans) {
        CHARRANGE range{};
        range.cpMin = span.start;
        range.cpMax = span.start + span.length;
        SendMessageW(g_preview, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));

        CHARFORMAT2W style = ResolveBlockFormat(span);
        SendMessageW(g_preview, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&style));
    }

    for (const PreviewInlineSpan& span : doc.inlineSpans) {
        CHARRANGE range{};
        range.cpMin = span.start;
        range.cpMax = span.start + span.length;
        SendMessageW(g_preview, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));

        CHARFORMAT2W overlay = ResolveInlineOverlay(span);
        SendMessageW(g_preview, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&overlay));
    }

    SendMessageW(g_preview, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&previousSelection));
    SendMessageW(g_preview, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_preview, nullptr, TRUE);
}

void ApplyEditorTheme() {
    const ThemeColors& theme = CurrentTheme();

    SendMessageW(g_editor, EM_SETBKGNDCOLOR, 0, theme.editorBackground);
    SendMessageW(g_preview, EM_SETBKGNDCOLOR, 0, theme.previewBackground);

    CHARFORMAT2W editorFormat{};
    editorFormat.cbSize = sizeof(editorFormat);
    editorFormat.dwMask = CFM_COLOR;
    editorFormat.crTextColor = theme.editorText;

    CHARRANGE allRange{};
    allRange.cpMin = 0;
    allRange.cpMax = -1;
    SendMessageW(g_editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&allRange));
    SendMessageW(g_editor, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&editorFormat));

    InvalidateRect(g_editor, nullptr, TRUE);
    InvalidateRect(g_preview, nullptr, TRUE);
}

}
