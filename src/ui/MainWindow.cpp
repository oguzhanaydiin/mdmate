#include "MainWindow.h"

#include <commctrl.h>
#include <richedit.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <windowsx.h>

#include <algorithm>
#include <cwctype>
#include <string>
#include <string_view>

#include "../core/AppState.h"
#include "../core/Constants.h"
#include "../core/StringUtils.h"
#include "../markdown/PreviewDocument.h"
#include "DocumentActions.h"
#include "FileExplorer.h"
#include "PreviewRenderer.h"
#include "Theme.h"

namespace mdmate {

namespace {

int CountWords(std::wstring_view text) {
    int words = 0;
    bool inWord = false;

    for (const wchar_t c : text) {
        const bool isWordChar = std::iswalnum(c) || c == L'\'';
        if (isWordChar) {
            if (!inWord) {
                ++words;
                inWord = true;
            }
        } else {
            inWord = false;
        }
    }

    return words;
}

std::wstring g_statusText;

constexpr UINT_PTR kStatusSubclassId = 1;
constexpr wchar_t kIconExplorerShown[] = L"\uE76B";
constexpr wchar_t kIconExplorerHidden[] = L"\uE76C";

struct TitleMenu {
    const wchar_t* label;
    HMENU popup;
    RECT rect;
};

TitleMenu g_titleMenus[3] = {
    {L"File", nullptr, {}},
    {L"View", nullptr, {}},
    {L"Help", nullptr, {}},
};
HMENU g_themeMenu = nullptr;
int g_hotTitleMenu = -1;

int ToolbarHeight(HWND window) {
    return ScaleForWindow(window, kToolbarHeight);
}

// Toolbar buttons sit flush left; the menu strip starts where they end.
int ToolbarButtonsRight(HWND window) {
    return ScaleForWindow(window, 8 + kToggleButtonWidth + 6 + kOpenFolderButtonWidth);
}

LRESULT CALLBACK StatusSubclassProc(HWND status, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId,
                                    DWORD_PTR /*refData*/) {
    if (message == WM_ERASEBKGND) {
        RECT client{};
        GetClientRect(status, &client);
        HBRUSH background = CreateSolidBrush(CurrentTheme().chromeBackground);
        FillRect(reinterpret_cast<HDC>(wParam), &client, background);
        DeleteObject(background);
        return 1;
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(status, StatusSubclassProc, subclassId);
    }
    return DefSubclassProc(status, message, wParam, lParam);
}

void DeleteOwnedFont(HFONT& font) {
    if (font != nullptr) {
        DeleteObject(font);
        font = nullptr;
    }
}

void RecreateUiFonts(HWND window) {
    DeleteOwnedFont(g_uiFont);
    DeleteOwnedFont(g_iconFont);
    DeleteOwnedFont(g_editorFont);
    DeleteOwnedFont(g_previewFont);

    g_uiFont = CreateFontW(-ScaleForWindow(window, 13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                           L"Segoe UI");
    g_iconFont = CreateFontW(-ScaleForWindow(window, 14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                             L"Segoe MDL2 Assets");
    g_editorFont = CreateFontW(-ScaleForWindow(window, 15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN,
                               L"Cascadia Code");
    g_previewFont = CreateFontW(-ScaleForWindow(window, 14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                L"Segoe UI");

    if (g_editor != nullptr) {
        SendMessageW(g_editor, WM_SETFONT, reinterpret_cast<WPARAM>(g_editorFont), TRUE);
    }
    if (g_preview != nullptr) {
        SendMessageW(g_preview, WM_SETFONT, reinterpret_cast<WPARAM>(g_previewFont), TRUE);
    }
}

void SyncMenuChecks() {
    if (g_titleMenus[1].popup != nullptr) {
        CheckMenuItem(g_titleMenus[1].popup, IDM_VIEW_TOGGLE_EXPLORER,
                      MF_BYCOMMAND | (g_showFileTree ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(g_titleMenus[1].popup, IDM_VIEW_TOGGLE_PREVIEW,
                      MF_BYCOMMAND | (g_showPreview ? MF_CHECKED : MF_UNCHECKED));
    }
    if (g_themeMenu != nullptr) {
        CheckMenuItem(g_themeMenu, IDM_VIEW_THEME_LIGHT,
                      MF_BYCOMMAND | (g_theme == AppTheme::Light ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(g_themeMenu, IDM_VIEW_THEME_DARK,
                      MF_BYCOMMAND | (g_theme == AppTheme::Dark ? MF_CHECKED : MF_UNCHECKED));
    }
}

void CreateAppMenus() {
    HMENU fileMenu = CreatePopupMenu();
    HMENU viewMenu = CreatePopupMenu();
    HMENU helpMenu = CreatePopupMenu();
    g_themeMenu = CreatePopupMenu();

    AppendMenuW(fileMenu, MF_STRING, IDM_FILE_NEW, L"&New\tCtrl+N");
    AppendMenuW(fileMenu, MF_STRING, IDM_FILE_OPEN, L"&Open...\tCtrl+O");
    AppendMenuW(fileMenu, MF_STRING, IDM_FILE_OPEN_FOLDER, L"Open &Folder...\tCtrl+K");
    AppendMenuW(fileMenu, MF_STRING, IDM_FILE_SAVE, L"&Save\tCtrl+S");
    AppendMenuW(fileMenu, MF_STRING, IDM_FILE_SAVE_AS, L"Save &As...\tCtrl+Shift+S");
    AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(fileMenu, MF_STRING, IDM_FILE_EXIT, L"E&xit\tAlt+F4");

    AppendMenuW(g_themeMenu, MF_STRING, IDM_VIEW_THEME_LIGHT, L"&Light");
    AppendMenuW(g_themeMenu, MF_STRING, IDM_VIEW_THEME_DARK, L"&Dark");

    AppendMenuW(viewMenu, MF_STRING, IDM_VIEW_TOGGLE_PREVIEW, L"Toggle &Preview\tF6");
    AppendMenuW(viewMenu, MF_STRING, IDM_VIEW_TOGGLE_EXPLORER, L"Toggle &Explorer\tCtrl+B");
    AppendMenuW(viewMenu, MF_STRING, IDM_VIEW_FULLSCREEN, L"&Fullscreen\tF11");
    AppendMenuW(viewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(viewMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(g_themeMenu), L"&Theme");

    AppendMenuW(helpMenu, MF_STRING, IDM_HELP_ABOUT, L"&About");

    g_titleMenus[0].popup = fileMenu;
    g_titleMenus[1].popup = viewMenu;
    g_titleMenus[2].popup = helpMenu;
    SyncMenuChecks();
}

void DestroyAppMenus() {
    if (g_titleMenus[0].popup != nullptr) {
        DestroyMenu(g_titleMenus[0].popup);
    }
    if (g_titleMenus[1].popup != nullptr) {
        DestroyMenu(g_titleMenus[1].popup);
    }
    if (g_titleMenus[2].popup != nullptr) {
        DestroyMenu(g_titleMenus[2].popup);
    }
    g_titleMenus[0].popup = nullptr;
    g_titleMenus[1].popup = nullptr;
    g_titleMenus[2].popup = nullptr;
    g_themeMenu = nullptr;
}

int HitTestTitleMenu(POINT client) {
    for (int i = 0; i < 3; ++i) {
        if (PtInRect(&g_titleMenus[i].rect, client)) {
            return i;
        }
    }
    return -1;
}

void ShowTitleMenu(HWND window, int index) {
    if (index < 0 || index > 2 || g_titleMenus[index].popup == nullptr) {
        return;
    }

    SyncMenuChecks();
    POINT screen{g_titleMenus[index].rect.left, g_titleMenus[index].rect.bottom};
    ClientToScreen(window, &screen);
    TrackPopupMenu(g_titleMenus[index].popup, TPM_LEFTALIGN | TPM_TOPALIGN, screen.x, screen.y, 0, window, nullptr);
}

void InvalidateToolbar(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    client.bottom = ToolbarHeight(window);
    InvalidateRect(window, &client, TRUE);
}

void PaintWindowChrome(HWND window, HDC dc) {
    RECT client{};
    GetClientRect(window, &client);

    const ThemeColors& theme = CurrentTheme();
    const int toolbarH = ToolbarHeight(window);

    RECT toolbar = client;
    toolbar.bottom = toolbarH;
    HBRUSH background = CreateSolidBrush(theme.chromeBackground);
    FillRect(dc, &toolbar, background);
    DeleteObject(background);

    RECT content = client;
    content.top = toolbarH;
    HBRUSH opaque = CreateSolidBrush(theme.editorBackground);
    FillRect(dc, &content, opaque);
    DeleteObject(opaque);

    if (g_uiFont != nullptr) {
        SelectObject(dc, g_uiFont);
    }
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, theme.chromeText);

    const int itemPad = ScaleForWindow(window, 12);
    const int itemInset = ScaleForWindow(window, 5);
    int x = ToolbarButtonsRight(window) + ScaleForWindow(window, 10);
    for (int i = 0; i < 3; ++i) {
        RECT measure{0, 0, 0, 0};
        DrawTextW(dc, g_titleMenus[i].label, -1, &measure, DT_SINGLELINE | DT_CALCRECT);

        RECT item{x, itemInset, x + measure.right + itemPad * 2, toolbarH - itemInset};
        g_titleMenus[i].rect = {item.left, 0, item.right, toolbarH};

        if (g_hotTitleMenu == i) {
            HBRUSH hot = CreateSolidBrush(theme.menuHotBackground);
            HPEN pen = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
            HGDIOBJ oldBrush = SelectObject(dc, hot);
            HGDIOBJ oldPen = SelectObject(dc, pen);
            const int radius = ScaleForWindow(window, 6);
            RoundRect(dc, item.left, item.top, item.right, item.bottom, radius, radius);
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(hot);
            DeleteObject(pen);
        }

        DrawTextW(dc, g_titleMenus[i].label, -1, &item, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        x = item.right + ScaleForWindow(window, 2);
    }

    RECT rule = client;
    rule.top = toolbarH - 1;
    rule.bottom = toolbarH;
    HBRUSH ruleBrush = CreateSolidBrush(theme.rule);
    FillRect(dc, &rule, ruleBrush);
    DeleteObject(ruleBrush);
}

}

void UpdateWindowTitle() {
    std::wstring name = g_currentFilePath.empty() ? L"Untitled.md" : GetFileNameFromPath(g_currentFilePath);
    if (g_isDirty) {
        name += L" *";
    }

    const std::wstring title = name + L" - " + kAppTitle;
    SetWindowTextW(g_mainWindow, title.c_str());
}

void UpdateStatusText() {
    const std::wstring text = ReadControlText(g_editor);
    const int words = CountWords(text);
    const int chars = static_cast<int>(text.size());
    const int lines = static_cast<int>(SendMessageW(g_editor, EM_GETLINECOUNT, 0, 0));

    wchar_t buffer[256]{};
    swprintf_s(buffer, L"Words: %d   Chars: %d   Lines: %d", words, chars, lines);
    g_statusText = buffer;
    SendMessageW(g_status, SB_SETTEXTW, SBT_OWNERDRAW, 0);
    InvalidateRect(g_status, nullptr, TRUE);
}

void RefreshPreview() {
    if (!g_showPreview || g_preview == nullptr) {
        return;
    }

    const std::wstring markdown = ReadControlText(g_editor);
    const PreviewDocument preview = RenderMarkdownPreview(markdown);
    ApplyPreviewStyles(preview);
}

void QueuePreviewRefresh(HWND window) {
    KillTimer(window, kPreviewTimerId);
    SetTimer(window, kPreviewTimerId, kPreviewDelayMs, nullptr);
}

void OnEditorChanged(HWND window) {
    if (!g_suppressEditorChange) {
        g_isDirty = true;
        UpdateWindowTitle();
        QueuePreviewRefresh(window);
    }
}

void LayoutControls(HWND window) {
    RECT client{};
    GetClientRect(window, &client);

    SendMessageW(g_status, WM_SIZE, 0, 0);

    RECT statusRect{};
    GetWindowRect(g_status, &statusRect);
    const int statusHeight = statusRect.bottom - statusRect.top;

    const int width = static_cast<int>(client.right - client.left);
    const int height = static_cast<int>(client.bottom - client.top);
    const int chromeH = ToolbarHeight(window);
    const int contentHeight = std::max(0, height - statusHeight - chromeH);
    g_contentTop = chromeH;
    g_contentHeight = contentHeight;

    const int buttonHeight = chromeH - ScaleForWindow(window, 10);
    const int buttonY = (chromeH - buttonHeight) / 2;
    const int toggleX = ScaleForWindow(window, 8);
    const int toggleWidth = ScaleForWindow(window, kToggleButtonWidth);
    MoveWindow(g_explorerToggleButton, toggleX, buttonY, toggleWidth, buttonHeight, TRUE);
    MoveWindow(g_openFolderButton, toggleX + toggleWidth + ScaleForWindow(window, 6), buttonY,
               ScaleForWindow(window, kOpenFolderButtonWidth), buttonHeight, TRUE);

    const int controlCount = (g_showFileTree ? 2 : 0) + (g_showPreview ? 3 : 2);
    HDWP layout = BeginDeferWindowPos(controlCount);

    int leftOffset = 0;
    if (g_showFileTree) {
        const int treeWidth = std::clamp(g_fileTreeWidth, 0, std::max(0, width - kSplitterWidth));
        layout = DeferWindowPos(layout, g_fileTree, nullptr, 0, chromeH, treeWidth, contentHeight, SWP_NOZORDER);
        layout = DeferWindowPos(layout, g_fileTreeSplitter, nullptr, treeWidth, chromeH, kSplitterWidth, contentHeight,
                                 SWP_NOZORDER);
        ShowWindow(g_fileTree, SW_SHOW);
        ShowWindow(g_fileTreeSplitter, SW_SHOW);
        leftOffset = treeWidth + kSplitterWidth;
    } else {
        ShowWindow(g_fileTree, SW_HIDE);
        ShowWindow(g_fileTreeSplitter, SW_HIDE);
    }

    const int remainingWidth = std::max(0, width - leftOffset);

    if (g_showPreview) {
        const int editorWidth =
            std::clamp(static_cast<int>(remainingWidth * g_splitRatio), 0, std::max(0, remainingWidth - kSplitterWidth));
        layout = DeferWindowPos(layout, g_editor, nullptr, leftOffset, chromeH, editorWidth, contentHeight, SWP_NOZORDER);
        layout = DeferWindowPos(layout, g_splitter, nullptr, leftOffset + editorWidth, chromeH, kSplitterWidth,
                                 contentHeight, SWP_NOZORDER);
        layout = DeferWindowPos(layout, g_preview, nullptr, leftOffset + editorWidth + kSplitterWidth, chromeH,
                                 remainingWidth - editorWidth - kSplitterWidth, contentHeight, SWP_NOZORDER);
        ShowWindow(g_splitter, SW_SHOW);
        ShowWindow(g_preview, SW_SHOW);
    } else {
        layout = DeferWindowPos(layout, g_editor, nullptr, leftOffset, chromeH, remainingWidth, contentHeight,
                                 SWP_NOZORDER);
        ShowWindow(g_splitter, SW_HIDE);
        ShowWindow(g_preview, SW_HIDE);
    }

    if (layout != nullptr) {
        EndDeferWindowPos(layout);
    }

    MoveWindow(g_status, 0, chromeH + contentHeight, width, statusHeight, TRUE);
}

void ToggleFileExplorer(HWND window) {
    g_showFileTree = !g_showFileTree;
    LayoutControls(window);

    SetWindowTextW(g_explorerToggleButton, g_showFileTree ? kIconExplorerShown : kIconExplorerHidden);
    InvalidateRect(g_explorerToggleButton, nullptr, TRUE);
    SyncMenuChecks();
}

void ToggleFullscreen(HWND window) {
    if (!g_isFullscreen) {
        g_windowStyle = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE));
        g_windowExStyle = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE));
        GetWindowPlacement(window, &g_windowPlacement);

        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(monitorInfo);
        GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitorInfo);

        SetWindowLongPtrW(window, GWL_STYLE, g_windowStyle & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(window, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
                     monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                     monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    } else {
        SetWindowLongPtrW(window, GWL_STYLE, g_windowStyle);
        SetWindowLongPtrW(window, GWL_EXSTYLE, g_windowExStyle);
        SetWindowPlacement(window, &g_windowPlacement);
        SetWindowPos(window, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }

    g_isFullscreen = !g_isFullscreen;
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            LoadLibraryW(L"Msftedit.dll");

            INITCOMMONCONTROLSEX commonControls{};
            commonControls.dwSize = sizeof(commonControls);
            commonControls.dwICC = ICC_BAR_CLASSES;
            InitCommonControlsEx(&commonControls);

            g_editor = CreateWindowExW(
                0, MSFTEDIT_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE |
                                                        ES_AUTOVSCROLL | ES_NOHIDESEL,
                0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_EDITOR)), g_instance,
                nullptr);

            g_preview = CreateWindowExW(
                0, MSFTEDIT_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE |
                                                        ES_AUTOVSCROLL | ES_NOHIDESEL | ES_READONLY,
                0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PREVIEW)), g_instance,
                nullptr);

            g_splitter = CreateWindowExW(0, kSplitterClassName, L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window,
                                         nullptr, g_instance, nullptr);

            g_fileTreeSplitter = CreateWindowExW(0, kFileTreeSplitterClassName, L"", WS_CHILD, 0, 0, 0, 0, window,
                                                 nullptr, g_instance, nullptr);

            g_explorerToggleButton = CreateWindowExW(
                0, L"BUTTON", kIconExplorerShown, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_TOGGLE_EXPLORER_BUTTON)), g_instance, nullptr);

            g_openFolderButton = CreateWindowExW(
                0, L"BUTTON", L"Open Folder", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_OPEN_FOLDER_BUTTON)), g_instance, nullptr);

