#include "FileExplorer.h"

#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <vector>

#include "../core/AppState.h"
#include "../core/Constants.h"
#include "DocumentActions.h"
#include "MainWindow.h"
#include "Theme.h"

namespace mdmate {

namespace {

constexpr int kFolderIconIndex = 0;
constexpr int kFileIconIndex = 1;

struct NodeData {
    std::wstring fullPath;
    bool isDirectory;
    bool childrenLoaded;
};

HTREEITEM InsertTreeNode(HTREEITEM parent, const std::filesystem::path& path, bool isDirectory) {
    const std::wstring name = path.filename().wstring();

    TVINSERTSTRUCTW insert{};
    insert.hParent = parent;
    insert.hInsertAfter = TVI_LAST;
    insert.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN;
    insert.item.pszText = const_cast<wchar_t*>(name.c_str());
    insert.item.iImage = isDirectory ? kFolderIconIndex : kFileIconIndex;
    insert.item.iSelectedImage = insert.item.iImage;
    insert.item.cChildren = isDirectory ? 1 : 0;
    insert.item.lParam = reinterpret_cast<LPARAM>(new NodeData{path.wstring(), isDirectory, false});

    return TreeView_InsertItem(g_fileTree, &insert);
}

void PopulateChildren(HTREEITEM parent, const std::wstring& path) {
    std::vector<std::filesystem::directory_entry> dirs;
    std::vector<std::filesystem::directory_entry> files;

    std::error_code ec;
    std::filesystem::directory_iterator it(path, std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::directory_iterator end;
    for (; !ec && it != end; it.increment(ec)) {
        const auto& entry = *it;
        std::error_code typeEc;
        if (entry.is_directory(typeEc)) {
            dirs.push_back(entry);
        } else if (entry.is_regular_file(typeEc)) {
            files.push_back(entry);
        }
    }

    const auto byName = [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b) {
        return _wcsicmp(a.path().filename().c_str(), b.path().filename().c_str()) < 0;
    };
    std::sort(dirs.begin(), dirs.end(), byName);
    std::sort(files.begin(), files.end(), byName);

    for (const auto& dir : dirs) {
        InsertTreeNode(parent, dir.path(), true);
    }
    for (const auto& file : files) {
        InsertTreeNode(parent, file.path(), false);
    }
}

}

HWND CreateFileExplorer(HWND parent) {
    g_fileTree = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
        0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_FILETREE)), g_instance, nullptr);

    HIMAGELIST imageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 2, 4);
    SHFILEINFOW info{};

    SHGetFileInfoW(L"folder", FILE_ATTRIBUTE_DIRECTORY, &info, sizeof(info),
                   SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    ImageList_AddIcon(imageList, info.hIcon);
    DestroyIcon(info.hIcon);

    SHGetFileInfoW(L"file.md", FILE_ATTRIBUTE_NORMAL, &info, sizeof(info),
                   SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    ImageList_AddIcon(imageList, info.hIcon);
    DestroyIcon(info.hIcon);

    TreeView_SetImageList(g_fileTree, imageList, TVSIL_NORMAL);

    ApplyFileExplorerTheme();
    return g_fileTree;
}

void PopulateFileTree(const std::wstring& folderPath) {
    g_currentFolderPath = folderPath;
    TreeView_DeleteAllItems(g_fileTree);

    if (folderPath.empty()) {
        return;
    }

    const std::filesystem::path root(folderPath);
    const std::wstring rootName = root.filename().wstring();

    TVINSERTSTRUCTW insert{};
    insert.hParent = TVI_ROOT;
    insert.hInsertAfter = TVI_LAST;
    insert.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    const std::wstring displayName = rootName.empty() ? folderPath : rootName;
    insert.item.pszText = const_cast<wchar_t*>(displayName.c_str());
    insert.item.iImage = kFolderIconIndex;
    insert.item.iSelectedImage = kFolderIconIndex;
    insert.item.lParam = reinterpret_cast<LPARAM>(new NodeData{folderPath, true, false});

    const HTREEITEM rootItem = TreeView_InsertItem(g_fileTree, &insert);
    PopulateChildren(rootItem, folderPath);
    TreeView_Expand(g_fileTree, rootItem, TVE_EXPAND);
}

void ApplyFileExplorerTheme() {
    if (g_fileTree == nullptr) {
        return;
    }

    const ThemeColors& theme = CurrentTheme();
    TreeView_SetBkColor(g_fileTree, theme.previewBackground);
    TreeView_SetTextColor(g_fileTree, theme.body);
    InvalidateRect(g_fileTree, nullptr, TRUE);
}

void HandleFileExplorerNotify(HWND window, LPARAM lParam) {
    const NMHDR* hdr = reinterpret_cast<const NMHDR*>(lParam);
    if (hdr == nullptr || hdr->idFrom != static_cast<UINT_PTR>(IDC_FILETREE)) {
        return;
    }

    if (hdr->code == TVN_ITEMEXPANDINGW) {
        const auto* expand = reinterpret_cast<const NMTREEVIEWW*>(lParam);
        auto* data = reinterpret_cast<NodeData*>(expand->itemNew.lParam);
        if (data != nullptr && data->isDirectory && !data->childrenLoaded && (expand->action & TVE_EXPAND)) {
            HTREEITEM child = TreeView_GetChild(g_fileTree, expand->itemNew.hItem);
            while (child != nullptr) {
                const HTREEITEM next = TreeView_GetNextSibling(g_fileTree, child);
                TreeView_DeleteItem(g_fileTree, child);
                child = next;
            }
            PopulateChildren(expand->itemNew.hItem, data->fullPath);
            data->childrenLoaded = true;
        }
        return;
    }

    if (hdr->code == TVN_DELETEITEMW) {
        const auto* deleted = reinterpret_cast<const NMTREEVIEWW*>(lParam);
        delete reinterpret_cast<NodeData*>(deleted->itemOld.lParam);
        return;
    }

    if (hdr->code == TVN_SELCHANGEDW) {
        const auto* sel = reinterpret_cast<const NMTREEVIEWW*>(lParam);
        auto* data = reinterpret_cast<NodeData*>(sel->itemNew.lParam);
        if (data != nullptr && !data->isDirectory) {
            if (!MaybeSavePendingChanges(window)) {
                return;
            }
            LoadDocumentIntoEditor(window, data->fullPath);
        }
        return;
    }
}

std::wstring ShowFolderPickerDialog(HWND owner) {
    wchar_t path[MAX_PATH]{};

    BROWSEINFOW info{};
    info.hwndOwner = owner;
    info.lpszTitle = L"Select a folder to open";
    info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST idList = SHBrowseForFolderW(&info);
    if (idList == nullptr) {
        return L"";
    }

    const bool ok = SHGetPathFromIDListW(idList, path);
    CoTaskMemFree(idList);
    return ok ? path : L"";
}

void OpenFolder(HWND window) {
    const std::wstring folder = ShowFolderPickerDialog(window);
    if (folder.empty()) {
        return;
    }

    PopulateFileTree(folder);
    g_showFileTree = true;
    LayoutControls(window);
}

}
