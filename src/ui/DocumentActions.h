#pragma once

#include <windows.h>

#include <string>

namespace mdmate {

// Opens the standard file dialogs.
std::wstring ShowOpenDialog(HWND owner);
std::wstring ShowSaveDialog(HWND owner, const std::wstring& currentPath);

// Handles document save, load, and transition prompts.
bool SaveDocument(HWND window, bool saveAs);
bool MaybeSavePendingChanges(HWND window);
bool LoadDocumentIntoEditor(HWND window, const std::wstring& path);

void NewDocument(HWND window);
void OpenDocument(HWND window);

}
