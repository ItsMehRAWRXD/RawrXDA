# Building with Visual Studio 2022 on C: (Ninja)

## Why Ninja?

The project’s **CMakeLists.txt** can use either:

- **Visual Studio 17 2022** generator – requires a Visual Studio **instance registered with the VS Installer**. If your VS is at `C:\VS2022Enterprise` (or only Build Tools in Program Files), the generator may report: *"could not find any instance of Visual Studio"* or *"instance is not known to the Visual Studio Installer"*.
- **Ninja** generator – uses the **same MSVC and SDK paths** that CMakeLists already detects (e.g. `C:\VS2022Enterprise\VC\Tools\MSVC\...` or Build Tools). No Installer check, so it works with your C: drive install.

## One-time setup (already done if build succeeded)

1. **Ninja** – Ensure `ninja` is on `PATH` (e.g. `C:\Users\...\WinGet\Links\ninja.exe` or Strawberry).
2. **Configure with Ninja** (from repo root):
   ```powershell
   cd D:\rawrxd
   # Optional: clear old VS generator cache so compiler is re-detected
   Remove-Item build\CMakeCache.txt -ErrorAction SilentlyContinue
   Remove-Item build\CMakeFiles -Recurse -ErrorAction SilentlyContinue
   cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -B build -S .
   ```
3. **Build**:
   ```powershell
   cmake --build build --target RawrXD-Win32IDE.exe
   # or build everything:
   cmake --build build
   ```

## Where CMake finds VS 2022

Detection order in **CMakeLists.txt** (before `project()`):

1. `D:/VS2022Enterprise/VC/Tools/MSVC`
2. `C:/VS2022Enterprise/VC/Tools/MSVC`
3. `C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC`

The first path that exists is used for `cl.exe`, `link.exe`, and SDK (Windows Kits 10 on C: or D:). No generator change is required for that; only use **Ninja** so CMake doesn’t require a registered VS instance.

## If you prefer the VS generator later

To use the Visual Studio generator again (e.g. to open the solution in IDE), the installation must be **registered with the Visual Studio Installer**. Run the Installer and ensure the instance you use (e.g. Enterprise at `C:\VS2022Enterprise` or Build Tools) is listed and repaired if needed.
