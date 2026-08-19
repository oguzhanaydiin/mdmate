#include <windows.h>

#include <ole2.h>

#include <iterator>

#include "core/AppState.h"
#include "core/Constants.h"
#include "ui/MainWindow.h"
#include "ui/Splitter.h"

using namespace mdmate;

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int commandShow) {
    g_instance = instance;

    OleInitialize(nullptr);

    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32 != nullptr) {
        using SetDpiAwarenessContextFn = BOOL(WINAPI*)(HANDLE);
        const auto setDpiAwarenessContext =
            reinterpret_cast<SetDpiAwarenessContextFn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (setDpiAwarenessContext != nullptr) {
            setDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_IBEAM);
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kWindowClassName;

    if (!RegisterClassExW(&windowClass)) {
        return 1;
    }

    WNDCLASSEXW splitterClass{};
    splitterClass.cbSize = sizeof(splitterClass);
    splitterClass.lpfnWndProc = SplitterWndProc;
    splitterClass.hInstance = instance;
    splitterClass.hCursor = LoadCursorW(nullptr, IDC_SIZEWE);
    splitterClass.lpszClassName = kSplitterClassName;

    if (!RegisterClassExW(&splitterClass)) {
        return 1;
    }

    WNDCLASSEXW fileTreeSplitterClass{};
    fileTreeSplitterClass.cbSize = sizeof(fileTreeSplitterClass);
    fileTreeSplitterClass.lpfnWndProc = FileTreeSplitterWndProc;
    fileTreeSplitterClass.hInstance = instance;
    fileTreeSplitterClass.hCursor = LoadCursorW(nullptr, IDC_SIZEWE);
    fileTreeSplitterClass.lpszClassName = kFileTreeSplitterClassName;

    if (!RegisterClassExW(&fileTreeSplitterClass)) {
        return 1;
    }

    g_mainWindow = CreateWindowExW(0, kWindowClassName, kAppTitle,
                                   WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                   CW_USEDEFAULT, CW_USEDEFAULT, 1300, 840,
                                   nullptr, nullptr, instance, nullptr);

    if (g_mainWindow == nullptr) {
        return 1;
    }

    ACCEL accelerators[] = {
        {FVIRTKEY | FCONTROL, 'N', IDM_FILE_NEW},
        {FVIRTKEY | FCONTROL, 'O', IDM_FILE_OPEN},
        {FVIRTKEY | FCONTROL, 'K', IDM_FILE_OPEN_FOLDER},
        {FVIRTKEY | FCONTROL, 'S', IDM_FILE_SAVE},
        {FVIRTKEY | FCONTROL | FSHIFT, 'S', IDM_FILE_SAVE_AS},
        {FVIRTKEY | FCONTROL, 'B', IDM_VIEW_TOGGLE_EXPLORER},
        {FVIRTKEY, VK_F6, IDM_VIEW_TOGGLE_PREVIEW},
        {FVIRTKEY, VK_F11, IDM_VIEW_FULLSCREEN},
    };
    g_accelerators = CreateAcceleratorTableW(accelerators, static_cast<int>(std::size(accelerators)));

    ShowWindow(g_mainWindow, commandShow);
    UpdateWindow(g_mainWindow);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (g_accelerators == nullptr || !TranslateAcceleratorW(g_mainWindow, g_accelerators, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    if (g_accelerators != nullptr) {
        DestroyAcceleratorTable(g_accelerators);
        g_accelerators = nullptr;
    }

    OleUninitialize();

    return static_cast<int>(message.wParam);
}
