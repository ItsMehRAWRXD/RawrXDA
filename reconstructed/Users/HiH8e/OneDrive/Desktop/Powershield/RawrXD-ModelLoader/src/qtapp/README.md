# QuantumIDEApp (Qt Widgets Scaffold)

This standalone Qt Widgets app prototypes the QuantumIDE UI: Goal Bar, Agent Control Panel, center code tab (`FILLAME.JS`) with an AI suggestion overlay, right Proposal Review dock, and a bottom QShell tab. It is dependency-light and compiler-friendly (MSVC/MinGW) with Qt6/Qt5 fallback.

## Files
- `AppMain.cpp`: Qt application entry point.
- `MainWindow.h/.cpp`: UI scaffold and signals/slots.
- `CMakeLists.txt`: Standalone build with Qt6/Qt5 detection.

## Build (MSVC — recommended)
```powershell
$root = "C:\\Users\\HiH8e\\OneDrive\\Desktop\\Powershield\\RawrXD-ModelLoader"
$src = Join-Path $root "src\\qtapp"
$build = Join-Path $root "build-msvc-qtapp"
if (-not (Test-Path $build)) { New-Item -ItemType Directory -Path $build | Out-Null }
Push-Location $build

# If Qt isn’t found, add -DQt6_DIR or -DQt5_DIR to cmake below
cmake -G "Visual Studio 17 2022" -A x64 $src
cmake --build . --config Release

& (Join-Path $build "bin-msvc\\QuantumIDEApp.exe")
Pop-Location
```

## Build (MinGW — static to avoid DLL issues)
```powershell
$root = "C:\\Users\\HiH8e\\OneDrive\\Desktop\\Powershield\\RawrXD-ModelLoader"
$src = Join-Path $root "src\\qtapp"
$build = Join-Path $root "build-mingw-qtapp"
if (-not (Test-Path $build)) { New-Item -ItemType Directory -Path $build | Out-Null }
Push-Location $build

cmake -G "MinGW Makefiles" $src
cmake --build . --config Release

& (Join-Path $build "bin-mingw\\QuantumIDEApp.exe")
Pop-Location
```

## Qt Path Hints
If CMake cannot locate Qt automatically, set one of:
- `-DQt6_DIR="C:\\Qt\\6.x\\msvc2022_64\\lib\\cmake\\Qt6"`
- `-DQt5_DIR="C:\\Qt\\5.x\\msvc2019_64\\lib\\cmake\\Qt5"`
- For MinGW: use your `mingw_64` Qt install dir.

## What You’ll See
- Top Goal Bar (Start Goal button) — emits `onGoalSubmitted(QString)`.
- Left Agent Control Panel — progress + agent statuses.
- Center tab `FILLAME.JS` — sample lines + AI suggestion overlay.
- Right Proposal Review — mock changes with action buttons.
- Bottom QShell — echoes `Invoke-QAgent`, `Get-QContext`, `Set-QConfig`.

## Notes
- Pure Qt Widgets: no web engine or external UI libraries.
- Output directories separated by compiler to avoid DLL collisions.
- MinGW links libstdc++/libgcc statically to reduce runtime issues.
