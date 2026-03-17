# ============================================================
# RAWRXD Object File Blacklist + COFF Validator
# ============================================================
# PURPOSE: Permanently blocks known-corrupt / zero-byte / non-COFF
#          .obj files from ever entering the link pipeline.
#
# USAGE:   . .\obj_blacklist.ps1          # dot-source to import
#          $clean = Get-CleanObjects       # returns validated FileInfo[]
#
# REVERSE-ENGINEERED FROM: genesis_bruteforce_link.ps1
#   The brute force shim trial-and-error removed objects that caused
#   LNK1136 (corrupt file).  This script replaces that with:
#     1. Static blacklist of known-bad object names
#     2. Dynamic COFF header validation (magic bytes check)
#     3. Zero-byte / tiny-file rejection
#     4. Runtime library mismatch detection
#     5. Duplicate name resolution (newest wins)
# ============================================================

$ErrorActionPreference = "Continue"

$Script:Root = "D:\rawrxd"

# ---- STATIC BLACKLIST ----
# Objects permanently identified as corrupt/incompatible by the
# brute force linker or manual audit.  LNK1136 = corrupt file format.
$Script:BlacklistedNames = @(
    # --- Brute-force-identified corrupt (0 bytes, LNK1136) ---
    "omega_final_working.obj",
    "omega_final.obj",
    "omega_simple.obj",
    "omega_pro_maximum.obj",
    "omega_professional.obj",

    # --- Excluded by original shim patterns ---
    "omega_pro.obj",
    "OmegaPolyglot_v5.obj",
    "dumpbin_final.obj",

    # --- Corrupt .pdata (LNK1223) ---
    "RawrXD_Prod.obj",
    "nvme_oracle_service.obj",
    "proof.obj",
    "NetworkRelay.obj",
    "RawrXD_NetworkRelay.obj",
    "RawrXD_P2PRelay.obj",

    # --- Fake/stub compiler objects (3 bytes each, not COFF) ---
    "c_compiler_from_scratch.obj",
    "carbon_compiler_from_scratch.obj",
    "clojure_compiler_from_scratch.obj",
    "cobol_compiler_from_scratch.obj",
    "cross_compiler.obj",
    "elixir_compiler_from_scratch.obj",
    "eon_compiler_complete.obj",
    "eon_compiler_from_scratch.obj",
    "eon_compiler_main.obj",
    "f__compiler_from_scratch.obj",
    "fortran_compiler_from_scratch.obj",
    "full_eon_compiler.obj",
    "jai_compiler_from_scratch.obj",
    "javascript_compiler_from_scratch.obj",
    "kotlin_compiler_from_scratch.obj",
    "lua_compiler_from_scratch.obj",
    "master_universal_compiler.obj",
    "multi_target_compiler.obj",
    "n0mn0m_cross_platform_compiler.obj",
    "n0mn0m_quantum_asm_compiler.obj",
    "perl_compiler_from_scratch.obj",
    "php_compiler_from_scratch.obj",
    "python_compiler_from_scratch.obj",
    "r_compiler_from_scratch.obj",
    "reverser_compiler_from_scratch.obj",
    "reverser_compiler.obj",
    "ruby_compiler_from_scratch.obj",
    "scala_compiler_from_scratch.obj",
    "swift_compiler_from_scratch.obj",
    "universal_multi_language_compiler.obj",
    "vb_net_compiler_from_scratch.obj",
    "ada_compiler_from_scratch_fixed.obj",
    "ada_compiler_from_scratch.obj",

    # --- Test/audit stubs (not real objects) ---
    "asm_MASM_STACK_ALIGNMENT_AUDIT_REPORT.obj",
    "test.obj",
    "test_asm.exe.obj",
    "test_hello.obj"
)

# ---- DIRECTORY EXCLUSIONS ----
# Directories whose .obj files should never enter the link pipeline.
$Script:ExcludeDirPatterns = @(
    '\\\.git\\',
    '\\dist\\',
    '\\node_modules\\',
    '\\temp\\',
    '\\source_audit\\',
    '\\crash_dumps\\',
    '\\build_fresh\\',
    '\\build_gold\\',
    '\\build_prod\\',
    '\\build_universal\\',
    '\\build_msvc\\',
    '\\build_qt_free\\',
    '\\build_win32_gui_test\\',
    '\\build_clean\\',
    '\\build_ide_ninja\\',
    '\\build_new\\',
    '\\build_test_parse\\',
    '\\CMakeFiles\\',
    '\\build\\',
    '\\compilers\\',
    '\\test_output_',
    '\\toolchain\\from_scratch\\',
    '\\itsmehrawrxd-master\\bin\\compilers\\'
)

