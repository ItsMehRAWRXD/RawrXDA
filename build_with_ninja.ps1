# Build via Ninja
$pathsToPrepend = @(
    "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.22621.0\\x64",
    "C:\\VS2022Enterprise\\VC\\Tools\\MSVC\\14.50.35717\\bin\\Hostx64\\x64"
)
$env:PATH = ($pathsToPrepend + ($env:PATH -split ';' | Where-Object { $_ })) -join ';'
& cmd.exe /c "C:\VS2022Enterprise\Common7\Tools\VsDevCmd.bat -arch=x64 && cd D:\rawrxd && rmdir /s /q build_ninja & mkdir build_ninja & cd build_ninja && cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=\""/D_DEBUG /D_ITERATOR_DEBUG_LEVEL=2 /RTC1 /GS /guard:cf\"" .. && ninja"

