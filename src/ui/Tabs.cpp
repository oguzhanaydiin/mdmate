#include "Tabs.h"

#include <algorithm>

#include "../core/AppState.h"
#include "../core/Constants.h"
#include "../core/Session.h"
#include "../core/StringUtils.h"
#include "DocumentActions.h"
#include "FileExplorer.h"
#include "MainWindow.h"
#include "Theme.h"

namespace mdmate {

namespace {

int g_hotTab = -1;
int g_hotClose = -1;
bool g_hotNewTab = false;
RECT g_newTabRect{};

bool HasTabs() {
    return !g_tabs.empty() && g_activeTab >= 0 && g_activeTab < static_cast<int>(g_tabs.size());
}

bool PathsEqual(const std::wstring& a, const std::wstring& b) {
    if (a.size() != b.size()) {
        return false;
    }
    return _wcsicmp(a.c_str(), b.c_str()) == 0;
}

bool IsReusableEmptyTab() {
    return HasTabs() && g_tabs[static_cast<size_t>(g_activeTab)].path.empty() && !g_isDirty &&
           ReadControlText(g_editor).empty();
}

int FindTabByPath(const std::wstring& path) {
    if (path.empty()) {
        return -1;
    }
    for (int i = 0; i < static_cast<int>(g_tabs.size()); ++i) {
        if (PathsEqual(g_tabs[static_cast<size_t>(i)].path, path)) {
            return i;
        }
    }
    return -1;
}

std::wstring TabLabel(const DocumentTab& tab) {
    std::wstring name = tab.path.empty() ? L"Untitled" : GetFileNameFromPath(tab.path);
    if (tab.dirty) {
        name += L" •";
    }
    return name;
}

void ShowEmptyWorkspace(HWND window) {
    g_tabs.clear();
    g_activeTab = -1;
    g_currentFilePath.clear();
    g_isDirty = false;

    if (g_editor != nullptr) {
        g_suppressEditorChange = true;
        SetControlText(g_editor, L"");
        g_suppressEditorChange = false;
        SendMessageW(g_editor, EM_SETMODIFY, FALSE, 0);
    }

    RefreshPreview();
    UpdateWindowTitle();
    UpdateStatusText();
    InvalidateTabBar(window);
}

void ApplyActiveTab(HWND window) {
    if (!HasTabs()) {
        ShowEmptyWorkspace(window);
        return;
    }

    DocumentTab& tab = g_tabs[static_cast<size_t>(g_activeTab)];
    g_currentFilePath = tab.path;
    g_isDirty = tab.dirty;

    g_suppressEditorChange = true;
    SetControlText(g_editor, tab.text);
    g_suppressEditorChange = false;
    SendMessageW(g_editor, EM_SETMODIFY, tab.dirty ? TRUE : FALSE, 0);

    RefreshPreview();
    UpdateWindowTitle();
    UpdateStatusText();
    if (!tab.path.empty()) {
        RevealPathInFileTree(tab.path);
    }
    InvalidateTabBar(window);
}

void LayoutTabRects(HWND window) {
    RECT client{};
    GetClientRect(window, &client);

    const int y = ScaleForWindow(window, kToolbarHeight);
    const int h = TabBarHeight(window);
    const int pad = ScaleForWindow(window, 8);
    const int gap = ScaleForWindow(window, 2);
    const int plus = ScaleForWindow(window, 26);
    const int closeSize = ScaleForWindow(window, 18);
    const int textPad = ScaleForWindow(window, 12);
    const int minW = ScaleForWindow(window, 108);
    const int maxW = ScaleForWindow(window, 200);

    HDC dc = GetDC(window);
    HGDIOBJ oldFont = nullptr;
    if (g_uiFont != nullptr) {
        oldFont = SelectObject(dc, g_uiFont);
    }

    int x = pad;
    for (DocumentTab& tab : g_tabs) {
        RECT measure{};
        const std::wstring label = TabLabel(tab);
        DrawTextW(dc, label.c_str(), -1, &measure, DT_SINGLELINE | DT_CALCRECT);
        const int tabW =
            std::clamp(static_cast<int>(measure.right) + textPad + closeSize + ScaleForWindow(window, 10), minW, maxW);
        tab.rect = {x, y + ScaleForWindow(window, 5), x + tabW, y + h};
        tab.closeRect = {tab.rect.right - closeSize - ScaleForWindow(window, 6),
                         tab.rect.top + (tab.rect.bottom - tab.rect.top - closeSize) / 2,
                         0, 0};
        tab.closeRect.right = tab.closeRect.left + closeSize;
        tab.closeRect.bottom = tab.closeRect.top + closeSize;
        x += tabW + gap;
    }

    if (oldFont != nullptr) {
        SelectObject(dc, oldFont);
    }
    ReleaseDC(window, dc);

    g_newTabRect = {x, y + ScaleForWindow(window, 5), x + plus, y + h};
}

void FillRound(HDC dc, const RECT& rect, int radius, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
    RoundRect(dc, rect.left, rect.top, rect.right + 1, rect.bottom + 1, radius, radius);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
}

void StrokeRound(HDC dc, const RECT& rect, int radius, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

}

int TabBarHeight(HWND window) {
    return ScaleForWindow(window, kTabBarHeight);
}

void InvalidateTabBar(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    client.top = ScaleForWindow(window, kToolbarHeight);
    client.bottom = client.top + TabBarHeight(window);
    InvalidateRect(window, &client, TRUE);
}

void CaptureActiveTab() {
    if (!HasTabs() || g_editor == nullptr) {
        return;
    }

    DocumentTab& tab = g_tabs[static_cast<size_t>(g_activeTab)];
    tab.path = g_currentFilePath;
    tab.dirty = g_isDirty;
    tab.text = ReadControlText(g_editor);
}

void SyncActiveTabMeta() {
    if (!HasTabs()) {
        return;
    }
    DocumentTab& tab = g_tabs[static_cast<size_t>(g_activeTab)];
    tab.path = g_currentFilePath;
    tab.dirty = g_isDirty;
    if (g_mainWindow != nullptr) {
        InvalidateTabBar(g_mainWindow);
    }
}

bool ActivateTab(HWND window, int index) {
    if (index < 0 || index >= static_cast<int>(g_tabs.size())) {
        return false;
    }
    if (index == g_activeTab) {
        return true;
    }

    CaptureActiveTab();
    g_activeTab = index;
    ApplyActiveTab(window);
    SaveSession();
    return true;
}

void NewTab(HWND window) {
    CaptureActiveTab();
    g_tabs.push_back({});
    g_activeTab = static_cast<int>(g_tabs.size()) - 1;
    ApplyActiveTab(window);
    SaveSession();
}

bool CloseTab(HWND window, int index) {
    if (index < 0 || index >= static_cast<int>(g_tabs.size())) {
        return false;
    }

    if (HasTabs() && index != g_activeTab) {
        if (!ActivateTab(window, index)) {
            return false;
        }
    }

    if (!MaybeSavePendingChanges(window)) {
        return false;
    }
    CaptureActiveTab();

    g_tabs.erase(g_tabs.begin() + index);
    if (g_tabs.empty()) {
        ShowEmptyWorkspace(window);
        SaveSession();
        return true;
    }

    if (g_activeTab >= static_cast<int>(g_tabs.size())) {
        g_activeTab = static_cast<int>(g_tabs.size()) - 1;
    }
    ApplyActiveTab(window);
    SaveSession();
    return true;
}

bool CloseActiveTab(HWND window) {
    if (!HasTabs()) {
        return true;
    }
    return CloseTab(window, g_activeTab);
}

void NextTab(HWND window) {
    if (g_tabs.size() < 2) {
        return;
    }
    ActivateTab(window, (g_activeTab + 1) % static_cast<int>(g_tabs.size()));
}

void PrevTab(HWND window) {
    if (g_tabs.size() < 2) {
        return;
    }
    const int count = static_cast<int>(g_tabs.size());
    ActivateTab(window, (g_activeTab + count - 1) % count);
}

bool OpenPathInTab(HWND window, const std::wstring& path) {
    const int existing = FindTabByPath(path);
    if (existing >= 0) {
        return ActivateTab(window, existing);
    }

    if (!IsReusableEmptyTab()) {
        CaptureActiveTab();
        g_tabs.push_back({});
        g_activeTab = static_cast<int>(g_tabs.size()) - 1;
    }

    if (!LoadDocumentIntoEditor(window, path)) {
        if (HasTabs() && g_tabs[static_cast<size_t>(g_activeTab)].path.empty() &&
            g_tabs[static_cast<size_t>(g_activeTab)].text.empty()) {
            g_tabs.erase(g_tabs.begin() + g_activeTab);
            if (g_tabs.empty()) {
                ShowEmptyWorkspace(window);
                return false;
            }
            if (g_activeTab >= static_cast<int>(g_tabs.size())) {
                g_activeTab = static_cast<int>(g_tabs.size()) - 1;
            }
            ApplyActiveTab(window);
        }
        return false;
    }

    CaptureActiveTab();
    InvalidateTabBar(window);
    return true;
}

bool MaybeSaveAllTabs(HWND window) {
    if (!HasTabs()) {
        return true;
    }

    CaptureActiveTab();
    const int start = g_activeTab;
    for (int i = 0; i < static_cast<int>(g_tabs.size()); ++i) {
        if (!g_tabs[static_cast<size_t>(i)].dirty) {
            continue;
        }
        if (!ActivateTab(window, i)) {
            return false;
        }
        if (!MaybeSavePendingChanges(window)) {
            return false;
        }
        CaptureActiveTab();
    }
    ActivateTab(window, std::clamp(start, 0, static_cast<int>(g_tabs.size()) - 1));
    return true;
}

void PaintTabBar(HWND window, HDC dc) {
    LayoutTabRects(window);

    const int saved = SaveDC(dc);
    RECT client{};
    GetClientRect(window, &client);
    const int y = ScaleForWindow(window, kToolbarHeight);
    const int h = TabBarHeight(window);
    const ThemeColors& theme = CurrentTheme();
    const int radius = ScaleForWindow(window, 8);

    RECT bar = client;
    bar.top = y;
    bar.bottom = y + h;
    HBRUSH background = CreateSolidBrush(theme.chromeBackground);
    FillRect(dc, &bar, background);
    DeleteObject(background);

    RECT rule = bar;
    rule.top = bar.bottom - 1;
    HBRUSH ruleBrush = CreateSolidBrush(theme.rule);
    FillRect(dc, &rule, ruleBrush);
    DeleteObject(ruleBrush);

    SetBkMode(dc, TRANSPARENT);
    if (g_uiFont != nullptr) {
        SelectObject(dc, g_uiFont);
    }

    for (int i = 0; i < static_cast<int>(g_tabs.size()); ++i) {
        const DocumentTab& tab = g_tabs[static_cast<size_t>(i)];
        const bool active = i == g_activeTab;
        const bool hot = i == g_hotTab && !active;

        RECT body = tab.rect;
        if (active) {
            body.bottom = bar.bottom;
        }
        FillRound(dc, body, radius, active ? theme.editorBackground : (hot ? theme.menuHotBackground : theme.chromeBackground));
        StrokeRound(dc, body, radius, theme.rule);
        if (active) {
            RECT seam{tab.rect.left + 1, bar.bottom - 1, tab.rect.right - 1, bar.bottom};
            HBRUSH seamBrush = CreateSolidBrush(theme.editorBackground);
            FillRect(dc, &seam, seamBrush);
            DeleteObject(seamBrush);
        }

        SetTextColor(dc, theme.chromeText);
        RECT textRect = tab.rect;
        textRect.left += ScaleForWindow(window, 12);
        textRect.right = tab.closeRect.left - ScaleForWindow(window, 4);
        textRect.bottom -= ScaleForWindow(window, 1);
        const std::wstring label = TabLabel(tab);
        DrawTextW(dc, label.c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);

        if (i == g_hotClose) {
            FillRound(dc, tab.closeRect, ScaleForWindow(window, 4), theme.sidebarHover);
        }
        SetTextColor(dc, i == g_hotClose ? theme.chromeText : theme.sidebarMuted);
        RECT closeRect = tab.closeRect;
        DrawTextW(dc, L"\u00D7", -1, &closeRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }

    FillRound(dc, g_newTabRect, radius, g_hotNewTab ? theme.menuHotBackground : theme.chromeBackground);
    StrokeRound(dc, g_newTabRect, radius, theme.rule);
    SetTextColor(dc, theme.chromeText);
    DrawTextW(dc, L"+", -1, &g_newTabRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    RestoreDC(dc, saved);
}

void HandleTabBarMouseMove(HWND window, POINT client) {
    LayoutTabRects(window);

    int hotTab = -1;
    int hotClose = -1;
    bool hotNew = PtInRect(&g_newTabRect, client) != FALSE;
    for (int i = 0; i < static_cast<int>(g_tabs.size()); ++i) {
        const DocumentTab& tab = g_tabs[static_cast<size_t>(i)];
        if (PtInRect(&tab.closeRect, client)) {
            hotClose = i;
            hotTab = i;
            break;
        }
        if (PtInRect(&tab.rect, client)) {
            hotTab = i;
            break;
        }
    }

    if (hotTab != g_hotTab || hotClose != g_hotClose || hotNew != g_hotNewTab) {
        g_hotTab = hotTab;
        g_hotClose = hotClose;
        g_hotNewTab = hotNew;
        InvalidateTabBar(window);
    }
}

void HandleTabBarMouseLeave(HWND window) {
    if (g_hotTab != -1 || g_hotClose != -1 || g_hotNewTab) {
        g_hotTab = -1;
        g_hotClose = -1;
        g_hotNewTab = false;
        InvalidateTabBar(window);
    }
}

bool HandleTabBarClick(HWND window, POINT client) {
    LayoutTabRects(window);
    if (PtInRect(&g_newTabRect, client)) {
        NewTab(window);
        return true;
    }
    for (int i = 0; i < static_cast<int>(g_tabs.size()); ++i) {
        const DocumentTab& tab = g_tabs[static_cast<size_t>(i)];
        if (PtInRect(&tab.closeRect, client)) {
            CloseTab(window, i);
            return true;
        }
        if (PtInRect(&tab.rect, client)) {
            ActivateTab(window, i);
            return true;
        }
    }
    return false;
}

bool HandleTabBarMiddleClick(HWND window, POINT client) {
    LayoutTabRects(window);
    for (int i = 0; i < static_cast<int>(g_tabs.size()); ++i) {
        if (PtInRect(&g_tabs[static_cast<size_t>(i)].rect, client)) {
            CloseTab(window, i);
            return true;
        }
    }
    return false;
}

}
