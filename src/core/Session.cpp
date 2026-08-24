#include "Session.h"

#include <shlobj.h>

#include "AppState.h"

namespace mdmate {

namespace {

constexpr wchar_t kSessionSection[] = L"Session";
constexpr wchar_t kFileKey[] = L"File";
constexpr wchar_t kFolderKey[] = L"Folder";

std::wstring SessionFilePath() {
    PWSTR localAppData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &localAppData)) ||
        localAppData == nullptr) {
        return {};
    }

    std::wstring path = localAppData;
    CoTaskMemFree(localAppData);

    path += L"\\MDMate";
    CreateDirectoryW(path.c_str(), nullptr);
    path += L"\\session.ini";
    return path;
}

}

void SaveSession() {
    const std::wstring iniPath = SessionFilePath();
    if (iniPath.empty()) {
        return;
    }

    WritePrivateProfileStringW(kSessionSection, kFileKey, g_currentFilePath.c_str(), iniPath.c_str());
    WritePrivateProfileStringW(kSessionSection, kFolderKey, g_currentFolderPath.c_str(), iniPath.c_str());
}

SessionPaths LoadSession() {
    SessionPaths session{};
    const std::wstring iniPath = SessionFilePath();
    if (iniPath.empty()) {
        return session;
    }

    wchar_t fileBuffer[32768]{};
    wchar_t folderBuffer[32768]{};
    GetPrivateProfileStringW(kSessionSection, kFileKey, L"", fileBuffer, 32768, iniPath.c_str());
    GetPrivateProfileStringW(kSessionSection, kFolderKey, L"", folderBuffer, 32768, iniPath.c_str());

    session.file = fileBuffer;
    session.folder = folderBuffer;
    return session;
}

}
