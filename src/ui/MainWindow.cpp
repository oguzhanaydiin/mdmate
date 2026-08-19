#include "MainWindow.h"

#include <commctrl.h>
#include <richedit.h>
#include <shellapi.h>

#include <algorithm>
#include <cwctype>
#include <string>
#include <string_view>

#include "../core/AppState.h"
#include "../core/Constants.h"
#include "../core/StringUtils.h"
#include "../markdown/PreviewDocument.h"
#include "DocumentActions.h"
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
    SendMessageW(g_status, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(buffer));
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
    const int contentHeight = std::max(0, height - statusHeight);
    g_contentHeight = contentHeight;

    HDWP layout = BeginDeferWindowPos(g_showPreview ? 3 : 2);

    if (g_showPreview) {
        const int editorWidth = std::clamp(static_cast<int>(width * g_splitRatio), 0, std::max(0, width - kSplitterWidth));
        layout = DeferWindowPos(layout, g_editor, nullptr, 0, 0, editorWidth, contentHeight, SWP_NOZORDER);
        layout = DeferWindowPos(layout, g_splitter, nullptr, editorWidth, 0, kSplitterWidth, contentHeight,
                                 SWP_NOZORDER);
        layout = DeferWindowPos(layout, g_preview, nullptr, editorWidth + kSplitterWidth, 0,
                                 width - editorWidth - kSplitterWidth, contentHeight, SWP_NOZORDER);
        ShowWindow(g_splitter, SW_SHOW);
        ShowWindow(g_preview, SW_SHOW);
    } else {
        layout = DeferWindowPos(layout, g_editor, nullptr, 0, 0, width, contentHeight, SWP_NOZORDER);
        ShowWindow(g_splitter, SW_HIDE);
        ShowWindow(g_preview, SW_HIDE);
    }

    if (layout != nullptr) {
        EndDeferWindowPos(layout);
    }

    MoveWindow(g_status, 0, contentHeight, width, statusHeight, TRUE);
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

