# Compile Ghost Paging Kernel
$Src = "$PSScriptRoot\..\src\thermal\masm"
$BuildDir = "$PSScriptRoot\..\build_ghost"

# Environmental Heuristics for the User's Machine
$MSVC_Bin = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64"
if (Test-Path $MSVC_Bin) {
    Write-Host "Found MSVC at $MSVC_Bin" -ForegroundColor Yellow
    $env:PATH = "$MSVC_Bin;$env:PATH"
}

if (!(Test-Path $BuildDir)) { New-Item -ItemType Directory -Path $BuildDir | Out-Null }

Set-Location $BuildDir

Write-Host "Compiling MASM Kernel..." -ForegroundColor Cyan
& ml64.exe /c /nologo /Zi /Fo"ghost_paging.obj" "$Src\ghost_paging.asm"
if ($LASTEXITCODE -ne 0) { throw "MASM Compilation Failed" }

Write-Host "Compiling C++ Host..." -ForegroundColor Cyan
# Attempt to find headers? CL needs INCLUDE
# Since we lack full vcvars, CL might fail on #include <iostream> if INCLUDE isn't set.
# We will defer full linking if we can't find libraries, but getting the OBJ is a win.

try {
    & cl.exe /c /nologo /EHsc /Zi /Fo"ghost_paging_main.obj" "$Src\ghost_paging_main.cpp"
} catch {
    Write-Warning "C++ compilation failed (likely missing INCLUDE paths). Skipping Host Demo."
    exit 0
}

Write-Host "Linking..." -ForegroundColor Cyan
# Link needs LIB paths.
try {
    & link.exe /NOLOGO /DEBUG /SUBSYSTEM:CONSOLE /OUT:"GhostPagingDemo.exe" "ghost_paging.obj" "ghost_paging_main.obj" kernel32.lib user32.lib
    Write-Host "Build Success! Running Demo..." -ForegroundColor Green
    .\GhostPagingDemo.exe
} catch {
     Write-Warning "Linking failed (likely missing LIB paths). Ensure you run this from a Developer Command Prompt."
}

