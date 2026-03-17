# Direct compilation with clang-cl (no CMake)
# This bypasses all the VS SDK and build system complexity

$clang_cl = "C:\Program Files\LLVM\bin\clang-cl.exe"
$output_dir = "build\bin"
$exe_name = "$output_dir\RawrXD-ModelLoader.exe"

# Create output directory
New-Item -ItemType Directory -Path $output_dir -Force | Out-Null

# Source files
$sources = @(
    "src/main.cpp",
    "src/gguf_loader.cpp",
    "src/vulkan_compute.cpp",
    "src/hf_downloader.cpp",
    "src/gui.cpp",
    "src/api_server.cpp"
)

# Libraries
$libs = @(
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
    "gdi32.lib"
)

# Compilation command
$compile_flags = @(
    "/O2",                       # Optimization level 2
    "/EHsc",                     # Exception handling
    "/std:c++20",                # C++20 standard
    "/W4",                        # Warning level 4
    "/D_CRT_SECURE_NO_WARNINGS", # Disable unsafe warnings
    "/D_WINDOWS",                # Windows define
    "/Iinclude"                  # Include directory
)

Write-Host "RawrXD Model Loader - Direct Clang Compilation" -ForegroundColor Cyan
Write-Host "Compiler: $clang_cl" -ForegroundColor Green
Write-Host "Output: $exe_name" -ForegroundColor Green
Write-Host "`nCompiling sources..." -ForegroundColor Yellow

# Build command
$cmd = @(
    $clang_cl,
    $compile_flags,
    $sources,
    "/link",
    $libs,
    "/OUT:$exe_name"
)

Write-Host "Command: $cmd`n" -ForegroundColor DarkGray

&  $clang_cl $compile_flags $sources /link $libs "/OUT:$exe_name"

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n✓ Compilation successful!" -ForegroundColor Green
    Write-Host "Executable: $(Resolve-Path $exe_name)" -ForegroundColor Green
} else {
    Write-Host "`n✗ Compilation failed with exit code: $LASTEXITCODE" -ForegroundColor Red
    exit 1
}