# ---- NAME PATTERN EXCLUSIONS ----
# Regex patterns for object names that should never link.
$Script:ExcludeNamePatterns = @(
    '\.cpp\.obj$',         # CMake intermediate (double-extension)
    '\.c\.obj$',
    '\.cc\.obj$',
    '^bench_',             # Benchmark objects
    '^test_',              # Test objects
    'compiler_from_scratch',
    '^omega_pro',          # All omega_pro variants
    '^OmegaPolyglot'
)

# ---- LNK2038 MISMATCH BLOCKLIST ----
# Objects compiled with /MD (dynamic CRT) that conflict with /MT build.
# These cause LNK2038: RuntimeLibrary mismatch MD vs MT.
$Script:RuntimeMismatchObjects = @(
    "FeatureFlags.obj",
    "flash_attn_avx2.obj",
    "flash_attn_optimized.obj",
    # Ship/ directory objects compiled with /MD
    "RawrXD_InferenceEngine_Win32.obj",
    "RawrXD_FoundationTest.obj",
    "RawrXD_Foundation_Integration.obj",
    "RawrXD_ResourceManager_Win32.obj",
    "RawrXD_TextEditor_Win32.obj",
    "RawrXD_FileManager_Win32.obj",
    "RawrXD_TerminalManager_Win32.obj",
    "RawrXD_SettingsManager_Win32.obj",
    "RawrXD_MainWindow_Win32.obj",
    "RawrXD_SystemMonitor.obj",
    "RawrXD_TaskScheduler.obj",
    "RawrXD_ModelLoader.obj",
    "RawrXD_MemoryManager.obj",
    "RawrXD_AgentPool.obj",
    "RawrXD_AdvancedCodingAgent.obj",
    "RawrXD_Core.obj",
    "RawrXD_AgentCoordinator_Minimal.obj"
)

# ==========================================================
# COFF Header Validator
# ==========================================================
# x64 COFF: first 2 bytes = 0x8664 (IMAGE_FILE_MACHINE_AMD64)
# x86 COFF: first 2 bytes = 0x014C (IMAGE_FILE_MACHINE_I386)
# Anything else => not a valid COFF object.
function Test-ValidCOFF {
    param([string]$Path)
    
    if (-not (Test-Path $Path)) { return $false }
    $info = Get-Item $Path
    
    # Zero-byte or tiny files are never valid COFF
    if ($info.Length -lt 20) { return $false }
    
    try {
        $fs = [System.IO.File]::OpenRead($Path)
        $header = New-Object byte[] 2
        $read = $fs.Read($header, 0, 2)
        $fs.Close()
        
        if ($read -lt 2) { return $false }
        
        $machine = [BitConverter]::ToUInt16($header, 0)
        # Accept x64 (0x8664) or x86 (0x014C) COFF
        return ($machine -eq 0x8664 -or $machine -eq 0x014C)
    } catch {
        return $false
    }
}

