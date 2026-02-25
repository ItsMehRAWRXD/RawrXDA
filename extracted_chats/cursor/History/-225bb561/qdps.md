# ScreenPilot++ - Portable Build Instructions

## 🎨 Features
- **White editor** (top 2/3) - Type your code here
- **Dark console** (bottom 1/3) - Build output appears here
- Fully portable - no PATH or DLL dependencies needed
- Uses MinGW for compilation (included in `toolchains/`)

## 🚀 Quick Build

### Double-click to build:
```
build_mingw.bat
```

That's it! The script will:
1. Find MinGW in `toolchains/mingw64/`
2. Compile `src/main.cpp`
3. Create `ScreenPilotPP.exe`
4. Auto-launch the app

### Requirements
- Windows 7 or later
- MinGW-w64 in `toolchains/mingw64/` (already included)

## 📁 Project Structure
```
SP++/
├── build_mingw.bat       ← Double-click to build
├── ScreenPilotPP.exe     ← Your compiled app
├── src/
│   └── main.cpp          ← Source code with color fixes
└── toolchains/
    └── mingw64/          ← Portable MinGW compiler (5.3 GB)
        └── bin/
            └── g++.exe
```

## 🎯 Build Options

### Full rebuild:
```batch
build_mingw.bat
```

### Visual Studio (if you have it):
```batch
build_host.bat
```

## 🔧 Color Theme
The app uses CHARFORMAT2W with proper RichEdit initialization:
- **Editor**: White background (#FFFFFF) + Black text (#000000)
- **Console**: Dark background (#1E1E1E) + Light text (#DCDCDC)
- **Font**: Consolas 18pt

## 📦 Fully Static
The executable is built with `-static` flag, so it includes all dependencies.
No external DLLs required - just copy `ScreenPilotPP.exe` anywhere and run!

## 💡 Notes
- MinGW bin folder is temporarily added to PATH during compilation only
- After build completes, PATH is restored to original state
- Colors are set using CHARFORMAT2W (not CHARFORMAT) for proper rendering
- EM_SETBKGNDCOLOR with wParam=0 to use custom colors (not system defaults)

