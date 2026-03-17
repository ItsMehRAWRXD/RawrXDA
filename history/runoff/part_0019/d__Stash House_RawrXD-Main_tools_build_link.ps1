<#
.SYNOPSIS
    Phase 2: RawrXD Linker — Custom PE/COFF64 Linker
.DESCRIPTION
    Links compiled .obj files from Phase 1 into a PE64 executable.
    Stamps the binary with a thermal profile signature at PE offset 0x40.
    
    Resolution chain: RawrXD-LINK → link.exe (VS scaffold)
    Once phase2_linker (coff_reader, section_merge, pe_writer) is complete,
    this script uses RawrXD-LINK instead of Microsoft's link.exe.
.PARAMETER ObjDir
    Directory containing .obj files from Phase 1
.PARAMETER BinDir
    Output directory for the linked executable
.PARAMETER Target
    Output executable name (without .exe)
.PARAMETER Subsystem
    PE subsystem: WINDOWS or CONSOLE
.PARAMETER UseRawrLink
    Use RawrXD-LINK instead of link.exe (self-hosting mode)
.PARAMETER ThermalSignature
    Thermal mode stamp written at PE offset 0x40 (max 11 bytes)
.EXAMPLE
    .\build_link.ps1 -Target "RawrXD-AgenticIDE" -Subsystem WINDOWS
#>
param(
    [string]$ObjDir = "$env:LOCALAPPDATA\RawrXD\obj",
    [string]$BinDir = "$env:LOCALAPPDATA\RawrXD\bin",
    [ValidateSet("RawrXD-AgenticIDE","RawrXD-Win32IDE","RawrXD-Agent","RawrXD-CLI")]
    [string]$Target = "RawrXD-AgenticIDE",
    [ValidateSet("WINDOWS","CONSOLE")]
    [string]$Subsystem = "WINDOWS",
    [switch]$UseRawrLink,
    [string]$ThermalSignature = "IGUANA_MODE"
)

$ErrorActionPreference = "Stop"
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

function Write-Phase2Log {
    param([string]$Message, [string]$Level = "Info")
    $ts = Get-Date -Format "HH:mm:ss.fff"
    $colorMap = @{ Info="White"; Success="Green"; Warning="Yellow"; Error="Red" }
    Write-Host "[$ts] [PHASE2::LINK] $Message" -ForegroundColor $colorMap[$Level]
}

# ═══════════════════════════════════════════════════════════════
# Linker Resolution (RawrXD-LINK → link.exe fallback)
# ═══════════════════════════════════════════════════════════════
$linker = $null

# ── Resolve paths relative to the repo ──
$repoRoot = (Resolve-Path "$PSScriptRoot\..").Path
$repoBinDir = Join-Path $repoRoot "build\bin"

if ($UseRawrLink) {
    $rawrLinkCandidates = @(
        (Join-Path $repoBinDir "rawrxd_link.exe"),
        "$env:LOCALAPPDATA\RawrXD\bin\rawrxd_link.exe",
        "D:\RawrXD\bin\rawrxd_link.exe"
    )
    foreach ($rlp in $rawrLinkCandidates) {
        if (Test-Path $rlp) {
            $linker = $rlp
            Write-Phase2Log "Self-hosting: using RawrXD-LINK at $rlp" "Success"
            break
        }
    }
    if (!$linker) {
        Write-Phase2Log "RawrXD-LINK not found, falling back to link.exe" "Warning"
    }
}

if (!$linker) {
    $linkCandidates = @(
        "link.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe",
        "C:\masm64\bin\link.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.42.34433\bin\Hostx64\x64\link.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.40.33807\bin\Hostx64\x64\link.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\14.42.34433\bin\Hostx64\x64\link.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.42.34433\bin\Hostx64\x64\link.exe",
        "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe",
        "D:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.42.34433\bin\Hostx64\x64\link.exe"
    )
    
    foreach ($candidate in $linkCandidates) {
        if ($candidate -eq "link.exe") {
            $found = Get-Command $candidate -ErrorAction SilentlyContinue
            if ($found) { $linker = $found.Source; break }
        } elseif (Test-Path $candidate) {
            $linker = $candidate
            break
        }
    }
    
    # Last resort: wildcard scan
    if (!$linker) {
        $wildcard = Get-ChildItem -Path "C:\Program Files*\Microsoft Visual Studio\2022" -Filter "link.exe" -Recurse -ErrorAction SilentlyContinue | Where-Object { $_.FullName -match 'Hostx64\\x64' } | Select-Object -First 1
        if ($wildcard) { $linker = $wildcard.FullName }
    }
    
    if (!$linker) {
        throw "FATAL: No linker found. Install VS Build Tools or run: .\toolchain\masm\Build-Fortress-Compiler-Linker.ps1"
    }
    Write-Phase2Log "Using link.exe scaffold: $linker" "Info"
}

