<#
.SYNOPSIS
    Wire-Patcher.ps1 — Surgical Qt-ectomy & ASM Linkage for RawrXD Sidebar
.DESCRIPTION
    1. Assembles RawrXD_Sidebar_x64.asm with ml64
    2. Performs aggressive Qt logging removal from Win32IDE_Sidebar.cpp
    3. Injects extern "C" declarations for MASM64 functions
    4. Injects initialization calls into Sidebar constructor
    5. Wires the .obj into CMakeLists.txt ASM_KERNEL_SOURCES
.PARAMETER Sidebar
    Path to the sidebar C++ source file (default: src/win32app/Win32IDE_Sidebar.cpp)
.PARAMETER BuildDir
    Build output directory (default: current directory)
.PARAMETER AsmDir
    Directory containing the .asm source (default: src/asm)
.PARAMETER DryRun
    If set, shows what would change without modifying files
#>
param(
    [string]$Sidebar = "src\win32app\Win32IDE_Sidebar.cpp",
    [string]$BuildDir = ".",
    [string]$AsmDir = "src\asm",
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$asmName = "RawrXD_Sidebar_x64"
$asmPath = Join-Path $AsmDir "$asmName.asm"
$objPath = Join-Path $BuildDir "$asmName.obj"

Write-Host ""
Write-Host "================================================================" -ForegroundColor DarkRed
Write-Host "  [RAWRXD] Qt-ectomy + MASM64 Sidebar Linkage" -ForegroundColor DarkRed
Write-Host "================================================================" -ForegroundColor DarkRed
Write-Host ""

# ─────────────────────────────────────────────────────────
# 1. Verify ASM source exists
# ─────────────────────────────────────────────────────────
if (-not (Test-Path $asmPath)) {
    Write-Error "[-] ASM source not found: $asmPath"
    exit 1
}
Write-Host "[1/6] ASM source verified: $asmPath" -ForegroundColor Green

# ─────────────────────────────────────────────────────────
# 2. Assemble with ml64
# ─────────────────────────────────────────────────────────
Write-Host "[2/6] Assembling with ml64..." -ForegroundColor Cyan

if (-not $DryRun) {
    # Find ml64 via vswhere or PATH
    $ml64 = Get-Command ml64.exe -ErrorAction SilentlyContinue
    if (-not $ml64) {
        # Try VS Developer Command Prompt path
        $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path $vsWhere) {
            $vsPath = & $vsWhere -latest -property installationPath
            $ml64Path = Join-Path $vsPath "VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe"
            $ml64Resolved = Get-ChildItem $ml64Path -ErrorAction SilentlyContinue | Select-Object -Last 1
            if ($ml64Resolved) {
                $ml64 = $ml64Resolved.FullName
            }
        }
    } else {
        $ml64 = $ml64.Source
    }

    if (-not $ml64) {
        Write-Warning "[-] ml64.exe not found in PATH. Run from VS Developer Command Prompt."
        Write-Warning "    Or: cmake --build build --config Release (CMake handles MASM)"
    } else {
        & $ml64 /c /Zi /Zd /Fo"$objPath" "$asmPath"
        if ($LASTEXITCODE -ne 0) {
            Write-Error "[-] Assembly failed with exit code $LASTEXITCODE"
            exit 1
        }
        Write-Host "    -> $objPath" -ForegroundColor Green
    }
} else {
    Write-Host "    [DRY RUN] Would run: ml64 /c /Zi /Zd /Fo`"$objPath`" `"$asmPath`"" -ForegroundColor Yellow
}

# ─────────────────────────────────────────────────────────
# 3. Qt-ectomy on Sidebar.cpp
# ─────────────────────────────────────────────────────────
Write-Host "[3/6] Qt-ectomy on $Sidebar..." -ForegroundColor Cyan

