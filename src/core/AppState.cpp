#include "AppState.h"

#include "Constants.h"

#include <string>
#include <vector>

namespace mdmate {

HINSTANCE g_instance = nullptr;
HWND g_mainWindow = nullptr;
HWND g_editor = nullptr;
HWND g_preview = nullptr;
HWND g_splitter = nullptr;
HWND g_status = nullptr;
HWND g_fileTree = nullptr;
HWND g_fileTreeSplitter = nullptr;
HWND g_explorerToggleButton = nullptr;
HWND g_openFolderButton = nullptr;
HFONT g_editorFont = nullptr;
HFONT g_previewFont = nullptr;
HFONT g_uiFont = nullptr;
HFONT g_iconFont = nullptr;
HACCEL g_accelerators = nullptr;

std::wstring g_currentFilePath;
bool g_isDirty = false;
bool g_showPreview = true;
bool g_suppressEditorChange = false;

bool g_isFullscreen = false;
WINDOWPLACEMENT g_windowPlacement{sizeof(WINDOWPLACEMENT)};
DWORD g_windowStyle = 0;
DWORD g_windowExStyle = 0;

bool g_isDraggingSplitter = false;
double g_splitRatio = 0.58;
int g_contentTop = 0;
int g_contentHeight = 0;

bool g_isDraggingFileTreeSplitter = false;
int g_fileTreeWidth = kDefaultFileTreeWidth;
bool g_showFileTree = true;
std::wstring g_currentFolderPath;

std::vector<DocumentTab> g_tabs;
int g_activeTab = -1;

}