# ═══════════════════════════════════════════════════════════════
# Object Collection
# ═══════════════════════════════════════════════════════════════
New-Item -ItemType Directory -Path $BinDir -Force | Out-Null

$objects = Get-ChildItem -Path $ObjDir -Filter "*.obj" -ErrorAction SilentlyContinue
if (!$objects -or $objects.Count -eq 0) {
    throw "No .obj files found in $ObjDir. Run Phase 1 (build_scc.ps1) first."
}

$objList = ($objects | Select-Object -ExpandProperty FullName) -join " "
Write-Phase2Log "Linking $($objects.Count) object files" "Info"

# ═══════════════════════════════════════════════════════════════
# System Libraries
# ═══════════════════════════════════════════════════════════════
$libs = @(
    "kernel32.lib", "user32.lib", "gdi32.lib", "advapi32.lib",
    "ole32.lib", "oleaut32.lib", "shell32.lib", "comdlg32.lib",
    "ws2_32.lib", "wininet.lib", "comctl32.lib", "shlwapi.lib",
    "uuid.lib", "dbghelp.lib"
) -join " "

# ═══════════════════════════════════════════════════════════════
# Linker Flags
# ═══════════════════════════════════════════════════════════════
$entryPoint = if ($Subsystem -eq "WINDOWS") { "WinMainCRTStartup" } else { "mainCRTStartup" }

$linkFlags = @(
    "/SUBSYSTEM:$Subsystem",
    "/ENTRY:$entryPoint",
    "/MACHINE:X64",
    "/LARGEADDRESSAWARE",
    "/HIGHENTROPYVA",
    "/DYNAMICBASE",
    "/NXCOMPAT",
    "/RELEASE",
    "/OPT:REF",
    "/OPT:ICF",
    "/SECTION:.text,ERW",
    "/ALIGN:0x1000",
    "/FILEALIGN:0x200",
    "/MANIFEST:EMBED",
    "/MANIFESTUAC:`"level='asInvoker' uiAccess='false'`""
) -join " "

$exe = Join-Path $BinDir "$Target.exe"
$pdb = Join-Path $BinDir "$Target.pdb"
$map = Join-Path $BinDir "$Target.map"

$linkCmd = "& `"$linker`" $linkFlags /OUT:`"$exe`" /PDB:`"$pdb`" /MAP:`"$map`" $objList $libs 2>&1"

Write-Phase2Log "Executing linker..." "Info"
$output = Invoke-Expression $linkCmd

if ($LASTEXITCODE -ne 0) {
    Write-Phase2Log "Linker output:" "Error"
    $output | ForEach-Object { Write-Phase2Log "  $_" "Error" }
    throw "Link failed with exit code $LASTEXITCODE"
}

# ═══════════════════════════════════════════════════════════════
# Thermal Signature Stamp (PE offset 0x40)
# ═══════════════════════════════════════════════════════════════
if (Test-Path $exe) {
    try {
        $bytes = [System.IO.File]::ReadAllBytes($exe)
        $sigBytes = [System.Text.Encoding]::ASCII.GetBytes($ThermalSignature.PadRight(11, [char]0x00).Substring(0, 11))
        
        # Verify we're past the DOS header but before PE signature
        if ($bytes.Length -gt 0x50) {
            [System.Buffer]::BlockCopy($sigBytes, 0, $bytes, 0x40, $sigBytes.Length)
            [System.IO.File]::WriteAllBytes($exe, $bytes)
            Write-Phase2Log "Thermal signature stamped: '$ThermalSignature' at offset 0x40" "Success"
        } else {
            Write-Phase2Log "Binary too small for thermal stamp" "Warning"
        }
    } catch {
        Write-Phase2Log "Could not stamp thermal signature: $_" "Warning"
    }
}

$stopwatch.Stop()

# ═══════════════════════════════════════════════════════════════
# Summary
# ═══════════════════════════════════════════════════════════════
if (Test-Path $exe) {
    $info = Get-Item $exe
    $sizeMB = [math]::Round($info.Length / 1MB, 2)
    
    Write-Host ""
    Write-Phase2Log "Phase 2 Complete" "Success"
    Write-Phase2Log "  Target:     $Target" "Info"
    Write-Phase2Log "  Output:     $exe" "Info"
    Write-Phase2Log "  Size:       $sizeMB MB" "Info"
    Write-Phase2Log "  Objects:    $($objects.Count) linked" "Info"
    Write-Phase2Log "  Subsystem:  $Subsystem" "Info"
    Write-Phase2Log "  Thermal:    $ThermalSignature" "Info"
    Write-Phase2Log "  Elapsed:    $($stopwatch.Elapsed.ToString('mm\:ss\.fff'))" "Info"
    if (Test-Path $pdb) {
        $pdbSize = [math]::Round((Get-Item $pdb).Length / 1MB, 2)
        Write-Phase2Log "  PDB:        $pdbSize MB" "Info"
    }
    Write-Host ""
} else {
    throw "Link produced no output: $exe not found"
}
