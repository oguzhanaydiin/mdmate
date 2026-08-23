#pragma once

#include <windows.h>

namespace mdmate {

enum class AppTheme { Light, Dark };

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
    COLORREF sidebarBackground;
    COLORREF sidebarText;
    COLORREF sidebarMuted;
    COLORREF sidebarSelection;
    COLORREF sidebarHover;
    COLORREF chromeBackground;
    COLORREF chromeText;
    COLORREF menuHotBackground;
    COLORREF splitterHover;
};

int ScaleForWindow(HWND window, int value);

// Returns colors for the selected theme.
const ThemeColors& CurrentTheme();

// Tints the system title bar and border to match the active theme.
void ApplyWindowChrome(HWND window);

// Themes a control's scrollbars to match the active theme.
void ApplyDarkScrollbar(HWND control);

}
