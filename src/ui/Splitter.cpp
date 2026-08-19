#include "Splitter.h"

#include <algorithm>

#include "../core/AppState.h"
#include "../core/Constants.h"
#include "MainWindow.h"
#include "Theme.h"

namespace mdmate {

LRESULT CALLBACK SplitterWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_LBUTTONDOWN:
            SetCapture(window);
            g_isDraggingSplitter = true;
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
                    MoveWindow(window, editorWidth, 0, kSplitterWidth, g_contentHeight, TRUE);
                }
            }
            return 0;

        case WM_SETCURSOR:
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            return TRUE;

        case WM_ERASEBKGND: {
            RECT rect{};
            GetClientRect(window, &rect);
            HBRUSH brush = CreateSolidBrush(CurrentTheme().rule);
            FillRect(reinterpret_cast<HDC>(wParam), &rect, brush);
            DeleteObject(brush);
            return 1;
        }

        default:
            break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

}
