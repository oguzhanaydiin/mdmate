#pragma once

#include <windows.h>

#include <string>

namespace mdmate {

int TabBarHeight(HWND window);
void InvalidateTabBar(HWND window);
void PaintTabBar(HWND window, HDC dc);
void CaptureActiveTab();
void SyncActiveTabMeta();
bool ActivateTab(HWND window, int index);
void NewTab(HWND window);
bool CloseTab(HWND window, int index);
bool CloseActiveTab(HWND window);
void NextTab(HWND window);
void PrevTab(HWND window);
bool OpenPathInTab(HWND window, const std::wstring& path);
bool MaybeSaveAllTabs(HWND window);

void HandleTabBarMouseMove(HWND window, POINT client);
void HandleTabBarMouseLeave(HWND window);
bool HandleTabBarClick(HWND window, POINT client);
bool HandleTabBarMiddleClick(HWND window, POINT client);

}