if (Test-Path $Sidebar) {
    $cpp = Get-Content $Sidebar -Raw
    $originalLen = $cpp.Length
    $changes = 0

    # Qt include removal
    $qtPatterns = @(
        @('#include\s*<QDebug>',             '// [RAWRXD] Qt removed — using MASM logger'),
        @('#include\s*<QtDebug>',            '// [RAWRXD] Qt removed — using MASM logger'),
        @('#include\s*<QTreeView>',          '// [RAWRXD] Qt removed — using MASM tree'),
        @('#include\s*<QTreeWidget>',        '// [RAWRXD] Qt removed — using MASM tree'),
        @('#include\s*<QFileSystemModel>',   '// [RAWRXD] Qt removed — using Win32 shell'),
        @('#include\s*<QApplication>',       '// [RAWRXD] Qt removed')
    )

    # Qt logging replacement → MASM logger
    $logPatterns = @(
        @('qDebug\s*\(\s*\)\s*<<\s*([^;]+);',
          'RawrXD_Logger_Write("DEBUG", __FILE__, __LINE__, "$1");'),
        @('qInfo\s*\(\s*\)\s*<<\s*([^;]+);',
          'RawrXD_Logger_Write("INFO", __FILE__, __LINE__, "$1");'),
        @('qWarning\s*\(\s*\)\s*<<\s*([^;]+);',
          'RawrXD_Logger_Write("WARN", __FILE__, __LINE__, "$1");'),
        @('qCritical\s*\(\s*\)\s*<<\s*([^;]+);',
          'RawrXD_Logger_Write("CRIT", __FILE__, __LINE__, "$1");')
    )

    foreach ($p in $qtPatterns) {
        $before = $cpp
        $cpp = $cpp -replace $p[0], $p[1]
        if ($cpp -ne $before) { $changes++ }
    }

    foreach ($p in $logPatterns) {
        $before = $cpp
        $cpp = $cpp -replace $p[0], $p[1]
        if ($cpp -ne $before) { $changes++ }
    }

    if ($changes -gt 0) {
        if (-not $DryRun) {
            # Backup original
            $backupPath = "$Sidebar.pre_qtectomy.bak"
            if (-not (Test-Path $backupPath)) {
                Copy-Item $Sidebar $backupPath
                Write-Host "    Backup: $backupPath" -ForegroundColor Gray
            }
            $cpp | Set-Content $Sidebar -NoNewline
        }
        Write-Host "    $changes Qt pattern(s) replaced" -ForegroundColor Green
    } else {
        Write-Host "    No Qt patterns found (already clean)" -ForegroundColor Gray
    }
} else {
    Write-Host "    Sidebar file not found: $Sidebar (skipping Qt-ectomy)" -ForegroundColor Yellow
}

# ─────────────────────────────────────────────────────────
# 4. Inject extern "C" declarations
# ─────────────────────────────────────────────────────────
Write-Host "[4/6] Injecting MASM64 extern linkage..." -ForegroundColor Cyan

$externsBlock = @"

// ============================================================
// RAWRXD MASM64 SIDEBAR LINKAGE — RawrXD_Sidebar_x64.asm
// Zero Qt, Zero CRT, Pure Win64 ABI
// ============================================================
extern "C" {
    void RawrXD_Logger_Init(const char* filename);
    void RawrXD_Logger_Write(const char* level, const char* file, unsigned int line, const char* msg);
    BOOL RawrXD_Debug_Attach(DWORD dwProcessId);
    BOOL RawrXD_Debug_Wait(void* lpDebugEvent, DWORD dwMilliseconds);
    void RawrXD_Debug_Step(HANDLE hThread, void* pContext);
    void RawrXD_Tree_LazyLoad(HWND hWndTree);
    void RawrXD_DarkMode_Force(HWND hWnd);
}

"@

if ((Test-Path $Sidebar) -and $cpp) {
    if ($cpp -notmatch 'RawrXD_Logger_Init') {
        # Find last #include and insert after it
        $includeMatches = [regex]::Matches($cpp, '#include\s+["<][^">]+[">]\s*\n')
        if ($includeMatches.Count -gt 0) {
            $lastMatch = $includeMatches[$includeMatches.Count - 1]
            $insertPos = $lastMatch.Index + $lastMatch.Length
            $cpp = $cpp.Insert($insertPos, $externsBlock)

            if (-not $DryRun) {
                $cpp | Set-Content $Sidebar -NoNewline
            }
            Write-Host "    Extern declarations injected after includes" -ForegroundColor Green
        } else {
            Write-Host "    No #include found — prepending externs" -ForegroundColor Yellow
            $cpp = $externsBlock + $cpp
            if (-not $DryRun) {
                $cpp | Set-Content $Sidebar -NoNewline
            }
        }
    } else {
        Write-Host "    Externs already present (idempotent)" -ForegroundColor Gray
    }
}

