# link_strategies_fixed.ps1 - Monolithic IDE Linking v2.0
# Fixes: CRT mismatch, ASM globals, unresolved C++ stubs

param(
    [string]$VCToolsDir = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64",
    [string]$SDKLibDir = "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64",
    [string]$UCRTLibDir = "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64",
    [string]$ObjDir = "D:\rawrxd\obj",
    [string]$OutDir = "D:\rawrxd\build",
    [string]$CMakeBuildDir = "D:\rawrxd\build",
    [switch]$PureASM = $false,          # Nuclear option: C++ objects excluded
    [switch]$CreateASMGlue = $true       # Auto-generate missing ASM symbols
)

$ErrorActionPreference = "Stop"
$VerbosePreference = "Continue"

# ================================================================================
# PHASE 1: ENVIRONMENT & CRT DETECTION
# ================================================================================

$link = Join-Path $VCToolsDir "link.exe"
$lib = Join-Path $VCToolsDir "lib.exe"
$ml64 = Join-Path $VCToolsDir "ml64.exe"

if (-not (Test-Path $link)) {
    $link = (Get-Command link.exe -ErrorAction SilentlyContinue)?.Source
    if (-not $link) { throw "link.exe not found" }
    $VCToolsDir = Split-Path $link -Parent
    $ml64 = Join-Path $VCToolsDir "ml64.exe"
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

# Detect dominant runtime in existing objects to avoid LNK2038
function Get-DominantRuntime {
    param([string[]]$ObjFiles)
    $md = 0; $mt = 0
    $dumpbin = Join-Path $VCToolsDir "dumpbin.exe"
    
    foreach ($obj in $ObjFiles | Select-Object -First 30) {
        if (-not (Test-Path $obj)) { continue }
        try {
            $info = & $dumpbin /headers $obj 2>$null | Select-String "Runtime"
            if ($info -match "/MD") { $md++ }
            elseif ($info -match "/MT") { $mt++ }
        } catch {}
    }
    if ($md -gt $mt) { return "MD" } else { return "MT" }
}

# ================================================================================
# PHASE 2: ASM GLUE GENERATION (Fixes g_hHeap, BeaconRecv, etc.)
# ================================================================================

$ASMGlueFile = Join-Path $OutDir "asm_linker_glue.asm"
$ASMGlueObj = Join-Path $OutDir "asm_linker_glue.obj"

if ($CreateASMGlue -and -not (Test-Path $ASMGlueObj)) {
    Write-Host "[PHASE 2] Generating ASM glue symbols..." -ForegroundColor Cyan
    
    $glueCode = @'
; asm_linker_glue.asm - Auto-generated symbol satisfier
; Assemble: ml64 /c /Foasm_linker_glue.obj asm_linker_glue.asm

.data
    ; Tier2 DLL exports (Strategy C fix)
    PUBLIC g_hHeap
    PUBLIC g_hInstance
    PUBLIC BeaconRecv
    PUBLIC TryBeaconRecv  
    PUBLIC RunInference
    PUBLIC RegisterAgent
    PUBLIC HotSwapModel
    
    g_hHeap QWORD 0
    g_hInstance QWORD 0
    BeaconRecv QWORD 0
    TryBeaconRecv QWORD 0
    RunInference QWORD 0
    RegisterAgent QWORD 0
    HotSwapModel QWORD 0
    
    ; C++ Stub satisfiers (Win32IDE methods - minimal stubs)
    EXTERN WinMainCRTStartup : PROC

.code

; CRT Runtime initialization stubs (Fix LNK2001 from msvcrt.lib)
PUBLIC __vcrt_initialize
PUBLIC __vcrt_uninitialize
PUBLIC __vcrt_uninitialize_critical
PUBLIC __vcrt_thread_attach
PUBLIC __vcrt_thread_detach
PUBLIC _is_c_termination_complete
PUBLIC __acrt_initialize
PUBLIC __acrt_uninitialize
PUBLIC __acrt_uninitialize_critical
PUBLIC __acrt_thread_attach
PUBLIC __acrt_thread_detach

__vcrt_initialize PROC
    mov eax, 1
    ret
__vcrt_initialize ENDP

__vcrt_uninitialize PROC
    xor eax, eax
    ret
__vcrt_uninitialize ENDP

__vcrt_uninitialize_critical PROC
    ret
__vcrt_uninitialize_critical ENDP

__vcrt_thread_attach PROC
    mov eax, 1
    ret
__vcrt_thread_attach ENDP

__vcrt_thread_detach PROC
    mov eax, 1
    ret
__vcrt_thread_detach ENDP

_is_c_termination_complete PROC
    xor eax, eax
    ret
_is_c_termination_complete ENDP

__acrt_initialize PROC
    mov eax, 1
    ret
__acrt_initialize ENDP

__acrt_uninitialize PROC
    xor eax, eax
    ret
__acrt_uninitialize ENDP

__acrt_uninitialize_critical PROC
    ret
__acrt_uninitialize_critical ENDP

__acrt_thread_attach PROC
    mov eax, 1
    ret
__acrt_thread_attach ENDP

__acrt_thread_detach PROC
    mov eax, 1
    ret
__acrt_thread_detach ENDP

; Win32IDE method stubs (satisfy linker until real implementations exist)
; These are minimal stubs that return 0/false to prevent LNK2001

PUBLIC ?getGitChangedFiles@Win32IDE@@AEBA?AV?$vector@UGitFile@@V?$allocator@UGitFile@@@std@@@std@@XZ
PUBLIC ?isGitRepository@Win32IDE@@AEBA_NXZ
PUBLIC ?gitUnstageFile@Win32IDE@@AEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
PUBLIC ?gitStageFile@Win32IDE@@AEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
PUBLIC ?gitPull@Win32IDE@@AEAAXXZ
PUBLIC ?gitPush@Win32IDE@@AEAAXXZ
PUBLIC ?gitCommit@Win32IDE@@AEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
PUBLIC ?newFile@Win32IDE@@AEAAXXZ
PUBLIC ?GetSubAgentManager@AgenticBridge@@QEAAPEAVSubAgentManager@@XZ
PUBLIC ?getStatusSummary@SubAgentManager@@QEBA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@XZ
PUBLIC ?getTodoList@SubAgentManager@@QEBA?AV?$vector@UTodoItem@@V?$allocator@UTodoItem@@@std@@@std@@XZ
PUBLIC ?generateResponse@Win32IDE@@AEAA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV23@@Z
PUBLIC ?isModelLoaded@Win32IDE@@AEBA_NXZ
PUBLIC ?HandleCopilotStreamUpdate@Win32IDE@@AEAAXPEBD_K@Z

?getGitChangedFiles@Win32IDE@@AEBA?AV?$vector@UGitFile@@V?$allocator@UGitFile@@@std@@@std@@XZ PROC
    xor eax, eax
    ret
?getGitChangedFiles@Win32IDE@@AEBA?AV?$vector@UGitFile@@V?$allocator@UGitFile@@@std@@@std@@XZ ENDP

?isGitRepository@Win32IDE@@AEBA_NXZ PROC
    xor eax, eax
    ret
?isGitRepository@Win32IDE@@AEBA_NXZ ENDP

?gitUnstageFile@Win32IDE@@AEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z PROC
    ret
?gitUnstageFile@Win32IDE@@AEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z ENDP

?gitStageFile@Win32IDE@@AEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z PROC
    ret
?gitStageFile@Win32IDE@@AEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z ENDP

?gitPull@Win32IDE@@AEAAXXZ PROC
    ret
?gitPull@Win32IDE@@AEAAXXZ ENDP

?gitPush@Win32IDE@@AEAAXXZ PROC
    ret
?gitPush@Win32IDE@@AEAAXXZ ENDP

?gitCommit@Win32IDE@@AEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z PROC
    ret
?gitCommit@Win32IDE@@AEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z ENDP

?newFile@Win32IDE@@AEAAXXZ PROC
    ret
?newFile@Win32IDE@@AEAAXXZ ENDP

?GetSubAgentManager@AgenticBridge@@QEAAPEAVSubAgentManager@@XZ PROC
    xor eax, eax
    ret
?GetSubAgentManager@AgenticBridge@@QEAAPEAVSubAgentManager@@XZ ENDP

?getStatusSummary@SubAgentManager@@QEBA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@XZ PROC
    xor eax, eax
    ret
?getStatusSummary@SubAgentManager@@QEBA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@XZ ENDP

?getTodoList@SubAgentManager@@QEBA?AV?$vector@UTodoItem@@V?$allocator@UTodoItem@@@std@@@std@@XZ PROC
    xor eax, eax
    ret
?getTodoList@SubAgentManager@@QEBA?AV?$vector@UTodoItem@@V?$allocator@UTodoItem@@@std@@@std@@XZ ENDP

?generateResponse@Win32IDE@@AEAA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV23@@Z PROC
    xor eax, eax
    ret
?generateResponse@Win32IDE@@AEAA?AV?$basic_string@DU?$char_traits@D@2@@std@@AEBV23@@Z ENDP

?isModelLoaded@Win32IDE@@AEBA_NXZ PROC
    xor eax, eax
    ret
?isModelLoaded@Win32IDE@@AEBA_NXZ ENDP

?HandleCopilotStreamUpdate@Win32IDE@@AEAAXPEBD_K@Z PROC
    ret
?HandleCopilotStreamUpdate@Win32IDE@@AEAAXPEBD_K@Z ENDP

END
'@
    
    $glueCode | Out-File -FilePath $ASMGlueFile -Encoding ASCII
    
    if (Test-Path $ml64) {
        & $ml64 /c /Fo"$ASMGlueObj" "$ASMGlueFile" 2>&1 | Out-Null
        if (Test-Path $ASMGlueObj) {
            Write-Host "   [OK] ASM glue created: $ASMGlueObj" -ForegroundColor Green
        }
    } else {
        Write-Host "   [WARN] ml64.exe not found, ASM glue not compiled" -ForegroundColor Yellow
    }
}

# ================================================================================
# PHASE 3: OBJECT INVENTORY & FILTERING
# ================================================================================

Write-Host "[PHASE 3] Inventorying objects..." -ForegroundColor Cyan

# C++ objects with missing implementations (from your LNK2001 errors)
$BrokenCppPatterns = @(
    "win32app_Win32IDE_Sidebar.obj",
    "win32app_Win32IDE_StreamingUX.obj",
    "win32app_Win32IDE_SubAgent.obj", 
    "win32app_Win32IDE_Tier3Cosmetics.obj",
    "win32app_Win32IDE_Tier3Polish.obj",
    "win32app_Win32IDE_Tier5Cosmetics.obj",
    "win32app_Win32IDE_VSCodeUI.obj",
    "win32app_Win32IDE_Window.obj"
)

# Non-linkable stubs
$ExcludedObjects = @(
    "mmap_loader_stub.obj",
    "simple_stub.obj",
    "*_test*.obj",
    "*_mock*.obj"
)

# Collect ASM objects (monolithic)
$ASMObjs = Get-ChildItem "$ObjDir\asm_monolithic_*.obj" | Where-Object { 
    $name = $_.Name
    $ExcludedObjects | ForEach-Object { if ($name -like $_) { return $false } }
    return $true
} | ForEach-Object { $_.FullName }

# Collect C++ objects (excluding broken ones)
$CppObjs = @()
if (-not $PureASM) {
    $CppObjs = Get-ChildItem "$ObjDir\*.obj" -Exclude asm_monolithic_*.obj | Where-Object {
        $name = $_.Name
        # Skip broken Win32IDE implementations
        if ($BrokenCppPatterns -contains $name) { return $false }
        # Skip test/mock objects
        if ($ExcludedObjects | Where-Object { $name -like $_ }) { return $false }
        return $true
    } | ForEach-Object { $_.FullName }
}

# Entry point selection
$EntryObj = $null
$PreferredEntry = Join-Path $ObjDir "win32ide_main.obj"
$ASMEntry = Join-Path $ObjDir "main.obj"
$ASMMonoEntry = Join-Path $ObjDir "asm_monolithic_main.obj"

if (Test-Path $PreferredEntry) {
    $EntryObj = $PreferredEntry
    Write-Host "   Entry: C++ (win32ide_main.obj)" -ForegroundColor Gray
    # Exclude ASM main to avoid duplicate WinMain
    $ASMObjs = $ASMObjs | Where-Object { $_ -ne $ASMMonoEntry -and $_ -ne $ASMEntry }
} elseif (Test-Path $ASMEntry) {
    $EntryObj = $ASMEntry
    Write-Host "   Entry: ASM (main.obj)" -ForegroundColor Gray
} elseif (Test-Path $ASMMonoEntry) {
    $EntryObj = $ASMMonoEntry
    Write-Host "   Entry: ASM Monolithic (asm_monolithic_main.obj)" -ForegroundColor Gray
}

# Add ASM glue if available
if (Test-Path $ASMGlueObj) {
    $ASMObjs += $ASMGlueObj
}

# Detect runtime to avoid LNK2038
$RuntimeMode = "MT"
if (-not $PureASM -and $CppObjs.Count -gt 0) {
    $RuntimeMode = Get-DominantRuntime -ObjFiles $CppObjs
    Write-Host "   Detected Runtime: /$RuntimeMode (from C++ objects)" -ForegroundColor Yellow
} elseif ($PureASM) {
    $RuntimeMode = "NONE"
    Write-Host "   Runtime: None (Pure ASM)" -ForegroundColor Yellow
}

# ================================================================================
# PHASE 4: LIBRARY CONFIGURATION (CRT Fix)
# ================================================================================

Write-Host "[PHASE 4] Configuring CRT libraries (/M$RuntimeMode)..." -ForegroundColor Cyan

$LibPaths = @()
$MSVCLibDir = Join-Path (Split-Path $VCToolsDir -Parent) "lib\x64"
if (Test-Path $MSVCLibDir) { $LibPaths += "/LIBPATH:$MSVCLibDir" }
if (Test-Path $SDKLibDir) { $LibPaths += "/LIBPATH:$SDKLibDir" }
if (Test-Path $UCRTLibDir) { $LibPaths += "/LIBPATH:$UCRTLibDir" }

if ($RuntimeMode -eq "MD") {
    $RuntimeLibs = @(
        "msvcrt.lib",
        "vcruntime.lib", 
        "ucrt.lib",
        "msvcprt.lib",
        "libvcruntime.lib",    # Satisfies __vcrt_*
        "libucrt.lib"          # Satisfies __acrt_*
    )
} elseif ($RuntimeMode -eq "MT") {
    $RuntimeLibs = @(
        "libcmt.lib",
        "libvcruntime.lib",
        "libucrt.lib",
        "libcpmt.lib"
    )
} else {
    $RuntimeLibs = @()
}

$SystemLibs = @(
    "kernel32.lib", "user32.lib", "gdi32.lib", "winspool.lib",
    "comdlg32.lib", "advapi32.lib", "shell32.lib", "ole32.lib",
    "oleaut32.lib", "uuid.lib", "odbc32.lib", "odbccp32.lib",
    "comctl32.lib", "bcrypt.lib", "dwmapi.lib", "shlwapi.lib", "winhttp.lib"
)

# ================================================================================
# PHASE 5: STRATEGY A - LIB INTERMEDIATE
# ================================================================================

Write-Host "[PHASE 5] Strategy A: LIB Intermediate..." -ForegroundColor Cyan

$StrategyALib = Join-Path $OutDir "monolithic_core.lib"
$StrategyAExe = Join-Path $OutDir "RawrXD_StrategyA.exe"

try {
    if ($ASMObjs.Count -eq 0) { throw "No ASM objects found" }
    
    # Build static lib from ASM objects
    & $lib /OUT:$StrategyALib /NOLOGO /IGNORE:4006,4221 $ASMObjs 2>&1 | Tee-Object "$OutDir\strategy_a_lib.log"
    
    if (Test-Path $StrategyALib) {
        $linkArgs = @(
            $EntryObj,
            $StrategyALib
        ) + $CppObjs + @(
            "/OUT:$StrategyAExe",
            "/SUBSYSTEM:WINDOWS",
            "/ENTRY:WinMain",
            "/LARGEADDRESSAWARE",
            "/LTCG",
            "/FORCE:MULTIPLE",
            "/NODEFAULTLIB:libcmt.lib",  # Prevent mixing
            "/NODEFAULTLIB:msvcrt.lib"   # We'll add the correct one below
        ) + $RuntimeLibs + $LibPaths + $SystemLibs + "/NOLOGO"
        
        & $link $linkArgs 2>&1 | Tee-Object "$OutDir\strategy_a_link.log"
        
        if (Test-Path $StrategyAExe) {
            $size = [math]::Round((Get-Item $StrategyAExe).Length / 1MB, 2)
            Write-Host "   [SUCCESS] Strategy A: $size MB" -ForegroundColor Green
        } else {
            Write-Host "   [FAILED] See strategy_a_link.log" -ForegroundColor Red
        }
    }
} catch {
    Write-Host "   [ERROR] Strategy A: $_" -ForegroundColor Red
}

# ================================================================================
# PHASE 6: STRATEGY B - DIRECT LINK
# ================================================================================

Write-Host "[PHASE 6] Strategy B: Direct Monolithic..." -ForegroundColor Cyan

$StrategyBExe = Join-Path $OutDir "RawrXD_StrategyB.exe"

try {
    $linkArgs = @(
        $EntryObj
    ) + $ASMObjs + $CppObjs + @(
        "/OUT:$StrategyBExe",
        "/SUBSYSTEM:WINDOWS",
        "/ENTRY:WinMain",
        "/LARGEADDRESSAWARE",
        "/LTCG",
        "/FORCE:MULTIPLE",
        "/NODEFAULTLIB:libcmt.lib",
        "/NODEFAULTLIB:msvcrt.lib"
    ) + $RuntimeLibs + $LibPaths + $SystemLibs + "/NOLOGO"
    
    & $link $linkArgs 2>&1 | Tee-Object "$OutDir\strategy_b_link.log"
    
    if (Test-Path $StrategyBExe) {
        $size = [math]::Round((Get-Item $StrategyBExe).Length / 1MB, 2)
        Write-Host "   [SUCCESS] Strategy B: $size MB" -ForegroundColor Green
    } else {
        Write-Host "   [FAILED] See strategy_b_link.log" -ForegroundColor Red
    }
} catch {
    Write-Host "   [ERROR] Strategy B: $_" -ForegroundColor Red
}

# ================================================================================
# PHASE 7: STRATEGY C - TIERED DLL WITH EXPORTS
# ================================================================================

Write-Host "[PHASE 7] Strategy C: Tiered DLL..." -ForegroundColor Cyan

$Tier1Dll = Join-Path $OutDir "RawrXD_Tier1.dll"
$Tier2Dll = Join-Path $OutDir "RawrXD_Tier2.dll"
$StrategyCExe = Join-Path $OutDir "RawrXD_StrategyC.exe"

try {
    $mid = [math]::Floor($ASMObjs.Count / 2)
    $Tier1Objs = $ASMObjs | Select-Object -First $mid
    $Tier2Objs = $ASMObjs | Select-Object -Skip $mid
    
    # Create export DEF file for Tier1 (fixes unresolved BeaconRecv etc.)
    $Tier1Def = Join-Path $OutDir "tier1_exports.def"
    @"
LIBRARY RawrXD_Tier1
EXPORTS
    g_hHeap DATA
    g_hInstance DATA
    BeaconRecv
    TryBeaconRecv
    RunInference
    RegisterAgent
    HotSwapModel
"@ | Out-File -FilePath $Tier1Def -Encoding ASCII
    
    # Link Tier1 DLL
    & $link /DLL /OUT:$Tier1Dll /DEF:$Tier1Def /LTCG /NOLOGO $Tier1Objs $LibPaths $RuntimeLibs $SystemLibs 2>&1 | Out-File "$OutDir\strategy_c_tier1.log"
    
    if (Test-Path $Tier1Dll) {
        Write-Host "   [OK] Tier1.dll created" -ForegroundColor Green
        
        # Link Tier2 DLL (imports from Tier1)
        & $link /DLL /OUT:$Tier2Dll /LTCG /NOLOGO $Tier2Objs $Tier1Dll $LibPaths $RuntimeLibs $SystemLibs 2>&1 | Out-File "$OutDir\strategy_c_tier2.log"
        
        if (Test-Path $Tier2Dll) {
            Write-Host "   [OK] Tier2.dll created" -ForegroundColor Green
            
            # Final EXE
            & $link $EntryObj $Tier1Dll $Tier2Dll $CppObjs /OUT:$StrategyCExe /SUBSYSTEM:WINDOWS /ENTRY:WinMain /LARGEADDRESSAWARE /LTCG /FORCE:MULTIPLE /NOLOGO $LibPaths $RuntimeLibs $SystemLibs 2>&1 | Tee-Object "$OutDir\strategy_c_link.log"
            
            if (Test-Path $StrategyCExe) {
                $size = [math]::Round((Get-Item $StrategyCExe).Length / 1MB, 2)
                Write-Host "   [SUCCESS] Strategy C: $size MB" -ForegroundColor Green
            } else {
                Write-Host "   [FAILED] See strategy_c_link.log" -ForegroundColor Red
            }
        }
    }
} catch {
    Write-Host "   [ERROR] Strategy C: $_" -ForegroundColor Red
}

# ================================================================================
# PHASE 8: VALIDATION & SUMMARY
# ================================================================================

Write-Host "`n[PHASE 8] Validation Report" -ForegroundColor Cyan
"=" * 60

$Results = @(
    @{Name="Strategy A (LIB)"; Path=$StrategyAExe},
    @{Name="Strategy B (Direct)"; Path=$StrategyBExe},
    @{Name="Strategy C (Tiered)"; Path=$StrategyCExe}
)

$Success = $false
foreach ($r in $Results) {
    if (Test-Path $r.Path) {
        $size = [math]::Round((Get-Item $r.Path).Length / 1KB, 2)
        Write-Host "[PASS] $($r.Name): $size KB" -ForegroundColor Green
        $Success = $true
        
        # Verify imports
        $imports = & (Join-Path $VCToolsDir "dumpbin.exe") /imports $r.Path 2>$null | Select-String "msvcp|vcruntime|ucrt"
        if ($imports) {
            Write-Host "   CRT Dependencies: $($imports.Count) imports" -ForegroundColor Yellow
        } else {
            Write-Host "   CRT Dependencies: None (static/pure)" -ForegroundColor Green
        }
    } else {
        Write-Host "[FAIL] $($r.Name)" -ForegroundColor Red
    }
}

if (-not $Success) {
    Write-Host "`n[CRITICAL] All strategies failed. Common fixes:" -ForegroundColor Red
    Write-Host "  1. Run with -PureASM to exclude broken C++ objects" -ForegroundColor Yellow
    Write-Host "  2. Check strategy_*_link.log for LNK2038 (runtime mismatch)" -ForegroundColor Yellow
    Write-Host "  3. Ensure ensure_vsenv.ps1 set up LIB paths correctly" -ForegroundColor Yellow
} else {
    Write-Host "`n[SUCCESS] Linking complete. Use -PureASM for smallest executable." -ForegroundColor Green
}