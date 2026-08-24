#include "FileExplorer.h"

#include <commctrl.h>
#include <shlobj.h>
#include <windowsx.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <system_error>
#include <vector>

#include "../core/AppState.h"
#include "../core/Constants.h"
#include "../core/Session.h"
#include "DocumentActions.h"
#include "MainWindow.h"
#include "Theme.h"

namespace mdmate {

namespace {

struct NodeData {
    std::wstring fullPath;
    bool isDirectory;
    bool childrenLoaded;
};

HFONT g_treeFont = nullptr;
HFONT g_symbolFont = nullptr;

constexpr wchar_t kChevronRight = L'\uE76C';
constexpr wchar_t kChevronDown = L'\uE70D';

void DrawFolderGlyph(HDC hdc, const RECT& rc) {
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    const int x = rc.left + w / 8;
    const int y = rc.top + h / 5;
    const int bodyW = (w * 3) / 4;
    const int bodyH = (h * 11) / 20;
    const int tabW = bodyW / 3;
    const int tabH = std::max(2, h / 7);
    const COLORREF fill = RGB(210, 168, 72);
    const COLORREF edge = RGB(176, 132, 48);

    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, edge);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);

    RECT tab{x, y, x + tabW, y + tabH + 1};
    RECT body{x, y + tabH, x + bodyW, y + tabH + bodyH};
    RoundRect(hdc, tab.left, tab.top, tab.right, tab.bottom + 2, 2, 2);
    RoundRect(hdc, body.left, body.top, body.right, body.bottom, 3, 3);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

int ItemDepth(HTREEITEM item) {
    int depth = 0;
    HTREEITEM parent = TreeView_GetParent(g_fileTree, item);
    while (parent != nullptr) {
        ++depth;
        parent = TreeView_GetParent(g_fileTree, parent);
    }
    return depth;
}

HTREEITEM InsertTreeNode(HTREEITEM parent, const std::filesystem::path& path, bool isDirectory) {
    const std::wstring name = path.filename().wstring();

    TVINSERTSTRUCTW insert{};
    insert.hParent = parent;
    insert.hInsertAfter = TVI_LAST;
    insert.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN;
    insert.item.pszText = const_cast<wchar_t*>(name.c_str());
    insert.item.cChildren = isDirectory ? 1 : 0;
    insert.item.lParam = reinterpret_cast<LPARAM>(new NodeData{path.wstring(), isDirectory, false});

    return TreeView_InsertItem(g_fileTree, &insert);
}

bool IsListedFile(const std::filesystem::path& path) {
    const std::wstring ext = path.extension().wstring();
    return _wcsicmp(ext.c_str(), L".md") == 0 || _wcsicmp(ext.c_str(), L".txt") == 0;
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
        } else if (entry.is_regular_file(typeEc) && IsListedFile(entry.path())) {
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

std::wstring NormalizePath(std::wstring path) {
    for (wchar_t& c : path) {
        if (c == L'/') {
            c = L'\\';
        } else {
            c = static_cast<wchar_t>(towlower(c));
        }
    }
    while (path.size() > 3 && path.back() == L'\\') {
        path.pop_back();
    }
    return path;
}

bool PathsEqual(const std::wstring& a, const std::wstring& b) {
    return NormalizePath(a) == NormalizePath(b);
}

bool IsPathUnderFolder(const std::wstring& filePath, const std::wstring& folderPath) {
    const std::wstring file = NormalizePath(filePath);
    const std::wstring folder = NormalizePath(folderPath);
    if (file.size() < folder.size()) {
        return false;
    }
    if (file.compare(0, folder.size(), folder) != 0) {
        return false;
    }
    return file.size() == folder.size() || file[folder.size()] == L'\\';
}

HTREEITEM FindChildByName(HTREEITEM parent, const std::wstring& name) {
    for (HTREEITEM child = TreeView_GetChild(g_fileTree, parent); child != nullptr;
         child = TreeView_GetNextSibling(g_fileTree, child)) {
        TVITEMW item{};
        item.mask = TVIF_PARAM;
        item.hItem = child;
        TreeView_GetItem(g_fileTree, &item);
        const auto* data = reinterpret_cast<const NodeData*>(item.lParam);
        if (data == nullptr) {
            continue;
        }
        const std::wstring childName = std::filesystem::path(data->fullPath).filename().wstring();
        if (_wcsicmp(childName.c_str(), name.c_str()) == 0) {
            return child;
        }
    }
    return nullptr;
}

}

HWND CreateFileExplorer(HWND parent) {
    g_fileTree = CreateWindowExW(
        0, WC_TREEVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | TVS_FULLROWSELECT | TVS_SHOWSELALWAYS | TVS_TRACKSELECT |
            TVS_DISABLEDRAGDROP | TVS_NOHSCROLL | TVS_INFOTIP,
        0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_FILETREE)), g_instance, nullptr);

    TreeView_SetExtendedStyle(g_fileTree, TVS_EX_DOUBLEBUFFER | TVS_EX_FADEINOUTEXPANDOS,
                              TVS_EX_DOUBLEBUFFER | TVS_EX_FADEINOUTEXPANDOS);

    RecreateFileExplorerFonts(parent);
    ApplyFileExplorerTheme();
    return g_fileTree;
}

