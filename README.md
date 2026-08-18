# MDMate

Super lightweight, Windows-native Markdown editor inspired by Ghostwriter.

## Stack

- C++20
- Win32 API (native controls)
- RichEdit (`Msftedit.dll`)

## Features

- Fast Markdown editing with a distraction-free native UI
- Side-by-side lightweight preview (plain rendered markdown)
- UTF-8 save/load support for `.md` and `.markdown`
- Word, character, and line counters in status bar
- Drag and drop files into the app
- Fullscreen writing mode (`F11`)

## Build (Visual Studio)

Open `MDMate.sln` in Visual Studio 2022 and build `Release | x64`.

## Build (Developer Command Prompt)

```powershell
msbuild .\MDMate.sln /m /p:Configuration=Release /p:Platform=x64
```

Run:

```powershell
.\bin\Release\MDMate.exe
```

## Keyboard Shortcuts

- `Ctrl+N`: New file
- `Ctrl+O`: Open file
- `Ctrl+S`: Save
- `Ctrl+Shift+S`: Save As
- `F6`: Toggle preview
- `F11`: Toggle fullscreen
