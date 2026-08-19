#pragma once

#include <windows.h>

#include <string>

namespace mdmate {

extern HINSTANCE g_instance;
extern HWND g_mainWindow;
extern HWND g_editor;
extern HWND g_preview;
extern HWND g_splitter;
extern HWND g_status;
extern HWND g_fileTree;
extern HWND g_fileTreeSplitter;
extern HFONT g_editorFont;
extern HFONT g_previewFont;
extern HACCEL g_accelerators;

extern std::wstring g_currentFilePath;
extern bool g_isDirty;
extern bool g_showPreview;
extern bool g_suppressEditorChange;

extern bool g_isFullscreen;
extern WINDOWPLACEMENT g_windowPlacement;
extern DWORD g_windowStyle;
extern DWORD g_windowExStyle;

extern bool g_isDraggingSplitter;
extern double g_splitRatio;
extern int g_contentHeight;

extern bool g_isDraggingFileTreeSplitter;
extern int g_fileTreeWidth;
extern bool g_showFileTree;
extern std::wstring g_currentFolderPath;

}
