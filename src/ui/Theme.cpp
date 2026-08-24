#include "Theme.h"

#include <dwmapi.h>
#include <uxtheme.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif

namespace mdmate {

AppTheme g_theme = AppTheme::Light;

namespace {

// Undocumented uxtheme exports needed to make per-control dark scrollbar theming take effect.
using SetPreferredAppModeFn = int(WINAPI*)(int);
using AllowDarkModeForWindowFn = BOOL(WINAPI*)(HWND, BOOL);
using FlushMenuThemesFn = void(WINAPI*)();

SetPreferredAppModeFn g_setPreferredAppMode = nullptr;
AllowDarkModeForWindowFn g_allowDarkModeForWindow = nullptr;
FlushMenuThemesFn g_flushMenuThemes = nullptr;
bool g_darkModeApisResolved = false;

void ResolveDarkModeApis() {
    if (g_darkModeApisResolved) {
        return;
    }
    g_darkModeApisResolved = true;

    const HMODULE uxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (uxtheme == nullptr) {
        return;
    }

    g_setPreferredAppMode = reinterpret_cast<SetPreferredAppModeFn>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(135)));
    g_allowDarkModeForWindow =
        reinterpret_cast<AllowDarkModeForWindowFn>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(133)));
    g_flushMenuThemes = reinterpret_cast<FlushMenuThemesFn>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(136)));
}

}
constexpr ThemeColors kLightTheme{
    .editorBackground = RGB(255, 255, 255),
    .editorText = RGB(30, 30, 30),
    .previewBackground = RGB(255, 255, 255),
    .body = RGB(30, 30, 30),
    .heading = {RGB(15, 15, 15), RGB(20, 20, 20), RGB(28, 28, 28), RGB(35, 35, 35), RGB(45, 45, 45), RGB(55, 55, 55)},
    .quote = RGB(95, 95, 95),
    .code = RGB(120, 40, 100),
    .list = RGB(30, 30, 30),
    .rule = RGB(200, 200, 200),
    .link = RGB(20, 90, 200),
    .inlineCode = RGB(170, 40, 110),
    .sidebarBackground = RGB(243, 243, 243),
    .sidebarText = RGB(30, 30, 30),
    .sidebarMuted = RGB(97, 97, 97),
    .sidebarSelection = RGB(209, 232, 255),
    .sidebarHover = RGB(228, 228, 228),
    .chromeBackground = RGB(243, 243, 243),
    .chromeText = RGB(26, 26, 26),
    .menuHotBackground = RGB(226, 226, 226),
    .splitterHover = RGB(120, 170, 220),
};

constexpr ThemeColors kDarkTheme{
    .editorBackground = RGB(30, 30, 30),
    .editorText = RGB(225, 225, 225),
    .previewBackground = RGB(30, 30, 30),
    .body = RGB(215, 215, 215),
    .heading = {RGB(255, 255, 255), RGB(240, 240, 240), RGB(225, 225, 225), RGB(210, 210, 210), RGB(195, 195, 195),
                RGB(180, 180, 180)},
    .quote = RGB(170, 170, 170),
    .code = RGB(230, 150, 80),
    .list = RGB(210, 210, 210),
    .rule = RGB(70, 70, 70),
    .link = RGB(110, 170, 255),
    .inlineCode = RGB(240, 150, 190),
    .sidebarBackground = RGB(25, 25, 25),
    .sidebarText = RGB(220, 220, 220),
    .sidebarMuted = RGB(140, 140, 140),
    .sidebarSelection = RGB(55, 55, 61),
    .sidebarHover = RGB(42, 42, 42),
    .chromeBackground = RGB(32, 32, 32),
    .chromeText = RGB(240, 240, 240),
    .menuHotBackground = RGB(55, 55, 55),
    .splitterHover = RGB(90, 130, 180),
};

int ScaleForWindow(HWND window, int value) {
    const UINT dpi = (window != nullptr) ? GetDpiForWindow(window) : 96;
    return MulDiv(value, static_cast<int>(dpi), 96);
}

const ThemeColors& CurrentTheme() {
    return g_theme == AppTheme::Dark ? kDarkTheme : kLightTheme;
}

void ApplyWindowChrome(HWND window) {
    ResolveDarkModeApis();
    const bool dark = g_theme == AppTheme::Dark;

    if (g_setPreferredAppMode != nullptr) {
        constexpr int kForceDark = 2;
        constexpr int kForceLight = 3;
        g_setPreferredAppMode(dark ? kForceDark : kForceLight);
    }
    if (g_allowDarkModeForWindow != nullptr) {
        g_allowDarkModeForWindow(window, dark);
    }
    if (g_flushMenuThemes != nullptr) {
        g_flushMenuThemes();
    }

    const BOOL useDarkMode = dark;
    DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));

    const DWORD cornerPreference = DWMWCP_ROUND;
    DwmSetWindowAttribute(window, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));

    // Opaque caption tinted to the app. Backdrop effects would punch holes in GDI text.
    const ThemeColors& theme = CurrentTheme();
    const COLORREF captionColor = theme.chromeBackground;
    const COLORREF textColor = theme.chromeText;
    DwmSetWindowAttribute(window, DWMWA_CAPTION_COLOR, &captionColor, sizeof(captionColor));
    DwmSetWindowAttribute(window, DWMWA_BORDER_COLOR, &captionColor, sizeof(captionColor));
    DwmSetWindowAttribute(window, DWMWA_TEXT_COLOR, &textColor, sizeof(textColor));
}

void ApplyDarkScrollbar(HWND control) {
    if (control == nullptr) {
        return;
    }

    ResolveDarkModeApis();
    const bool dark = g_theme == AppTheme::Dark;

    if (g_allowDarkModeForWindow != nullptr) {
        g_allowDarkModeForWindow(control, dark);
    }

    SetWindowTheme(control, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    SetWindowPos(control, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

}
