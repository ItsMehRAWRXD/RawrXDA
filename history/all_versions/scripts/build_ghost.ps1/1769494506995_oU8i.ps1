# Compile Ghost Paging Kernel
$Src = "$PSScriptRoot\..\src\thermal\masm"
$BuildDir = "$PSScriptRoot\..\build_ghost"

if (!(Test-Path $BuildDir)) { New-Item -ItemType Directory -Path $BuildDir | Out-Null }

Set-Location $BuildDir

Write-Host "Compiling MASM Kernel..." -ForegroundColor Cyan
& ml64.exe /c /nologo /Zi /Fo"ghost_paging.obj" "$Src\ghost_paging.asm"
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "Compiling C++ Host..." -ForegroundColor Cyan
& cl.exe /c /nologo /EHsc /Zi /Fo"ghost_paging_main.obj" "$Src\ghost_paging_main.cpp"

Write-Host "Linking..." -ForegroundColor Cyan
& link.exe /NOLOGO /DEBUG /SUBSYSTEM:CONSOLE /OUT:"GhostPagingDemo.exe" "ghost_paging.obj" "ghost_paging_main.obj" kernel32.lib user32.lib

Write-Host "Build Success! Running Demo..." -ForegroundColor Green
.\GhostPagingDemo.exe
