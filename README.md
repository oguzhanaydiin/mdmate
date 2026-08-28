# MDMate

Super lightweight, Windows 11–native Markdown editor. One file, no installer.

## Download

**MDMate 1.0** is the build to run: [MDMate-1.0.0.exe](https://github.com/oguzhanaydiin/mdmate/releases/download/v1.0.0/MDMate-1.0.0.exe)

The same file is in the repo at [`dist/MDMate-1.0.0.exe`](dist/MDMate-1.0.0.exe). `bin/` and `obj/` are only local Visual Studio output and stay out of git.

Windows 11. Open the exe and write. Last files and folder come back next time (`%LOCALAPPDATA%\MDMate`). Windows may warn because the file is not signed; you can still run it.

## Features

- Fast Markdown editing with a native Windows 11 UI
- Tabs for more than one file
- File explorer for `.md` and `.txt`
- Side-by-side preview
- Light and dark themes
- Remembers the last session
- Word, character, and line counts
- Drag and drop files into the app
- Fullscreen writing (`F11`)

## Keyboard Shortcuts

- `Ctrl+N`: New file
- `Ctrl+O`: Open file
- `Ctrl+K`: Open folder
- `Ctrl+S`: Save
- `Ctrl+Shift+S`: Save As
- `Ctrl+W`: Close tab
- `Ctrl+Tab` / `Ctrl+Shift+Tab`: Next / previous tab
- `F6`: Toggle preview
- `F11`: Toggle fullscreen

## Build from source

Open `MDMate.sln` in Visual Studio 2022 and build `Release | x64`.

```powershell
msbuild .\MDMate.sln /m /p:Configuration=Release /p:Platform=x64
```

Run:

```powershell
.\bin\Release\MDMate.exe
```

When you ship a build, copy it to `dist/` as `MDMate-x.y.z.exe`. Do not commit `bin/`, `obj/`, or `MDMate.pdb`.