void RecreateFileExplorerFonts(HWND owner) {
    if (g_treeFont != nullptr) {
        DeleteObject(g_treeFont);
        g_treeFont = nullptr;
    }
    if (g_symbolFont != nullptr) {
        DeleteObject(g_symbolFont);
        g_symbolFont = nullptr;
    }

    const int treePx = -ScaleForWindow(owner, 13);
    g_treeFont = CreateFontW(treePx, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    const int symbolPx = -ScaleForWindow(owner, 14);
    g_symbolFont = CreateFontW(symbolPx, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                               L"Segoe MDL2 Assets");

    if (g_fileTree != nullptr) {
        SendMessageW(g_fileTree, WM_SETFONT, reinterpret_cast<WPARAM>(g_treeFont), TRUE);
        SendMessageW(g_fileTree, TVM_SETITEMHEIGHT, static_cast<WPARAM>(ScaleForWindow(owner, 24)), 0);
        TreeView_SetIndent(g_fileTree, ScaleForWindow(owner, 16));
    }
}

void DestroyFileExplorerResources() {
    if (g_treeFont != nullptr) {
        DeleteObject(g_treeFont);
        g_treeFont = nullptr;
    }
    if (g_symbolFont != nullptr) {
        DeleteObject(g_symbolFont);
        g_symbolFont = nullptr;
    }
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
    insert.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN;
    const std::wstring displayName = rootName.empty() ? folderPath : rootName;
    insert.item.pszText = const_cast<wchar_t*>(displayName.c_str());
    insert.item.cChildren = 1;
    insert.item.lParam = reinterpret_cast<LPARAM>(new NodeData{folderPath, true, false});

    const HTREEITEM rootItem = TreeView_InsertItem(g_fileTree, &insert);
    PopulateChildren(rootItem, folderPath);
    auto* rootData = reinterpret_cast<NodeData*>(insert.item.lParam);
    if (rootData != nullptr) {
        rootData->childrenLoaded = true;
    }
    TreeView_Expand(g_fileTree, rootItem, TVE_EXPAND);
}

void RevealPathInFileTree(const std::wstring& filePath) {
    if (g_fileTree == nullptr || g_currentFolderPath.empty() || filePath.empty()) {
        return;
    }
    if (!IsPathUnderFolder(filePath, g_currentFolderPath)) {
        return;
    }

    const std::filesystem::path relative =
        std::filesystem::path(NormalizePath(filePath)).lexically_relative(NormalizePath(g_currentFolderPath));
    if (relative.empty() || relative == std::filesystem::path(L".") ||
        (!relative.empty() && *relative.begin() == L"..")) {
        return;
    }

    HTREEITEM item = TreeView_GetRoot(g_fileTree);
    if (item == nullptr) {
        return;
    }

    for (const auto& part : relative) {
        TreeView_Expand(g_fileTree, item, TVE_EXPAND);
        const HTREEITEM child = FindChildByName(item, part.wstring());
        if (child == nullptr) {
            return;
        }
        item = child;
    }

    TreeView_SelectItem(g_fileTree, item);
    TreeView_EnsureVisible(g_fileTree, item);
}

void ApplyFileExplorerTheme() {
    if (g_fileTree == nullptr) {
        return;
    }

    const ThemeColors& theme = CurrentTheme();
    TreeView_SetBkColor(g_fileTree, theme.sidebarBackground);
    TreeView_SetTextColor(g_fileTree, theme.sidebarText);
    TreeView_SetLineColor(g_fileTree, theme.sidebarBackground);
    InvalidateRect(g_fileTree, nullptr, TRUE);
}

LRESULT HandleFileTreeCustomDraw(LPARAM lParam) {
    auto* customDraw = reinterpret_cast<LPNMTVCUSTOMDRAW>(lParam);
    switch (customDraw->nmcd.dwDrawStage) {
        case CDDS_PREPAINT:
            return CDRF_NOTIFYITEMDRAW;
        case CDDS_ITEMPREPAINT: {
            HDC hdc = customDraw->nmcd.hdc;
            const int saved = SaveDC(hdc);
            const ThemeColors& theme = CurrentTheme();
            const HTREEITEM treeItem = reinterpret_cast<HTREEITEM>(customDraw->nmcd.dwItemSpec);

            wchar_t text[260]{};
            TVITEMW item{};
            item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_STATE;
            item.stateMask = TVIS_EXPANDED | TVIS_SELECTED;
            item.hItem = treeItem;
            item.pszText = text;
            item.cchTextMax = 260;
            TreeView_GetItem(g_fileTree, &item);
            const auto* data = reinterpret_cast<const NodeData*>(item.lParam);

            RECT row = customDraw->nmcd.rc;
            RECT itemRect{};
            if (TreeView_GetItemRect(g_fileTree, treeItem, &itemRect, FALSE)) {
                row = itemRect;
            }

            const bool selected = (item.state & TVIS_SELECTED) != 0 ||
                                  (customDraw->nmcd.uItemState & (CDIS_SELECTED | CDIS_FOCUS)) != 0;
            const bool hot = (customDraw->nmcd.uItemState & CDIS_HOT) != 0;

            HBRUSH rowBrush = CreateSolidBrush(theme.sidebarBackground);
            FillRect(hdc, &row, rowBrush);
            DeleteObject(rowBrush);

            if (selected || hot) {
                RECT pill = row;
                pill.left += ScaleForWindow(g_fileTree, 4);
                pill.right -= ScaleForWindow(g_fileTree, 4);
                pill.top += 1;
                pill.bottom -= 1;
                HBRUSH fill = CreateSolidBrush(selected ? theme.sidebarSelection : theme.sidebarHover);
                HPEN pen = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
                HGDIOBJ oldBrush = SelectObject(hdc, fill);
                HGDIOBJ oldPen = SelectObject(hdc, pen);
                const int radius = ScaleForWindow(g_fileTree, 6);
                RoundRect(hdc, pill.left, pill.top, pill.right, pill.bottom, radius, radius);
                SelectObject(hdc, oldBrush);
                SelectObject(hdc, oldPen);
                DeleteObject(fill);
                DeleteObject(pen);
            }

            const int depth = ItemDepth(treeItem);
            const int indent = ScaleForWindow(g_fileTree, 16);
            const int glyph = ScaleForWindow(g_fileTree, 16);
            int x = ScaleForWindow(g_fileTree, 8) + depth * indent;
            const bool isFolder = data != nullptr && data->isDirectory;
            const bool expanded = (item.state & TVIS_EXPANDED) != 0;

            SetBkMode(hdc, TRANSPARENT);
            if (g_symbolFont != nullptr) {
                SelectObject(hdc, g_symbolFont);
            }

            RECT glyphRect = row;
            glyphRect.left = x;
            glyphRect.right = x + glyph;
            if (isFolder) {
                SetTextColor(hdc, theme.sidebarMuted);
                const wchar_t chevron = expanded ? kChevronDown : kChevronRight;
                DrawTextW(hdc, &chevron, 1, &glyphRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
            }
            x += glyph;

            glyphRect.left = x;
            glyphRect.right = x + glyph;
            if (isFolder) {
                DrawFolderGlyph(hdc, glyphRect);
            }
            x += glyph + ScaleForWindow(g_fileTree, 6);

            if (g_treeFont != nullptr) {
                SelectObject(hdc, g_treeFont);
            }
            SetTextColor(hdc, theme.sidebarText);
            RECT textRect = row;
            textRect.left = x;
            textRect.right -= ScaleForWindow(g_fileTree, 8);
            DrawTextW(hdc, text, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);

            RestoreDC(hdc, saved);
            return CDRF_SKIPDEFAULT;
        }
        default:
            return CDRF_DODEFAULT;
    }
}

LRESULT HandleFileExplorerNotify(HWND window, LPARAM lParam) {
    const NMHDR* hdr = reinterpret_cast<const NMHDR*>(lParam);
    if (hdr == nullptr || hdr->idFrom != static_cast<UINT_PTR>(IDC_FILETREE)) {
        return 0;
    }

    if (hdr->code == NM_CUSTOMDRAW) {
        return HandleFileTreeCustomDraw(lParam);
    }

    if (hdr->code == NM_CLICK) {
        TVHITTESTINFO hit{};
        const DWORD pos = GetMessagePos();
        hit.pt.x = GET_X_LPARAM(pos);
        hit.pt.y = GET_Y_LPARAM(pos);
        ScreenToClient(g_fileTree, &hit.pt);
        const HTREEITEM item = TreeView_HitTest(g_fileTree, &hit);
        if (item == nullptr) {
            return 0;
        }

        TVITEMW tv{};
        tv.mask = TVIF_PARAM;
        tv.hItem = item;
        TreeView_GetItem(g_fileTree, &tv);
        const auto* data = reinterpret_cast<const NodeData*>(tv.lParam);
        if (data == nullptr || !data->isDirectory) {
            return 0;
        }

        const int depth = ItemDepth(item);
        const int indent = ScaleForWindow(g_fileTree, 16);
        const int glyph = ScaleForWindow(g_fileTree, 16);
        const int chevronLeft = ScaleForWindow(g_fileTree, 8) + depth * indent;
        if (hit.pt.x >= chevronLeft && hit.pt.x < chevronLeft + glyph) {
            TreeView_Expand(g_fileTree, item, TVE_TOGGLE);
        }
        return 0;
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
        return 0;
    }

    if (hdr->code == TVN_DELETEITEMW) {
        const auto* deleted = reinterpret_cast<const NMTREEVIEWW*>(lParam);
        delete reinterpret_cast<NodeData*>(deleted->itemOld.lParam);
        return 0;
    }

    if (hdr->code == TVN_SELCHANGEDW) {
        const auto* sel = reinterpret_cast<const NMTREEVIEWW*>(lParam);
        auto* data = reinterpret_cast<NodeData*>(sel->itemNew.lParam);
        if (data != nullptr && !data->isDirectory) {
            if (PathsEqual(data->fullPath, g_currentFilePath)) {
                return 0;
            }
            if (!MaybeSavePendingChanges(window)) {
                return 0;
            }
            LoadDocumentIntoEditor(window, data->fullPath);
        }
        return 0;
    }

    return 0;
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
    SaveSession();
    LayoutControls(window);
}

}