# ─────────────────────────────────────────────────────────
# 5. Inject init calls into constructor/onCreate
# ─────────────────────────────────────────────────────────
Write-Host "[5/6] Injecting MASM init calls..." -ForegroundColor Cyan

if ((Test-Path $Sidebar) -and $cpp) {
    if ($cpp -notmatch 'RawrXD_Logger_Init\s*\(') {
        # Try to find Sidebar constructor or initSidebar function
        $ctorPatterns = @(
            '(initSidebar\s*\([^)]*\)\s*\{)',
            '(Sidebar\s*\([^)]*\)\s*\{)',
            '(createSidebar\s*\([^)]*\)\s*\{)',
            '(Win32IDE::initSidebar\s*\([^)]*\)\s*\{)',
            '(initializeExplorerView\s*\([^)]*\)\s*\{)'
        )

        $injected = $false
        foreach ($pattern in $ctorPatterns) {
            if ($cpp -match $pattern) {
                $initBlock = @"
`$1
    // RAWRXD MASM64 Sidebar Init
    RawrXD_Logger_Init(nullptr);

"@
                $cpp = $cpp -replace $pattern, $initBlock
                if (-not $DryRun) {
                    $cpp | Set-Content $Sidebar -NoNewline
                }
                Write-Host "    Init injected into matched function" -ForegroundColor Green
                $injected = $true
                break
            }
        }

        if (-not $injected) {
            Write-Host "    No constructor pattern matched — add manually:" -ForegroundColor Yellow
            Write-Host '      RawrXD_Logger_Init(nullptr);' -ForegroundColor White
            Write-Host '      RawrXD_DarkMode_Force(hWnd);' -ForegroundColor White
        }
    } else {
        Write-Host "    Init calls already present (idempotent)" -ForegroundColor Gray
    }
}

# ─────────────────────────────────────────────────────────
# 6. Wire into CMakeLists.txt
# ─────────────────────────────────────────────────────────
Write-Host "[6/6] Wiring into CMakeLists.txt..." -ForegroundColor Cyan

$cmakePath = "CMakeLists.txt"
if (Test-Path $cmakePath) {
    $cmake = Get-Content $cmakePath -Raw
    $asmRef = "src/asm/$asmName.asm"

    if ($cmake -notmatch [regex]::Escape($asmRef)) {
        # Insert after the last ASM_KERNEL_SOURCES entry (before the closing paren)
        $cmake = $cmake -replace '(\s+)(# src/RawrXD_Complete_ReverseEngineered\.asm[^\n]*\n\s*\))',
            "`$1$asmRef`n`$1`$2"

        if (-not $DryRun) {
            $cmake | Set-Content $cmakePath -NoNewline
            Write-Host "    Added $asmRef to ASM_KERNEL_SOURCES" -ForegroundColor Green
        } else {
            Write-Host "    [DRY RUN] Would add: $asmRef" -ForegroundColor Yellow
        }
    } else {
        Write-Host "    Already in CMakeLists.txt (idempotent)" -ForegroundColor Gray
    }

    # Add define
    $define = "DRAWRXD_LINK_SIDEBAR_ASM"
    if ($cmake -notmatch $define) {
        $cmake = $cmake -replace '(add_definitions\(-DRAWRXD_LINK_VISION_PROJECTION_ASM=1\))',
            "`$1`n    add_definitions(-D${define}=1)"
        if (-not $DryRun) {
            $cmake | Set-Content $cmakePath -NoNewline
            Write-Host "    Added -D$define=1" -ForegroundColor Green
        }
    }
} else {
    Write-Host "    CMakeLists.txt not found in current directory" -ForegroundColor Yellow
}

# ─────────────────────────────────────────────────────────
# Summary
# ─────────────────────────────────────────────────────────
Write-Host ""
Write-Host "================================================================" -ForegroundColor Green
Write-Host "  [+] Surgical complete. Qt logging exterminated." -ForegroundColor Green
Write-Host "  [+] MASM64 sidebar: $asmRef" -ForegroundColor Green
Write-Host "  [+] Exports: Logger, DebugEngine, TreeVirt, DarkMode" -ForegroundColor Green
Write-Host "================================================================" -ForegroundColor Green
Write-Host ""
Write-Host "  Build: cmake --build build --config Release" -ForegroundColor Cyan
Write-Host "  Or:    ml64 /c /Fo`"$objPath`" `"$asmPath`"" -ForegroundColor Cyan
Write-Host ""
