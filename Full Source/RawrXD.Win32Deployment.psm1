# RawrXD.Win32Deployment.psm1
# PowerShell scaffolding for Win32 IDE build/deployment

$script:Win32IDEDigest = @'
Win32IDE.cpp Digest (RawrXD Win32 IDE)
--------------------------------------
Source: d:\lazy init ide\src\win32app\Win32IDE.cpp
Lines: ~6127
Reviewed: 2026-01-24

Purpose
- Implements the primary Win32 IDE window, UI layout, and event loop.
- Hosts editor, terminals, output tabs, file explorers, PowerShell panel, AI chat, and agentic autonomy.
- Integrates GGUF model loading (streaming), Ollama HTTP inference, and digestion engine hooks.

Primary Entry Points
- Win32IDE::Win32IDE(): Initializes logger, renderer, PowerShell state, theme, snippets, settings, GGUF loader.
- Win32IDE::createWindow(): Registers class, creates main HWND, centers window.
- Win32IDE::onCreate(): Builds UI (menu, toolbar, sidebar, editor, terminals, output tabs, minimap, status bar, PowerShell panel, debugger UI, chat panel) then initializes renderer, agentic bridge, autonomy manager.
- Win32IDE::handleMessage(): Central WndProc for keyboard/menu/WM_* flows.

Digestion Engine (Agentic Source Digestion)
- WM_RUN_DIGESTION / WM_DIGESTION_PROGRESS / WM_DIGESTION_COMPLETE.
- DIGESTION_CTX struct (packed) with source/output paths, chunk size, threads, flags, callbacks.
- queueDigestionJob() allocates DIGESTION_CTX and SendMessage(WM_RUN_DIGESTION).
- DigestionThreadProc() calls RawrXD_DigestionEngine_Avx512 and posts completion message.
- Hotkey: Ctrl+Shift+D (or Tools > Run Digestion) uses current file -> .digest output.

Rendering + Editor Surface
- RichEdit editor; subclassed to suppress paint and hide caret.
- TransparentRenderer (DirectX) with optional Vulkan renderer (ENABLE_VULKAN).
- syncEditorToGpuSurface() extracts text/caret state and passes to renderer.

UI Layout + Panels
- Activity bar + primary sidebar; secondary sidebar for AI chat.
- Minimap rendered in owner-draw static control.
- Output tabs with severity filter and timestamping for Errors/Debug.
- Splitter between terminal and output panel with drag-resize handling.
- Status bar and command input at bottom.

