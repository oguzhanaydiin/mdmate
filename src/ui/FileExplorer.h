#pragma once

#include <windows.h>

#include <string>

namespace mdmate {

// Creates the file tree control (left panel) inside the given parent window.
HWND CreateFileExplorer(HWND parent);

// Populates the tree with the contents of a folder, replacing any prior contents.
void PopulateFileTree(const std::wstring& folderPath);

// Expands folders down to a file and selects it, if that file lives in the open folder.
void RevealPathInFileTree(const std::wstring& filePath);

// Applies the current theme's colors to the file tree.
void ApplyFileExplorerTheme();
void RecreateFileExplorerFonts(HWND owner);
void DestroyFileExplorerResources();

// Handles WM_NOTIFY messages targeted at the file tree control. Returns the value the
// window procedure should return for the message (relevant for custom draw notifications).
LRESULT HandleFileExplorerNotify(HWND window, LPARAM lParam);

// Shows a folder picker dialog and returns the chosen path (empty if cancelled).
std::wstring ShowFolderPickerDialog(HWND owner);

// Prompts the user for a folder and populates the tree with its contents.
void OpenFolder(HWND window);

}
