#include "Splitter.h"

#include <algorithm>

#include "../core/AppState.h"
#include "../core/Constants.h"
#include "MainWindow.h"
#include "Theme.h"

namespace mdmate {

namespace {

HWND g_hoveredSplitter = nullptr;

void TrackSplitterHover(HWND window) {
    if (g_hoveredSplitter != window) {
        g_hoveredSplitter = window;
        InvalidateRect(window, nullptr, TRUE);
    }

    TRACKMOUSEEVENT tracking{};
    tracking.cbSize = sizeof(tracking);
    tracking.dwFlags = TME_LEAVE;
    tracking.hwndTrack = window;
    TrackMouseEvent(&tracking);
}

void ClearSplitterHover(HWND window) {
    if (g_hoveredSplitter == window) {
        g_hoveredSplitter = nullptr;
        InvalidateRect(window, nullptr, TRUE);
    }
}

void PaintSplitterBackground(HWND window, HDC dc) {
    RECT rect{};
    GetClientRect(window, &rect);
    const ThemeColors& theme = CurrentTheme();

    // Opaque fill in the content gap; glass only belongs in the DWM-extended chrome.
    HBRUSH baseBrush = CreateSolidBrush(theme.editorBackground);
    FillRect(dc, &rect, baseBrush);
    DeleteObject(baseBrush);

    const bool active = (window == g_hoveredSplitter);
    const int lineWidth = active ? 2 : 1;
    RECT line = rect;
    line.left = (rect.left + rect.right - lineWidth) / 2;
    line.right = line.left + lineWidth;
    HBRUSH lineBrush = CreateSolidBrush(active ? theme.splitterHover : theme.rule);
    FillRect(dc, &line, lineBrush);
    DeleteObject(lineBrush);
}

}

LRESULT CALLBACK SplitterWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_LBUTTONDOWN:
            SetCapture(window);
            g_isDraggingSplitter = true;
            TrackSplitterHover(window);
            return 0;

        case WM_LBUTTONUP:
            if (g_isDraggingSplitter) {
                g_isDraggingSplitter = false;
                ReleaseCapture();
                if (g_mainWindow != nullptr) {
                    LayoutControls(g_mainWindow);
                }
            }
            return 0;

        case WM_CAPTURECHANGED:
            if (g_isDraggingSplitter) {
                g_isDraggingSplitter = false;
                if (g_mainWindow != nullptr) {
                    LayoutControls(g_mainWindow);
                }
            }
            return 0;

        case WM_MOUSEMOVE:
            TrackSplitterHover(window);
            if (g_isDraggingSplitter && g_mainWindow != nullptr) {
                POINT cursor{};
                GetCursorPos(&cursor);
                ScreenToClient(g_mainWindow, &cursor);

                RECT client{};
                GetClientRect(g_mainWindow, &client);
                const int width = client.right - client.left;
                if (width > 0) {
                    const double ratio = static_cast<double>(cursor.x) / static_cast<double>(width);
                    g_splitRatio = std::clamp(ratio, kMinSplitRatio, kMaxSplitRatio);

                    // Defer pane reflow until the drag ends to avoid Rich Edit redraw cost.
                    const int editorWidth =
                        std::clamp(static_cast<int>(width * g_splitRatio), 0, std::max(0, width - kSplitterWidth));
                    MoveWindow(window, editorWidth, g_contentTop, kSplitterWidth, g_contentHeight, TRUE);
                }
            }
            return 0;

        case WM_MOUSELEAVE:
            ClearSplitterHover(window);
            return 0;

        case WM_SETCURSOR:
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            return TRUE;

        case WM_ERASEBKGND:
            PaintSplitterBackground(window, reinterpret_cast<HDC>(wParam));
            return 1;

        default:
            break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT CALLBACK FileTreeSplitterWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_LBUTTONDOWN:
            SetCapture(window);
            g_isDraggingFileTreeSplitter = true;
            TrackSplitterHover(window);
            return 0;

        case WM_LBUTTONUP:
            if (g_isDraggingFileTreeSplitter) {
                g_isDraggingFileTreeSplitter = false;
                ReleaseCapture();
                if (g_mainWindow != nullptr) {
                    LayoutControls(g_mainWindow);
                }
            }
            return 0;

        case WM_CAPTURECHANGED:
            if (g_isDraggingFileTreeSplitter) {
                g_isDraggingFileTreeSplitter = false;
                if (g_mainWindow != nullptr) {
                    LayoutControls(g_mainWindow);
                }
            }
            return 0;

        case WM_MOUSEMOVE:
            TrackSplitterHover(window);
            if (g_isDraggingFileTreeSplitter && g_mainWindow != nullptr) {
                POINT cursor{};
                GetCursorPos(&cursor);
                ScreenToClient(g_mainWindow, &cursor);

                RECT client{};
                GetClientRect(g_mainWindow, &client);
                const int width = client.right - client.left;
                const int maxWidth = std::max(kMinFileTreeWidth, width - kSplitterWidth * 2);
                g_fileTreeWidth =
                    std::clamp(static_cast<int>(cursor.x), kMinFileTreeWidth, std::min(kMaxFileTreeWidth, maxWidth));

                // Defer pane reflow until the drag ends to avoid Rich Edit redraw cost.
                MoveWindow(window, g_fileTreeWidth, g_contentTop, kSplitterWidth, g_contentHeight, TRUE);
            }
            return 0;

        case WM_MOUSELEAVE:
            ClearSplitterHover(window);
            return 0;

        case WM_SETCURSOR:
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            return TRUE;

        case WM_ERASEBKGND:
            PaintSplitterBackground(window, reinterpret_cast<HDC>(wParam));
            return 1;

        default:
            break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

}
