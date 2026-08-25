#include "Session.h"

#include <shlobj.h>

#include <algorithm>
#include <string>

#include "AppState.h"

namespace mdmate {

namespace {

constexpr wchar_t kSessionSection[] = L"Session";
constexpr wchar_t kFileKey[] = L"File";
constexpr wchar_t kFolderKey[] = L"Folder";
constexpr wchar_t kCountKey[] = L"Count";
constexpr wchar_t kActiveKey[] = L"Active";

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

std::wstring FileKey(int index) {
    return L"File" + std::to_wstring(index);
}

}

void SaveSession() {
    const std::wstring iniPath = SessionFilePath();
    if (iniPath.empty()) {
        return;
    }

    WritePrivateProfileStringW(kSessionSection, kFolderKey, g_currentFolderPath.c_str(), iniPath.c_str());
    WritePrivateProfileStringW(kSessionSection, kActiveKey, std::to_wstring(std::max(0, g_activeTab)).c_str(),
                               iniPath.c_str());

    int stored = 0;
    for (const DocumentTab& tab : g_tabs) {
        if (tab.path.empty()) {
            continue;
        }
        WritePrivateProfileStringW(kSessionSection, FileKey(stored).c_str(), tab.path.c_str(), iniPath.c_str());
        ++stored;
    }
    WritePrivateProfileStringW(kSessionSection, kCountKey, std::to_wstring(stored).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(kSessionSection, kFileKey, g_currentFilePath.c_str(), iniPath.c_str());
}

SessionPaths LoadSession() {
    SessionPaths session{};
    const std::wstring iniPath = SessionFilePath();
    if (iniPath.empty()) {
        return session;
    }

    wchar_t folderBuffer[32768]{};
    GetPrivateProfileStringW(kSessionSection, kFolderKey, L"", folderBuffer, 32768, iniPath.c_str());
    session.folder = folderBuffer;
    session.active = GetPrivateProfileIntW(kSessionSection, kActiveKey, 0, iniPath.c_str());

    const int count = GetPrivateProfileIntW(kSessionSection, kCountKey, 0, iniPath.c_str());
    if (count > 0) {
        session.files.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) {
            wchar_t fileBuffer[32768]{};
            GetPrivateProfileStringW(kSessionSection, FileKey(i).c_str(), L"", fileBuffer, 32768, iniPath.c_str());
            if (fileBuffer[0] != L'\0') {
                session.files.emplace_back(fileBuffer);
            }
        }
    } else {
        wchar_t fileBuffer[32768]{};
        GetPrivateProfileStringW(kSessionSection, kFileKey, L"", fileBuffer, 32768, iniPath.c_str());
        if (fileBuffer[0] != L'\0') {
            session.files.emplace_back(fileBuffer);
        }
    }

    return session;
}

}
