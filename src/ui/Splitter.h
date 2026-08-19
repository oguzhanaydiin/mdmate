#pragma once

#include <windows.h>

namespace mdmate {

// Handles splitter dragging and repainting.
LRESULT CALLBACK SplitterWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

}
