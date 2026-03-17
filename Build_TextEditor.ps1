# ============================================================================
# Build_TextEditor.ps1 - RawrXD Text Editor Build Script
# ============================================================================
# Compiles all MASM modules and links into executable
# ============================================================================

param(
    [string]$BuildMode = "Release",
    [string]$OutputDir = ".\build\text-editor"
)

# Configuration
$ToolRoots = @(
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC",
    "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC"
)

function Resolve-ToolPath {
    param(
        [string]$ToolName
    )

    $cmd = Get-Command $ToolName -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    foreach ($root in $ToolRoots) {
        if (-not (Test-Path $root)) {
            continue
        }

        $match = Get-ChildItem -Path $root -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending |
            ForEach-Object {
                $candidate = Join-Path $_.FullName "bin\Hostx64\x64\$ToolName"
                if (Test-Path $candidate) { return $candidate }
            } |
            Select-Object -First 1

        if ($match) {
            return $match
        }
    }

    return $null
}

$ML64 = Resolve-ToolPath "ml64.exe"
$LINK = Resolve-ToolPath "link.exe"
$SourceDir = "."
$Modules = @(
    "RawrXD_TextBuffer.asm",
    "RawrXD_CursorTracker.asm",
    "RawrXD_TextEditorGUI.asm",
    "RawrXD_TextEditor_Main.asm"
)

# Create output directory
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

Write-Host "╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║ RawrXD Text Editor - Build System                            ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

if (-not $ML64) {
    Write-Host "  ✗ Unable to locate ml64.exe" -ForegroundColor Red
    exit 1
}

if (-not $LINK) {
    Write-Host "  ✗ Unable to locate link.exe" -ForegroundColor Red
    exit 1
}

# Step 1: Assemble all modules
Write-Host "[Step 1] Assembling MASM modules..." -ForegroundColor Yellow

$ObjFiles = @()

foreach ($Module in $Modules) {
    $SourceFile = Join-Path $SourceDir $Module
    $ObjFile = Join-Path $OutputDir "$($Module -replace '.asm', '.obj')"
    
    if (-not (Test-Path $SourceFile)) {
        Write-Host "  ✗ Not found: $SourceFile" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "  ▶ $Module"
    
    # Assemble with ml64
    & $ML64 /c /D_AMD64_ /Fo"$ObjFile" "$SourceFile" 2>&1 | Out-Null
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "    ✓ Assembled: $($ObjFile | Split-Path -Leaf)" -ForegroundColor Green
        $ObjFiles += $ObjFile
    } else {
        Write-Host "    ✗ Assembly failed: $Module" -ForegroundColor Red
        Write-Host "    Run: ml64 /c $SourceFile" -ForegroundColor Gray
        exit 1
    }
}

Write-Host ""
Write-Host "[Step 1] ✓ All modules assembled successfully" -ForegroundColor Green
Write-Host ""

# Step 2: Link object files
Write-Host "[Step 2] Linking executable..." -ForegroundColor Yellow

$ExeFile = Join-Path $OutputDir "RawrXD_TextEditor.exe"
$ObjFileList = $ObjFiles -join " "

Write-Host "  ▶ Linking: $($ExeFile | Split-Path -Leaf)"

# Build linker command
$LinkArgs = @(
    "/SUBSYSTEM:WINDOWS",
    "/ENTRY:main",
    "/OUT:`"$ExeFile`"",
    "kernel32.lib",
    "user32.lib",
    "gdi32.lib",
    "comctl32.lib"
) + $ObjFiles

& $LINK @LinkArgs 2>&1 | Out-Null

if ($LASTEXITCODE -eq 0) {
    Write-Host "    ✓ Linked: $(Split-Path $ExeFile -Leaf)" -ForegroundColor Green
} else {
    Write-Host "    ✗ Linking failed" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "[Step 2] ✓ Executable linked successfully" -ForegroundColor Green
Write-Host ""

# Step 3: Verify output
Write-Host "[Step 3] Verifying build output..." -ForegroundColor Yellow

if (Test-Path $ExeFile) {
    $FileSize = (Get-Item $ExeFile).Length
    Write-Host "  ✓ Executable created: $(Split-Path $ExeFile -Leaf)" -ForegroundColor Green
    Write-Host "    Size: $($FileSize / 1024)KB" -ForegroundColor Gray
    Write-Host "    Location: $ExeFile" -ForegroundColor Gray
} else {
    Write-Host "  ✗ Executable not found" -ForegroundColor Red
    exit 1
}

Write-Host ""

# Step 4: Summary
Write-Host "╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║ Build Complete! ✓                                            ║" -ForegroundColor Green
Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Green
Write-Host ""
Write-Host "Build Output:" -ForegroundColor Cyan
Write-Host "  Executable: $ExeFile" -ForegroundColor Green
Write-Host "  Object Files: $(($ObjFiles.Count)) files" -ForegroundColor Green
Write-Host ""
Write-Host "To run:" -ForegroundColor Yellow
Write-Host "  .\$($ExeFile | Split-Path -Leaf)" -ForegroundColor Gray
Write-Host ""
Write-Host "Features:" -ForegroundColor Cyan
Write-Host "  ✓ Multi-line text editing" -ForegroundColor Gray
Write-Host "  ✓ Cursor position tracking (line/column)" -ForegroundColor Gray
Write-Host "  ✓ Keyboard navigation (arrows, Home, End, Page Up/Down)" -ForegroundColor Gray
Write-Host "  ✓ Text selection and highlighting" -ForegroundColor Gray
Write-Host "  ✓ Cursor blinking animation" -ForegroundColor Gray
Write-Host "  ✓ Mouse click positioning" -ForegroundColor Gray
Write-Host "  ✓ Line number rendering" -ForegroundColor Gray
Write-Host ""

exit 0