HMENU BuildMainMenu() {
    HMENU mainMenu = CreateMenu();
    HMENU fileMenu = CreatePopupMenu();
    HMENU viewMenu = CreatePopupMenu();
    HMENU themeMenu = CreatePopupMenu();
    HMENU helpMenu = CreatePopupMenu();

    AppendMenuW(fileMenu, MF_STRING, IDM_FILE_NEW, L"&New\tCtrl+N");
    AppendMenuW(fileMenu, MF_STRING, IDM_FILE_OPEN, L"&Open...\tCtrl+O");
    AppendMenuW(fileMenu, MF_STRING, IDM_FILE_SAVE, L"&Save\tCtrl+S");
    AppendMenuW(fileMenu, MF_STRING, IDM_FILE_SAVE_AS, L"Save &As...\tCtrl+Shift+S");
    AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(fileMenu, MF_STRING, IDM_FILE_EXIT, L"E&xit\tAlt+F4");

    AppendMenuW(themeMenu, MF_STRING | (g_theme == AppTheme::Light ? MF_CHECKED : MF_UNCHECKED), IDM_VIEW_THEME_LIGHT,
                L"&Light");
    AppendMenuW(themeMenu, MF_STRING | (g_theme == AppTheme::Dark ? MF_CHECKED : MF_UNCHECKED), IDM_VIEW_THEME_DARK,
                L"&Dark");
    AppendMenuW(themeMenu, MF_STRING | (g_theme == AppTheme::Pixel ? MF_CHECKED : MF_UNCHECKED),
                IDM_VIEW_THEME_PIXEL, L"&Pixel");

    AppendMenuW(viewMenu, MF_STRING, IDM_VIEW_TOGGLE_PREVIEW, L"Toggle &Preview\tF6");
    AppendMenuW(viewMenu, MF_STRING, IDM_VIEW_FULLSCREEN, L"&Fullscreen\tF11");
    AppendMenuW(viewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(viewMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(themeMenu), L"&Theme");

    AppendMenuW(helpMenu, MF_STRING, IDM_HELP_ABOUT, L"&About");

    AppendMenuW(mainMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"&File");
    AppendMenuW(mainMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(viewMenu), L"&View");
    AppendMenuW(mainMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(helpMenu), L"&Help");

    return mainMenu;
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
                WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE |
                                                        ES_AUTOVSCROLL | ES_NOHIDESEL,
                0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_EDITOR)), g_instance,
                nullptr);

            g_preview = CreateWindowExW(
                WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE |
                                                        ES_AUTOVSCROLL | ES_NOHIDESEL | ES_READONLY,
                0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PREVIEW)), g_instance,
                nullptr);

            g_splitter = CreateWindowExW(0, kSplitterClassName, L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window,
                                         nullptr, g_instance, nullptr);

            g_status = CreateWindowExW(0, STATUSCLASSNAMEW, L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window,
                                       reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_STATUS)), g_instance,
                                       nullptr);

            g_editorFont = CreateFontW(-20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                       FIXED_PITCH | FF_MODERN, L"Consolas");

            g_previewFont = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            SendMessageW(g_editor, WM_SETFONT, reinterpret_cast<WPARAM>(g_editorFont), TRUE);
            SendMessageW(g_preview, WM_SETFONT, reinterpret_cast<WPARAM>(g_previewFont), TRUE);

            SendMessageW(g_editor, EM_SETLIMITTEXT, 0, 0);
            SendMessageW(g_preview, EM_SETLIMITTEXT, 0, 0);
            SendMessageW(g_editor, EM_SETEVENTMASK, 0, ENM_CHANGE);

            ApplyEditorTheme();

            DragAcceptFiles(window, TRUE);

            SetMenu(window, BuildMainMenu());
            UpdateWindowTitle();
            UpdateStatusText();
            RefreshPreview();
            return 0;
        }

        case WM_SIZE:
            LayoutControls(window);
            return 0;

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

            switch (id) {
                case IDM_FILE_NEW:
                    NewDocument(window);
                    return 0;
                case IDM_FILE_OPEN:
                    OpenDocument(window);
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
                    if (g_showPreview) {
                        RefreshPreview();
                    }
                    return 0;
                case IDM_VIEW_FULLSCREEN:
                    ToggleFullscreen(window);
                    return 0;
                case IDM_VIEW_THEME_LIGHT:
                case IDM_VIEW_THEME_DARK:
                case IDM_VIEW_THEME_PIXEL: {
                    if (id == IDM_VIEW_THEME_DARK) {
                        g_theme = AppTheme::Dark;
                    } else if (id == IDM_VIEW_THEME_PIXEL) {
                        g_theme = AppTheme::Pixel;
                    } else {
                        g_theme = AppTheme::Light;
                    }
                    ApplyEditorTheme();
                    RefreshPreview();

                    HMENU menu = GetMenu(window);
                    CheckMenuItem(menu, IDM_VIEW_THEME_LIGHT,
                                  MF_BYCOMMAND | (g_theme == AppTheme::Light ? MF_CHECKED : MF_UNCHECKED));
                    CheckMenuItem(menu, IDM_VIEW_THEME_DARK,
                                  MF_BYCOMMAND | (g_theme == AppTheme::Dark ? MF_CHECKED : MF_UNCHECKED));
                    CheckMenuItem(menu, IDM_VIEW_THEME_PIXEL,
                                  MF_BYCOMMAND | (g_theme == AppTheme::Pixel ? MF_CHECKED : MF_UNCHECKED));
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
            if (g_editorFont != nullptr) {
                DeleteObject(g_editorFont);
                g_editorFont = nullptr;
            }
            if (g_previewFont != nullptr) {
                DeleteObject(g_previewFont);
                g_previewFont = nullptr;
            }
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

}