# ==========================================================
# Master Object Collector + Validator
# ==========================================================
function Get-CleanObjects {
    param(
        [string]$RootPath = $Script:Root,
        [switch]$Verbose,
        [switch]$SkipCOFFValidation
    )
    
    $stats = @{
        Total       = 0
        Blacklisted = 0
        DirExcluded = 0
        NameExcluded = 0
        ZeroByte    = 0
        InvalidCOFF = 0
        Duplicate   = 0
        Mismatch    = 0
        Clean       = 0
    }
    
    # Collect ALL .obj recursively
    $allObjs = Get-ChildItem -Path $RootPath -Filter "*.obj" -Recurse -ErrorAction SilentlyContinue
    $stats.Total = $allObjs.Count
    
    $survivors = @()
    
    foreach ($obj in $allObjs) {
        $path = $obj.FullName
        $name = $obj.Name
        
        # 1. Static blacklist check
        if ($name -in $Script:BlacklistedNames) {
            $stats.Blacklisted++
            if ($Verbose) { Write-Host "  [BLOCKED] Blacklisted: $name" -Fore DarkRed }
            continue
        }
        
        # 2. Directory exclusion check
        $dirBlocked = $false
        foreach ($pattern in $Script:ExcludeDirPatterns) {
            if ($path -match $pattern) {
                $dirBlocked = $true
                break
            }
        }
        if ($dirBlocked) {
            $stats.DirExcluded++
            if ($Verbose) { Write-Host "  [BLOCKED] Dir excluded: $name ($($obj.DirectoryName))" -Fore DarkYellow }
            continue
        }
        
        # 3. Name pattern exclusion
        $nameBlocked = $false
        foreach ($pattern in $Script:ExcludeNamePatterns) {
            if ($name -match $pattern) {
                $nameBlocked = $true
                break
            }
        }
        if ($nameBlocked) {
            $stats.NameExcluded++
            if ($Verbose) { Write-Host "  [BLOCKED] Name pattern: $name" -Fore DarkYellow }
            continue
        }
        
        # 4. Zero-byte check
        if ($obj.Length -eq 0) {
            $stats.ZeroByte++
            if ($Verbose) { Write-Host "  [BLOCKED] Zero-byte: $name" -Fore Red }
            continue
        }
        
        # 5. Runtime mismatch check
        if ($name -in $Script:RuntimeMismatchObjects) {
            $stats.Mismatch++
            if ($Verbose) { Write-Host "  [BLOCKED] CRT mismatch (/MD vs /MT): $name" -Fore DarkMagenta }
            continue
        }
        
        # 6. COFF header validation (unless skipped for speed)
        if (-not $SkipCOFFValidation) {
            if (-not (Test-ValidCOFF $path)) {
                $stats.InvalidCOFF++
                if ($Verbose) { Write-Host "  [BLOCKED] Invalid COFF: $name ($($obj.Length) bytes)" -Fore Red }
                continue
            }
        }
        
        $survivors += $obj
    }
    
    # 7. Deduplicate by name (keep most recent)
    $deduped = $survivors | Sort-Object LastWriteTime -Descending | Group-Object Name | ForEach-Object {
        if ($_.Count -gt 1) { $stats.Duplicate += ($_.Count - 1) }
        $_.Group[0]
    }
    
    $stats.Clean = $deduped.Count
    
    # Report
    Write-Host ""
    Write-Host "=== OBJECT VALIDATION REPORT ===" -Fore Cyan
    Write-Host "  Total scanned:      $($stats.Total)" -Fore White
    Write-Host "  Blacklisted:        $($stats.Blacklisted)" -Fore $(if($stats.Blacklisted -gt 0){'Red'}else{'Green'})
    Write-Host "  Dir excluded:       $($stats.DirExcluded)" -Fore $(if($stats.DirExcluded -gt 0){'Yellow'}else{'Green'})
    Write-Host "  Name excluded:      $($stats.NameExcluded)" -Fore $(if($stats.NameExcluded -gt 0){'Yellow'}else{'Green'})
    Write-Host "  Zero-byte:          $($stats.ZeroByte)" -Fore $(if($stats.ZeroByte -gt 0){'Red'}else{'Green'})
    Write-Host "  CRT mismatch:       $($stats.Mismatch)" -Fore $(if($stats.Mismatch -gt 0){'Magenta'}else{'Green'})
    Write-Host "  Invalid COFF:       $($stats.InvalidCOFF)" -Fore $(if($stats.InvalidCOFF -gt 0){'Red'}else{'Green'})
    Write-Host "  Duplicates removed: $($stats.Duplicate)" -Fore $(if($stats.Duplicate -gt 0){'Yellow'}else{'Green'})
    Write-Host "  --------------------------------"
    Write-Host "  CLEAN OBJECTS:      $($stats.Clean)" -Fore Green
    Write-Host ""
    
    return $deduped
}

# ==========================================================
# Quarantine corrupt objects  (moves to .quarantine/)
# ==========================================================
function Invoke-QuarantineCorrupt {
    param([string]$RootPath = $Script:Root)
    
    $quarantine = Join-Path $RootPath ".quarantine"
    if (!(Test-Path $quarantine)) { New-Item -ItemType Directory -Path $quarantine -Force | Out-Null }
    
    $moved = 0
    
    # Move zero-byte .obj
    Get-ChildItem -Path $RootPath -Filter "*.obj" -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.Length -eq 0 -and $_.DirectoryName -notmatch '\\\.quarantine\\' } |
        ForEach-Object {
            $dest = Join-Path $quarantine $_.Name
            Move-Item $_.FullName $dest -Force -ErrorAction SilentlyContinue
            $moved++
        }
    
    # Move blacklisted objects
    Get-ChildItem -Path $RootPath -Filter "*.obj" -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -in $Script:BlacklistedNames -and $_.DirectoryName -notmatch '\\\.quarantine\\' } |
        ForEach-Object {
            $dest = Join-Path $quarantine $_.Name
            Move-Item $_.FullName $dest -Force -ErrorAction SilentlyContinue
            $moved++
        }
    
    # Move non-COFF tiny objects
    Get-ChildItem -Path $RootPath -Filter "*.obj" -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.Length -gt 0 -and $_.Length -lt 20 -and $_.DirectoryName -notmatch '\\\.quarantine\\' } |
        ForEach-Object {
            if (-not (Test-ValidCOFF $_.FullName)) {
                $dest = Join-Path $quarantine $_.Name
                Move-Item $_.FullName $dest -Force -ErrorAction SilentlyContinue
                $moved++
            }
        }
    
    Write-Host "[QUARANTINE] Moved $moved corrupt/blacklisted objects to $quarantine" -Fore Yellow
}

# ==========================================================
# Export if loaded as a module (ignored when dot-sourced)
# ==========================================================
if ($MyInvocation.MyCommand.ScriptBlock.Module) {
    Export-ModuleMember -Function Get-CleanObjects, Test-ValidCOFF, Invoke-QuarantineCorrupt
}