            CreateFileExplorer(window);

            g_status = CreateWindowExW(0, STATUSCLASSNAMEW, L"", WS_CHILD | WS_VISIBLE | CCS_NODIVIDER, 0, 0, 0, 0,
                                       window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_STATUS)), g_instance,
                                       nullptr);
            SetWindowTheme(g_status, L"", L"");
            SetWindowSubclass(g_status, StatusSubclassProc, kStatusSubclassId, 0);

            RecreateUiFonts(window);

            SendMessageW(g_editor, EM_SETLIMITTEXT, 0, 0);
            SendMessageW(g_preview, EM_SETLIMITTEXT, 0, 0);
            SendMessageW(g_editor, EM_SETEVENTMASK, 0, ENM_CHANGE);

            DragAcceptFiles(window, TRUE);
            CreateAppMenus();
            ApplyWindowChrome(window);
            ApplyDarkScrollbar(g_editor);
            ApplyDarkScrollbar(g_preview);
            ApplyDarkScrollbar(g_fileTree);
            ApplyEditorTheme();
            UpdateWindowTitle();
            UpdateStatusText();
            RefreshPreview();
            return 0;
        }

        case WM_SIZE:
            LayoutControls(window);
            InvalidateToolbar(window);
            return 0;

        case WM_DPICHANGED: {
            const auto* suggested = reinterpret_cast<const RECT*>(lParam);
            SetWindowPos(window, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                         suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
            RecreateUiFonts(window);
            RecreateFileExplorerFonts(window);
            ApplyWindowChrome(window);
            LayoutControls(window);
            return 0;
        }

        case WM_SHOWWINDOW:
            if (wParam != 0) {
                ApplyWindowChrome(window);
            }
            break;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT paint{};
            const HDC dc = BeginPaint(window, &paint);
            PaintWindowChrome(window, dc);
            EndPaint(window, &paint);
            return 0;
        }

        case WM_MOUSEMOVE: {
            POINT client{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            const int hot = HitTestTitleMenu(client);
            if (hot != g_hotTitleMenu) {
                g_hotTitleMenu = hot;
                InvalidateToolbar(window);
            }

            TRACKMOUSEEVENT tracking{};
            tracking.cbSize = sizeof(tracking);
            tracking.dwFlags = TME_LEAVE;
            tracking.hwndTrack = window;
            TrackMouseEvent(&tracking);
            break;
        }

        case WM_MOUSELEAVE:
            if (g_hotTitleMenu != -1) {
                g_hotTitleMenu = -1;
                InvalidateToolbar(window);
            }
            return 0;

        case WM_LBUTTONDOWN: {
            POINT client{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            const int menuIndex = HitTestTitleMenu(client);
            if (menuIndex >= 0) {
                ShowTitleMenu(window, menuIndex);
                return 0;
            }
            break;
        }

        case WM_SYSKEYDOWN:
            if (wParam == 'F') {
                ShowTitleMenu(window, 0);
                return 0;
            }
            if (wParam == 'V') {
                ShowTitleMenu(window, 1);
                return 0;
            }
            if (wParam == 'H') {
                ShowTitleMenu(window, 2);
                return 0;
            }
            break;

        case WM_DRAWITEM: {
            const auto* drawItem = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            if (drawItem == nullptr) {
                break;
            }

            const ThemeColors& theme = CurrentTheme();

            if (drawItem->CtlID == static_cast<UINT>(IDC_STATUS)) {
                HBRUSH background = CreateSolidBrush(theme.chromeBackground);
                FillRect(drawItem->hDC, &drawItem->rcItem, background);
                DeleteObject(background);

                if (g_uiFont != nullptr) {
                    SelectObject(drawItem->hDC, g_uiFont);
                }
                RECT textRect = drawItem->rcItem;
                textRect.left += 8;
                SetBkMode(drawItem->hDC, TRANSPARENT);
                SetTextColor(drawItem->hDC, theme.chromeText);
                DrawTextW(drawItem->hDC, g_statusText.c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
                return TRUE;
            }

            if (drawItem->CtlID == static_cast<UINT>(IDC_TOGGLE_EXPLORER_BUTTON) ||
                drawItem->CtlID == static_cast<UINT>(IDC_OPEN_FOLDER_BUTTON)) {
                const bool pressed = (drawItem->itemState & ODS_SELECTED) != 0;
                HBRUSH background = CreateSolidBrush(theme.chromeBackground);
                FillRect(drawItem->hDC, &drawItem->rcItem, background);
                DeleteObject(background);

                if (pressed) {
                    HBRUSH hot = CreateSolidBrush(theme.menuHotBackground);
                    HPEN pen = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
                    HGDIOBJ oldBrush = SelectObject(drawItem->hDC, hot);
                    HGDIOBJ oldPen = SelectObject(drawItem->hDC, pen);
                    const int radius = ScaleForWindow(window, 6);
                    RoundRect(drawItem->hDC, drawItem->rcItem.left, drawItem->rcItem.top, drawItem->rcItem.right,
                              drawItem->rcItem.bottom, radius, radius);
                    SelectObject(drawItem->hDC, oldBrush);
                    SelectObject(drawItem->hDC, oldPen);
                    DeleteObject(hot);
                    DeleteObject(pen);
                }

                wchar_t caption[64]{};
                GetWindowTextW(drawItem->hwndItem, caption, static_cast<int>(sizeof(caption) / sizeof(caption[0])));

                const bool iconButton = drawItem->CtlID == static_cast<UINT>(IDC_TOGGLE_EXPLORER_BUTTON);
                HFONT font = iconButton ? g_iconFont : g_uiFont;
                if (font != nullptr) {
                    SelectObject(drawItem->hDC, font);
                }

                SetBkMode(drawItem->hDC, TRANSPARENT);
                SetTextColor(drawItem->hDC, theme.chromeText);
                RECT textRect = drawItem->rcItem;
                DrawTextW(drawItem->hDC, caption, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
                return TRUE;
            }
            break;
        }

        case WM_DROPFILES: {
            if (!MaybeSavePendingChanges(window)) {
                DragFinish(reinterpret_cast<HDROP>(wParam));
                return 0;
            }

            wchar_t path[MAX_PATH]{};
            DragQueryFileW(reinterpret_cast<HDROP>(wParam), 0, path, MAX_PATH);
            DragFinish(reinterpret_cast<HDROP>(wParam));
            LoadDocumentIntoEditor(window, path);
            return 0;
        }

        case WM_TIMER:
            if (wParam == kPreviewTimerId) {
                KillTimer(window, kPreviewTimerId);
                RefreshPreview();
                UpdateStatusText();
                return 0;
            }
            break;

        case WM_COMMAND: {
            const int id = LOWORD(wParam);
            const int notification = HIWORD(wParam);

            if (id == IDC_EDITOR && notification == EN_CHANGE) {
                OnEditorChanged(window);
                return 0;
            }

            if (id == IDC_TOGGLE_EXPLORER_BUTTON && notification == BN_CLICKED) {
                ToggleFileExplorer(window);
                return 0;
            }

            if (id == IDC_OPEN_FOLDER_BUTTON && notification == BN_CLICKED) {
                OpenFolder(window);
                return 0;
            }

            switch (id) {
                case IDM_FILE_NEW:
                    NewDocument(window);
                    return 0;
                case IDM_FILE_OPEN:
                    OpenDocument(window);
                    return 0;
                case IDM_FILE_OPEN_FOLDER:
                    OpenFolder(window);
                    return 0;
                case IDM_FILE_SAVE:
                    SaveDocument(window, false);
                    return 0;
                case IDM_FILE_SAVE_AS:
                    SaveDocument(window, true);
                    return 0;
                case IDM_FILE_EXIT:
                    SendMessageW(window, WM_CLOSE, 0, 0);
                    return 0;
                case IDM_VIEW_TOGGLE_PREVIEW:
                    g_showPreview = !g_showPreview;
                    LayoutControls(window);
                    SyncMenuChecks();
                    if (g_showPreview) {
                        RefreshPreview();
                    }
                    return 0;
                case IDM_VIEW_FULLSCREEN:
                    ToggleFullscreen(window);
                    return 0;
                case IDM_VIEW_TOGGLE_EXPLORER:
                    ToggleFileExplorer(window);
                    return 0;
                case IDM_VIEW_THEME_LIGHT:
                case IDM_VIEW_THEME_DARK: {
                    g_theme = (id == IDM_VIEW_THEME_DARK) ? AppTheme::Dark : AppTheme::Light;
                    ApplyWindowChrome(window);
                    ApplyDarkScrollbar(g_editor);
                    ApplyDarkScrollbar(g_preview);
                    ApplyDarkScrollbar(g_fileTree);
                    ApplyEditorTheme();
                    RefreshPreview();
                    ApplyFileExplorerTheme();
                    InvalidateRect(g_status, nullptr, TRUE);
                    InvalidateRect(g_splitter, nullptr, TRUE);
                    InvalidateRect(g_fileTreeSplitter, nullptr, TRUE);
                    InvalidateRect(g_explorerToggleButton, nullptr, TRUE);
                    InvalidateRect(g_openFolderButton, nullptr, TRUE);
                    InvalidateRect(window, nullptr, TRUE);
                    SyncMenuChecks();
                    return 0;
                }
                case IDM_HELP_ABOUT:
                    MessageBoxW(window,
                                L"MDMate\nA native, ultra-lightweight Markdown editor for Windows.\n\n"
                                L"Shortcuts:\n"
                                L"Ctrl+N New\nCtrl+O Open\nCtrl+S Save\nCtrl+Shift+S Save As\n"
                                L"F6 Toggle Preview\nF11 Fullscreen",
                                kAppTitle, MB_OK | MB_ICONINFORMATION);
                    return 0;
                default:
                    break;
            }
            break;
        }

        case WM_NOTIFY: {
            const NMHDR* hdr = reinterpret_cast<const NMHDR*>(lParam);
            if (hdr != nullptr && hdr->idFrom == IDC_EDITOR && hdr->code == EN_CHANGE) {
                OnEditorChanged(window);
                return 0;
            }
            if (hdr != nullptr && hdr->idFrom == IDC_FILETREE) {
                return HandleFileExplorerNotify(window, lParam);
            }
            break;
        }

        case WM_CLOSE:
            if (!MaybeSavePendingChanges(window)) {
                return 0;
            }
            DestroyWindow(window);
            return 0;

        case WM_DESTROY:
            KillTimer(window, kPreviewTimerId);
            DestroyAppMenus();
            DestroyFileExplorerResources();
            DeleteOwnedFont(g_editorFont);
            DeleteOwnedFont(g_previewFont);
            DeleteOwnedFont(g_uiFont);
            DeleteOwnedFont(g_iconFont);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

}
