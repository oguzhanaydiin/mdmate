#pragma once

#include <windows.h>

namespace mdmate {

inline constexpr wchar_t kWindowClassName[] = L"MDMateWindowClass";
inline constexpr wchar_t kAppTitle[] = L"MDMate";

inline constexpr int IDC_EDITOR = 101;
inline constexpr int IDC_PREVIEW = 102;
inline constexpr int IDC_STATUS = 103;

inline constexpr wchar_t kSplitterClassName[] = L"MDMateSplitterClass";
inline constexpr int kSplitterWidth = 6;
inline constexpr double kMinSplitRatio = 0.15;
inline constexpr double kMaxSplitRatio = 0.85;

inline constexpr UINT_PTR kPreviewTimerId = 1;
inline constexpr UINT kPreviewDelayMs = 120;

inline constexpr int IDM_FILE_NEW = 40001;
inline constexpr int IDM_FILE_OPEN = 40002;
inline constexpr int IDM_FILE_SAVE = 40003;
inline constexpr int IDM_FILE_SAVE_AS = 40004;
inline constexpr int IDM_FILE_EXIT = 40005;
inline constexpr int IDM_VIEW_TOGGLE_PREVIEW = 40101;
inline constexpr int IDM_VIEW_FULLSCREEN = 40102;
inline constexpr int IDM_VIEW_THEME_LIGHT = 40103;
inline constexpr int IDM_VIEW_THEME_DARK = 40104;
inline constexpr int IDM_VIEW_THEME_PIXEL = 40105;
inline constexpr int IDM_HELP_ABOUT = 40201;

}