Terminal + PowerShell
- Multi-pane terminal manager with split horizontal/vertical.
- Command input supports chat mode (/chat, /exit-chat) when model loaded.
- Dedicated PowerShell panel (dockable/visible toggle via Ctrl+`).

AI Chat / Inference
- GGUF streaming loader (StreamingGGUFLoader or GGUFLoader) with zone loading.
- Ollama HTTP integration (WinHTTP) for /api/generate.
- AI chat panel with model selector + max tokens slider.
- generateResponseAsync() streams responses back to UI callbacks.

Agentic/Autonomy
- Initializes Win32IDE_AgenticBridge and AutonomyManager.
- Menu actions: start/stop/toggle auto loop, set goal, status, memory snapshot.

Git Integration
- Basic status/commit/push/pull and a Git panel.
- executeGitCommand() uses CreateProcess + temp output file.

Search / Replace
- Custom find/replace dialogs with case, whole word, regex toggles (regex not implemented).

Persistence
- ide_settings.ini stores UI sizes, output visibility, severity filter, GGUF loader choice, Vulkan renderer, Ollama base URL, model tag.
- Themes saved under themes\*.theme; snippets saved under snippets\snippets.txt.

Notable Messages / IDs
- Custom WM_USER offsets for digestion + Copilot streaming.
- Menu IDs for autonomy/agent actions, terminal splits, model load, output panel toggles.

Known Structural Notes
- Some function blocks appear duplicated in-file (likely merge artifacts). Validate if multiple definitions exist in compilation unit.
- Extensive debug diagnostics write to C:\Users\HiH8e\Desktop\*.txt for constructor phases.
'@

function Get-Win32IDEDigest {
    [CmdletBinding()]
    param(
        [switch]$AsObject
    )

    if ($AsObject) {
        return [pscustomobject]@{
            SourceFile = 'd:\lazy init ide\src\win32app\Win32IDE.cpp'
            Lines = 6127
            Reviewed = '2026-01-24'
            Subsystems = @(
                'Win32 window lifecycle + message pump',
                'Digestion engine (WM_RUN_DIGESTION)',
                'Renderer (DirectX/Vulkan) + editor surface sync',
                'Terminal manager + command input',
                'PowerShell panel + dedicated terminal',
                'GGUF streaming loader + model info',
                'Ollama HTTP inference + AI chat panel',
                'Sidebar + file explorers + minimap',
                'Output tabs + severity filter',
                'Git panel + status/commit/push/pull',
                'Autonomy manager + agentic bridge'
            )
            Summary = $script:Win32IDEDigest
        }
    }

    return $script:Win32IDEDigest
}

function Test-RawrXDWin32Prereqs {
    $checks = @(
        @{ Name = 'CMake'; Command = 'cmake' },
        @{ Name = 'MinGW Make'; Command = 'mingw32-make' }
    )
    $results = @()
    foreach ($check in $checks) {
        $exists = Get-Command $check.Command -ErrorAction SilentlyContinue
        $results += [ordered]@{ Component = $check.Name; Available = [bool]$exists; Command = $check.Command }
    }
    return $results
}

function Invoke-RawrXDWin32Build {
    param(
        [string]$SourcePath = (Get-RawrXDRootPath),
        [string]$BuildPath = (Join-Path (Get-RawrXDRootPath) 'build-win32'),
        [string]$BuildType = 'Release',
        [string]$Generator = 'MinGW Makefiles',
        [switch]$MASM,
        [switch]$NASM,
        [switch]$DryRun
    )

    if ($DryRun) {
        $steps = @(
            "cmake -S `"$SourcePath`" -B `"$BuildPath`" -G `"$Generator`" -DBUILD_TESTS=ON",
            "cmake --build `"$BuildPath`" --config $BuildType"
        )
        return @{ Success = $true; Steps = $steps }
    }

    # Actual build execution
    try {
        if (-not (Test-Path $BuildPath)) {
            New-Item -ItemType Directory -Path $BuildPath -Force | Out-Null
        }

        $cmakeFile = Join-Path $SourcePath 'CMakeLists.txt'
        if (-not (Test-Path $cmakeFile)) {
            return @{ Success = $false; Error = 'CMakeLists.txt not found' }
        }

        Push-Location $BuildPath
        try {
            Write-Host "[Win32Build] Configuring..." -ForegroundColor Cyan
            $configArgs = @('-S', $SourcePath, '-B', '.', '-G', $Generator, "-DCMAKE_BUILD_TYPE=$BuildType")
            
            if ($MASM) {
                $masmPath = 'C:\masm32\bin\ml.exe'
                if (Test-Path $masmPath) {
                    $configArgs += '-DENABLE_MASM=ON', "-DCMAKE_ASM_MASM_COMPILER=$masmPath"
                }
            }
            
            if ($NASM) {
                $nasmPath = 'C:\nasm\nasm.exe'
                if (Test-Path $nasmPath) {
                    $configArgs += '-DENABLE_NASM=ON', "-DCMAKE_ASM_NASM_COMPILER=$nasmPath"
                }
            }
            
            & cmake $configArgs
            if ($LASTEXITCODE -ne 0) {
                return @{ Success = $false; Error = "CMake config failed: $LASTEXITCODE" }
            }

            Write-Host "[Win32Build] Building..." -ForegroundColor Cyan
            & cmake --build . --config $BuildType --parallel
            if ($LASTEXITCODE -ne 0) {
                return @{ Success = $false; Error = "Build failed: $LASTEXITCODE" }
            }

            Write-Host "[Win32Build] Completed!" -ForegroundColor Green
            return @{ Success = $true; BuildPath = $BuildPath; BuildType = $BuildType }
        } finally {
            Pop-Location
        }
    } catch {
        return @{ Success = $false; Error = $_.Exception.Message }
    }
}

function Initialize-RawrXDPackage {
    param(
        [string]$PackageName = "RawrXD-Production",
        [string]$Version = "1.0.0"
    )
    
    $outDir = Join-Path (Get-RawrXDRootPath) "dist\$PackageName-$Version"
    if (Test-Path $outDir) { Remove-Item $outDir -Recurse -Force }
    New-Item -Path $outDir -ItemType Directory -Force | Out-Null
    
    # Copy core modules
    $modulesDir = New-Item -Path (Join-Path $outDir "modules") -ItemType Directory -Force
    Get-ChildItem (Get-RawrXDRootPath) -Filter "RawrXD.*.psm1" | Copy-Item -Destination $modulesDir
    
    # Copy binaries
    $binDir = New-Item -Path (Join-Path $outDir "bin") -ItemType Directory -Force
    if (Test-Path (Join-Path (Get-RawrXDRootPath) "RawrXD-ModelLoader.exe")) {
        Copy-Item (Join-Path (Get-RawrXDRootPath) "RawrXD-ModelLoader.exe") -Destination $binDir
    }
    
    # Create launcher
    $launcherContent = @"
@echo off
powershell.exe -ExecutionPolicy Bypass -Command "Import-Module './modules/RawrXD.Core.psm1'; Start-RawrXDAutonomousLoop -Goal '%*'"
"@
    $launcherContent | Set-Content (Join-Path $outDir "run.bat")
    
    Write-Host "Package created at: $outDir" -ForegroundColor Green
    return $outDir
}

Export-ModuleMember -Function Test-RawrXDWin32Prereqs, Invoke-RawrXDWin32Build, Initialize-RawrXDPackage, Get-Win32IDEDigest
