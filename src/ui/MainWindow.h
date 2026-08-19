#pragma once

#include <windows.h>

namespace mdmate {

// Dispatches messages for the main editor window.
LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

// Updates the editor layout and window metadata.
void LayoutControls(HWND window);

void UpdateWindowTitle();
void UpdateStatusText();
void RefreshPreview();
void QueuePreviewRefresh(HWND window);
void OnEditorChanged(HWND window);
void ToggleFullscreen(HWND window);

HMENU BuildMainMenu();

}
