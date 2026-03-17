# ============================================================================
# RawrXD Sovereign Host - PowerShell Build Script (CLI + GUI)
# Complete autonomous agentic system with amphibious support
# ============================================================================

Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   RawrXD SOVEREIGN HOST - AMPHIBIOUS BUILD SYSTEM             ║" -ForegroundColor Cyan
Write-Host "║   Building both CLI and GUI subsystem versions                ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# Function to find ml64.exe
function Find-ML64 {
    $paths = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe",
        "C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe",
        "C:\masm64\bin\ml64.exe"
    )
    
    foreach ($pattern in $paths) {
        $found = Get-Item $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) {
            return $found.FullName
        }
    }
    return $null
}

# Function to find link.exe  
function Find-Link {
    $paths = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe",
        "C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe"
    )
    
    foreach ($pattern in $paths) {
        $found = Get-Item $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) {
            return $found.FullName
        }
    }
    return $null
}

# Find tools
Write-Host "[INIT] Locating build tools..." -ForegroundColor Yellow
$ml64 = Find-ML64
$link = Find-Link

if (-not $ml64) {
    Write-Host "[ERROR] ml64.exe not found!" -ForegroundColor Red
    Write-Host "`nPlease install:" -ForegroundColor Yellow
    Write-Host "  - Visual Studio 2019/2022 with C++ Desktop Development" -ForegroundColor White
    Write-Host "  - OR MASM64 standalone package`n" -ForegroundColor White
    exit 1
}

if (-not $link) {
    Write-Host "[ERROR] link.exe not found!" -ForegroundColor Red
    exit 1
}

Write-Host "  [OK] Found ml64: $ml64" -ForegroundColor Green
Write-Host "  [OK] Found link: $link" -ForegroundColor Green

# Clean previous builds
Write-Host "`n[1/6] Cleaning previous builds..." -ForegroundColor Yellow
Remove-Item -Path "RawrXD_CLI.exe", "RawrXD_GUI.exe", "*.obj", "*.ilk", "*.pdb" -ErrorAction SilentlyContinue

# Build CLI version
Write-Host "[2/6] Assembling CLI version (Console Subsystem)..." -ForegroundColor Yellow
& $ml64 /c /Zi /Fo:RawrXD_Sovereign_CLI.obj RawrXD_Sovereign_CLI.asm 2>&1 | Out-String | Write-Host
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] CLI assembly failed" -ForegroundColor Red
    exit 1
}

Write-Host "[3/6] Linking CLI executable..." -ForegroundColor Yellow
& $link /subsystem:console /entry:main /out:RawrXD_CLI.exe RawrXD_Sovereign_CLI.obj kernel32.lib user32.lib 2>&1 | Out-String | Write-Host
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] CLI linking failed" -ForegroundColor Red
    exit 1
}

# Build GUI version
Write-Host "[4/6] Assembling GUI version (Windows Subsystem)..." -ForegroundColor Yellow
& $ml64 /c /Zi /Fo:RawrXD_Sovereign_GUI.obj RawrXD_Sovereign_GUI.asm 2>&1 | Out-String | Write-Host
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] GUI assembly failed" -ForegroundColor Red
    exit 1
}

Write-Host "[5/6] Linking GUI executable..." -ForegroundColor Yellow
& $link /subsystem:windows /entry:WinMain /out:RawrXD_GUI.exe RawrXD_Sovereign_GUI.obj kernel32.lib user32.lib gdi32.lib 2>&1 | Out-String | Write-Host
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] GUI linking failed" -ForegroundColor Red
    exit 1
}

# Verify builds
Write-Host "[6/6] Verifying builds..." -ForegroundColor Yellow
if (Test-Path "RawrXD_CLI.exe") {
    $size = (Get-Item "RawrXD_CLI.exe").Length
    Write-Host "  [OK] RawrXD_CLI.exe created ($size bytes)" -ForegroundColor Green
} else {
    Write-Host "  [FAIL] CLI executable missing" -ForegroundColor Red
}

if (Test-Path "RawrXD_GUI.exe") {
    $size = (Get-Item "RawrXD_GUI.exe").Length
    Write-Host "  [OK] RawrXD_GUI.exe created ($size bytes)" -ForegroundColor Green
} else {
    Write-Host "  [FAIL] GUI executable missing" -ForegroundColor Red
}

Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║   BUILD COMPLETE - AMPHIBIOUS DEPLOYMENT READY                ║" -ForegroundColor Green
Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Green

Write-Host "Run:" -ForegroundColor Cyan
Write-Host "  .\RawrXD_CLI.exe    - Console mode (CLI)" -ForegroundColor White
Write-Host "  .\RawrXD_GUI.exe    - Windowed mode (GUI)" -ForegroundColor White
Write-Host "`nBoth executables feature:" -ForegroundColor Cyan
Write-Host "  • Autonomous Agentic Loops" -ForegroundColor White
Write-Host "  • Multi-Agent Coordination (32 agents)" -ForegroundColor White
Write-Host "  • Self-Healing Infrastructure" -ForegroundColor White
Write-Host "  • Auto-Fix Compilation Cycle" -ForegroundColor White
Write-Host "  • Full Pipeline: Chat → Prompt → LLM → Token → Renderer`n" -ForegroundColor White
