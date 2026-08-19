#include "DocumentActions.h"

#include <commdlg.h>

#include "../core/AppState.h"
#include "../core/Constants.h"
#include "../core/FileIO.h"
#include "../core/StringUtils.h"
#include "MainWindow.h"

namespace mdmate {

std::wstring ShowOpenDialog(HWND owner) {
    wchar_t fileBuffer[MAX_PATH]{};

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Markdown Files (*.md;*.markdown)\0*.md;*.markdown\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER;
    ofn.lpstrDefExt = L"md";

    if (GetOpenFileNameW(&ofn)) {
        return fileBuffer;
    }

    return L"";
}

std::wstring ShowSaveDialog(HWND owner, const std::wstring& currentPath) {
    wchar_t fileBuffer[MAX_PATH]{};
    if (!currentPath.empty()) {
        wcsncpy_s(fileBuffer, currentPath.c_str(), _TRUNCATE);
    }

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Markdown Files (*.md;*.markdown)\0*.md;*.markdown\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    ofn.lpstrDefExt = L"md";

    if (GetSaveFileNameW(&ofn)) {
        return fileBuffer;
    }

    return L"";
}

bool SaveDocument(HWND window, bool saveAs) {
    std::wstring targetPath = g_currentFilePath;
    if (saveAs || targetPath.empty()) {
        targetPath = ShowSaveDialog(window, g_currentFilePath);
        if (targetPath.empty()) {
            return false;
        }
    }

    const std::wstring content = ReadControlText(g_editor);
    if (!SaveTextFileUtf8(targetPath, content)) {
        MessageBoxW(window, L"Could not save this file.", kAppTitle, MB_ICONERROR | MB_OK);
        return false;
    }

    g_currentFilePath = targetPath;
    g_isDirty = false;
    SendMessageW(g_editor, EM_SETMODIFY, FALSE, 0);
    UpdateWindowTitle();
    UpdateStatusText();
    return true;
}

bool MaybeSavePendingChanges(HWND window) {
    if (!g_isDirty) {
        return true;
    }

    const int result = MessageBoxW(window, L"You have unsaved changes. Save now?", kAppTitle,
                                   MB_ICONQUESTION | MB_YESNOCANCEL);
    if (result == IDCANCEL) {
        return false;
    }
    if (result == IDYES) {
        return SaveDocument(window, false);
    }
    return true;
}

bool LoadDocumentIntoEditor(HWND window, const std::wstring& path) {
    std::wstring content;
    if (!LoadTextFile(path, content)) {
        MessageBoxW(window, L"Could not open this file.", kAppTitle, MB_ICONERROR | MB_OK);
        return false;
    }

    g_suppressEditorChange = true;
    SetControlText(g_editor, content);
    g_suppressEditorChange = false;

    g_currentFilePath = path;
    g_isDirty = false;
    SendMessageW(g_editor, EM_SETMODIFY, FALSE, 0);

    RefreshPreview();
    UpdateWindowTitle();
    UpdateStatusText();
    return true;
}

void NewDocument(HWND window) {
    if (!MaybeSavePendingChanges(window)) {
        return;
    }

    g_suppressEditorChange = true;
    SetControlText(g_editor, L"");
    g_suppressEditorChange = false;

    g_currentFilePath.clear();
    g_isDirty = false;
    SendMessageW(g_editor, EM_SETMODIFY, FALSE, 0);

    RefreshPreview();
    UpdateWindowTitle();
    UpdateStatusText();
}

void OpenDocument(HWND window) {
    if (!MaybeSavePendingChanges(window)) {
        return;
    }

    const std::wstring path = ShowOpenDialog(window);
    if (!path.empty()) {
        LoadDocumentIntoEditor(window, path);
    }
}

}
