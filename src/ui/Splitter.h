#pragma once

#include <windows.h>

namespace mdmate {

// Handles splitter dragging and repainting.
LRESULT CALLBACK SplitterWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

// Handles dragging and repainting of the splitter between the file explorer and the editor.
LRESULT CALLBACK FileTreeSplitterWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

}
