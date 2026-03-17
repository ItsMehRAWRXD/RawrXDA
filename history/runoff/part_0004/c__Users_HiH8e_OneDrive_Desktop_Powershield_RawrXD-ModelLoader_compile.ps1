# Compile with full environment setup
$ErrorActionPreference = "Stop"

Write-Host "RawrXD Model Loader - Compilation with VS Environment" -ForegroundColor Cyan

# Setup VS environment using PowerShell directly
$vsdevcmd = "C:\VS2022Enterprise\Common7\Tools\VsDevCmd.bat"
$vcvars = "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat"

# Alternative approach: Set environment variables manually based on VS installation
$vs_path = "C:\VS2022Enterprise"
$vc_path = "$vs_path\VC\Tools\MSVC"

# Find the latest MSVC version
$msvc_versions = Get-ChildItem $vc_path -Directory | Sort-Object Name -Descending
if ($msvc_versions.Count -eq 0) {
    Write-Host "✗ No MSVC version found" -ForegroundColor Red
    exit 1
}
$msvc_latest = $msvc_versions[0].FullName

Write-Host "Found MSVC: $msvc_latest" -ForegroundColor Green

# Set environment variables for VS
$env:PATH += ";$msvc_latest\bin\Hostx64\x64"
$env:PATH += ";C:\Program Files\LLVM\bin"
$env:PATH += ";$vs_path\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"

# Windows Kit paths
$winsdk = "C:\Program Files (x86)\Windows Kits\10"
$winsdk_version = "10.0.26100.0"

if (Test-Path "$winsdk\Include\$winsdk_version") {
    $crt_include = "$msvc_latest\include"
    $ucrt_include = "$winsdk\Include\$winsdk_version\ucrt"
    $um_include = "$winsdk\Include\$winsdk_version\um"
    $shared_include = "$winsdk\Include\$winsdk_version\shared"
    
    $env:INCLUDE = "$crt_include;$ucrt_include;$um_include;$shared_include"
    $env:LIB = "$msvc_latest\lib\x64;$winsdk\Lib\$winsdk_version\ucrt\x64;$winsdk\Lib\$winsdk_version\um\x64"
}

# Verify clang-cl is now available
$clang_test = clang-cl --version 2>&1 | Select-Object -First 1
Write-Host "Clang-cl: $clang_test" -ForegroundColor Green

# Source files
$sources = @(
    "src/main.cpp",
    "src/gguf_loader.cpp",
    "src/vulkan_compute.cpp",
    "src/hf_downloader.cpp",
    "src/gui.cpp",
    "src/api_server.cpp"
)

# Prepare build directory
New-Item -ItemType Directory -Path "build\bin" -Force | Out-Null

# Compile
Write-Host "`nCompiling with Clang-CL..." -ForegroundColor Yellow

$compile_cmd = @(
    "clang-cl",
    "/O2",
    "/EHsc",
    "/std:c++20",
    "/W4",
    "/D_CRT_SECURE_NO_WARNINGS",
    "/D_WINDOWS",
    "/Iinclude",
    ($sources -join " "),
    "/link",
    "vulkan-1.lib",
    "ws2_32.lib",
    "winmm.lib",
    "imm32.lib",
    "ole32.lib",
    "oleaut32.lib",
    "winspool.lib",
    "advapi32.lib",
    "shell32.lib",
    "user32.lib",
    "gdi32.lib",
    "/OUT:build\bin\RawrXD-ModelLoader.exe"
)

Write-Host ($compile_cmd -join " `n  ") -ForegroundColor DarkGray

clang-cl /O2 /EHsc /std:c++20 /W4 /D_CRT_SECURE_NO_WARNINGS /D_WINDOWS /Iinclude $sources /link vulkan-1.lib ws2_32.lib winmm.lib imm32.lib ole32.lib oleaut32.lib winspool.lib advapi32.lib shell32.lib user32.lib gdi32.lib /OUT:build\bin\RawrXD-ModelLoader.exe

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n✓ Compilation SUCCESSFUL!" -ForegroundColor Green
    $exe = Resolve-Path "build\bin\RawrXD-ModelLoader.exe"
    Write-Host "Executable: $exe" -ForegroundColor Green
    Write-Host "Size: $((Get-Item $exe).Length / 1MB | Round -Precision 2) MB" -ForegroundColor Green
} else {
    Write-Host "`n✗ Compilation FAILED" -ForegroundColor Red
    Write-Host "Exit code: $LASTEXITCODE" -ForegroundColor Red
    exit 1
}
