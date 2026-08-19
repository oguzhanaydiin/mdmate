#pragma once

#include <windows.h>

namespace mdmate {

enum class AppTheme { Light, Dark, Pixel };

extern AppTheme g_theme;

struct ThemeColors {
    COLORREF editorBackground;
    COLORREF editorText;
    COLORREF previewBackground;
    COLORREF body;
    COLORREF heading[6];
    COLORREF quote;
    COLORREF code;
    COLORREF list;
    COLORREF rule;
    COLORREF link;
    COLORREF inlineCode;
};

// Returns colors for the selected theme.
const ThemeColors& CurrentTheme();

}
