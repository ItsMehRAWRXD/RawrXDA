# Export-RawrXD-FullDataset.ps1
# Reverse Engineering Dump — Complete IDE Structural Extraction
# No external deps. Pure PowerShell. Deterministic reconstruction.
# DEP FREE: No Qt, no external deps except Win32, MASM, C++20.

param(
    [string]$Root = "D:\rawrxd",
    [string]$Binary = "D:\rawrxd\build\RawrXD-Win32IDE.exe",
    [string]$OutDir = "D:\rawrxd\rawrxd_dataset",
    [switch]$FailOnRandom,
    [switch]$GenerateStubs
)

$ErrorActionPreference = "Continue"

function Write-Header($t) { Write-Host "`n=== $t ===" -ForegroundColor Cyan }

# ============================================================
# 1. DATASET CONTAINERS
# ============================================================
$Dataset = @{
    Meta = @{
        Timestamp    = (Get-Date -Format "yyyy-MM-ddTHH:mm:ss")
        Version      = "14.2.0"
        Root         = (Resolve-Path $Root).Path
        BinaryExists = Test-Path $Binary
        TotalFiles   = 0
        TotalLines   = 0
    }
    Topology = @{
        Files        = [System.Collections.ArrayList]::new()
        Headers      = [System.Collections.ArrayList]::new()
        Sources      = [System.Collections.ArrayList]::new()
        Assembly     = [System.Collections.ArrayList]::new()
        Resources    = [System.Collections.ArrayList]::new()
        BuildScripts = [System.Collections.ArrayList]::new()
        Dependencies = [System.Collections.ArrayList]::new()
    }
    Commands = @{
        TableEntries = [System.Collections.ArrayList]::new()
        Handlers     = [System.Collections.ArrayList]::new()
        Dispatchers  = [System.Collections.ArrayList]::new()
        KeyBindings  = [System.Collections.ArrayList]::new()
        MenuItems    = [System.Collections.ArrayList]::new()
    }
    Subsystems = @{
        Rendering = @{ Status = "Absent"; Details = [System.Collections.ArrayList]::new() }
        LLM       = @{ Status = "Absent"; Details = [System.Collections.ArrayList]::new() }
        LSP       = @{ Status = "Absent"; Details = [System.Collections.ArrayList]::new() }
        UI        = @{ Status = "Absent"; Details = [System.Collections.ArrayList]::new() }
        IO        = @{ Status = "Absent"; Details = [System.Collections.ArrayList]::new() }
        Crypto    = @{ Status = "Absent"; Details = [System.Collections.ArrayList]::new() }
        Vulkan    = @{ Status = "Absent"; Details = [System.Collections.ArrayList]::new() }
        MASM      = @{ Status = "Absent"; Details = [System.Collections.ArrayList]::new() }
        Streaming = @{ Status = "Absent"; Details = [System.Collections.ArrayList]::new() }
        Neural    = @{ Status = "Absent"; Details = [System.Collections.ArrayList]::new() }
    }
    EntryPoints = @{
        WinMain     = $null
        DllMain     = $null
        WinProc     = [System.Collections.ArrayList]::new()
        ThreadProcs = [System.Collections.ArrayList]::new()
    }
    Agents = @{
        Orchestrators = [System.Collections.ArrayList]::new()
        Workers       = [System.Collections.ArrayList]::new()
        Models        = [System.Collections.ArrayList]::new()
        Tools         = [System.Collections.ArrayList]::new()
        Hooks         = [System.Collections.ArrayList]::new()
    }
    AsmBridges = @{
        Exports  = [System.Collections.ArrayList]::new()
        Imports  = [System.Collections.ArrayList]::new()
        CallConv = [System.Collections.ArrayList]::new()
        Stubs    = [System.Collections.ArrayList]::new()
    }
    Random = @{
        UnmappedFunctions   = [System.Collections.ArrayList]::new()
        DeadCode            = [System.Collections.ArrayList]::new()
        OrphanSymbols       = [System.Collections.ArrayList]::new()
        UnreferencedGlobals = [System.Collections.ArrayList]::new()
        MysteryStructs      = [System.Collections.ArrayList]::new()
    }
}

# ============================================================
# 2. FILE INGESTION
# ============================================================
Write-Header "PHASE 1: FILE TOPOLOGY EXTRACTION"

$srcDir     = Join-Path $Root "src"
$incDir     = Join-Path $Root "include"
$testDir    = Join-Path $Root "test"

$AllFiles = [System.Collections.ArrayList]::new()
$SearchDirs = @($srcDir, $incDir, $testDir, $Root)
$Extensions = @("*.cpp","*.c","*.hpp","*.h","*.asm","*.inc","*.rc","*.ps1","*.cmake")

foreach ($dir in $SearchDirs) {
    if (!(Test-Path $dir)) { continue }
    foreach ($ext in $Extensions) {
        $found = Get-ChildItem -Path $dir -Filter $ext -ErrorAction SilentlyContinue
        foreach ($f in $found) { [void]$AllFiles.Add($f) }
    }
}
# Also grab CMakeLists.txt at root
$cmakeRoot = Join-Path $Root "CMakeLists.txt"
if (Test-Path $cmakeRoot) {
    [void]$AllFiles.Add((Get-Item $cmakeRoot))
}

# Deduplicate by FullName
$AllFiles = $AllFiles | Sort-Object FullName -Unique

$Dataset.Meta.TotalFiles = $AllFiles.Count

# Build per-file metadata & accumulate all source text
$AllContentParts = [System.Collections.ArrayList]::new()

foreach ($file in $AllFiles) {
    $raw = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
    if (-not $raw) { $raw = "" }
    $lines = ($raw -split "`n").Count
    $Dataset.Meta.TotalLines += $lines

    $fileObj = @{
        Path  = $file.FullName.Replace($Root, ".")
        Size  = $file.Length
        Lines = $lines
        Type  = $file.Extension
        Hash  = (Get-FileHash $file.FullName -Algorithm SHA256).Hash.Substring(0,16)
    }

    switch -Regex ($file.Extension) {
        "\.(h|hpp|inc)$" { [void]$Dataset.Topology.Headers.Add($fileObj) }
        "\.(c|cpp)$"     { [void]$Dataset.Topology.Sources.Add($fileObj) }
        "\.asm$"         { [void]$Dataset.Topology.Assembly.Add($fileObj) }
        "\.rc$"          { [void]$Dataset.Topology.Resources.Add($fileObj) }
        "\.(ps1|cmake|txt)$" { [void]$Dataset.Topology.BuildScripts.Add($fileObj) }
    }

    [void]$Dataset.Topology.Files.Add($fileObj)

    if ($file.Extension -match "\.(cpp|c|h|hpp|asm|inc)$") {
        [void]$AllContentParts.Add($raw)
    }
}

$AllContent = $AllContentParts -join "`n"

Write-Host "Indexed $($Dataset.Meta.TotalFiles) files, $($Dataset.Meta.TotalLines) lines"

# ============================================================
# 3. SYMBOL EXTRACTION
# ============================================================
Write-Header "PHASE 2: SYMBOL EXTRACTION"

# WinMain
if ($AllContent -match "int\s+WINAPI\s+WinMain\s*\(") {
    $Dataset.EntryPoints.WinMain = @{ Found = $true; Signature = "WINAPI WinMain" }
}
if ($AllContent -match "int\s+WINAPI\s+wWinMain\s*\(") {
    $Dataset.EntryPoints.WinMain = @{ Found = $true; Signature = "WINAPI wWinMain" }
}
# DllMain
if ($AllContent -match "BOOL\s+(APIENTRY|WINAPI)\s+DllMain\s*\(") {
    $Dataset.EntryPoints.DllMain = @{ Found = $true; Signature = "DllMain" }
}

# Window Procedures
$procMatches = [regex]::Matches($AllContent, "LRESULT\s+(CALLBACK\s+)?(\w+)\s*\(\s*HWND")
foreach ($m in $procMatches) {
    [void]$Dataset.EntryPoints.WinProc.Add(@{
        Name = $m.Groups[2].Value
        Type = "WindowProc"
    })
}

# Thread entry points
$threadMatches = [regex]::Matches($AllContent, "DWORD\s+WINAPI\s+(\w+)\s*\(\s*LPVOID")
foreach ($m in $threadMatches) {
    [void]$Dataset.EntryPoints.ThreadProcs.Add(@{
        Name = $m.Groups[1].Value
        Type = "ThreadProc"
    })
}

Write-Host "WinMain: $(if($Dataset.EntryPoints.WinMain){'Found'}else{'MISSING'})"
Write-Host "WinProc: $($Dataset.EntryPoints.WinProc.Count) found"
Write-Host "ThreadProcs: $($Dataset.EntryPoints.ThreadProcs.Count) found"

# ============================================================
# 4. COMMAND TABLE EXTRACTION
# ============================================================
Write-Header "PHASE 3: COMMAND SYSTEM ANALYSIS"

# Command constants
$cmdConsts = [regex]::Matches($AllContent, "#define\s+(CMD_\w+|ID_\w+|IDM_\w+)\s+(\d+)")
foreach ($m in $cmdConsts) {
    [void]$Dataset.Commands.TableEntries.Add(@{
        Name  = $m.Groups[1].Value
        Value = $m.Groups[2].Value
        Type  = "Constant"
    })
}

# Command table structs
$tableMatches = [regex]::Matches($AllContent, "(?:COMMAND_TABLE|CommandEntry|DispatchEntry)\s*\w*\s*\[\s*\]")
foreach ($m in $tableMatches) {
    [void]$Dataset.Commands.Dispatchers.Add(@{
        Match = $m.Value
        Type  = "TableDecl"
    })
}

# Handler functions (handle*, On*, Cmd*)
$handlerMatches = [regex]::Matches($AllContent, "(?:void|LRESULT|bool|int)\s+(handle\w+|On\w+|Cmd\w+|cmd_\w+)\s*\(")
$knownHandlers = [System.Collections.Generic.HashSet[string]]::new()
foreach ($h in $handlerMatches) {
    $name = $h.Groups[1].Value
    if ($knownHandlers.Add($name)) {
        [void]$Dataset.Commands.Handlers.Add(@{
            Name = $name
            Type = "CommandHandler"
        })
    }
}

# WM_ message handling
$wmMessages = [regex]::Matches($AllContent, "case\s+(WM_\w+)\s*:")
$wmSet = [System.Collections.Generic.HashSet[string]]::new()
foreach ($m in $wmMessages) { [void]$wmSet.Add($m.Groups[1].Value) }

$criticalWM = @("WM_CREATE","WM_PAINT","WM_DESTROY","WM_COMMAND","WM_SIZE","WM_CLOSE")
foreach ($wm in $criticalWM) {
    $status = if ($wmSet.Contains($wm)) { "Present" } else { "MISSING" }
    [void]$Dataset.Commands.KeyBindings.Add(@{ Message = $wm; Status = $status })
}

Write-Host "Command constants: $($Dataset.Commands.TableEntries.Count)"
Write-Host "Handlers: $($Dataset.Commands.Handlers.Count)"
Write-Host "WM messages handled: $($wmSet.Count)"

# ============================================================
# 5. SUBSYSTEM DETECTION
# ============================================================
Write-Header "PHASE 4: SUBSYSTEM INVENTORY"

# Rendering
if ($AllContent -match "D2D1CreateFactory|ID2D1Factory|Direct2D") {
    $Dataset.Subsystems.Rendering.Status = "Direct2D"
    [void]$Dataset.Subsystems.Rendering.Details.Add("Direct2D Factory detected")
}
if ($AllContent -match "BeginPaint|GetDC|BitBlt|FillRect|DrawText") {
    if ($Dataset.Subsystems.Rendering.Status -eq "Absent") {
        $Dataset.Subsystems.Rendering.Status = "GDI"
    }
    [void]$Dataset.Subsystems.Rendering.Details.Add("GDI primitives detected")
}

# Vulkan
if ($AllContent -match "vkCreateInstance|VkDevice|VK_SUCCESS|vulkan\.h") {
    $Dataset.Subsystems.Vulkan.Status = "Present"
    [void]$Dataset.Subsystems.Vulkan.Details.Add("Vulkan Compute detected")
}

# LLM/AI
if ($AllContent -match "GGUF|llama_|Inference|CPUInferenceEngine|ModelCaller") {
    $Dataset.Subsystems.LLM.Status = "Integrated"
    [void]$Dataset.Subsystems.LLM.Details.Add("Local LLM inference detected")
}

# LSP
if ($AllContent -match "LanguageServer|LSP|textDocument/|CompletionEngine") {
    $Dataset.Subsystems.LSP.Status = "Client"
    [void]$Dataset.Subsystems.LSP.Details.Add("LSP-style features detected")
}

# UI
if ($AllContent -match "CreateWindowEx|RegisterClassEx|ShowWindow|UpdateWindow") {
    $Dataset.Subsystems.UI.Status = "Win32"
    [void]$Dataset.Subsystems.UI.Details.Add("Win32 UI framework detected")
}

# IO / FileSystem
if ($AllContent -match "CreateFile[AW]|ReadFile|WriteFile|MapViewOfFile") {
    $Dataset.Subsystems.IO.Status = "Win32IO"
    [void]$Dataset.Subsystems.IO.Details.Add("Win32 file I/O detected")
}

# Crypto
if ($AllContent -match "AES|SHA256|BCrypt|carmilla|encrypt|decrypt") {
    $Dataset.Subsystems.Crypto.Status = "Integrated"
    [void]$Dataset.Subsystems.Crypto.Details.Add("Cryptographic routines detected")
}

# MASM
if ($AllContent -match "\.CODE|PROC\s+FRAME|PUBLIC\s+\w+|EXTERN\s+\w+") {
    $Dataset.Subsystems.MASM.Status = "Active"
    [void]$Dataset.Subsystems.MASM.Details.Add("MASM x64 assembly kernels active")
}

# Streaming
if ($AllContent -match "StreamingGGUF|RingBuffer|Titan_SubmitChunk|g_RingBase") {
    $Dataset.Subsystems.Streaming.Status = "Present"
    [void]$Dataset.Subsystems.Streaming.Details.Add("Token streaming pipeline detected")
}

# Neural
if ($AllContent -match "RawrXD_Tokenize|RawrXD_Inference|RawrXD_Detokenize|neural_core") {
    $Dataset.Subsystems.Neural.Status = "Active"
    [void]$Dataset.Subsystems.Neural.Details.Add("MASM neural inference core detected")
}

foreach ($key in $Dataset.Subsystems.Keys) {
    $s = $Dataset.Subsystems[$key]
    $color = if ($s.Status -eq "Absent") { "Red" } else { "Green" }
    Write-Host "  $($key): $($s.Status)" -ForegroundColor $color
}

# ============================================================
# 6. AGENT SYSTEM EXTRACTION
# ============================================================
Write-Header "PHASE 5: AGENTIC SYSTEM MAPPING"

$agentPatterns = @{
    Orchestrators = "Orchestrator|AgentHost|SwarmController|AutonomousFeature"
    Workers       = "AgentWorker|TaskExecutor|InferenceWorker|BackgroundWorker"
    Models        = "ModelConnection|GGUFLoader|Tokenizer|ModelCaller|CPUInferenceEngine"
    Tools         = "ToolCall|FunctionCall|Capability|PatternBridge|PipeClient"
    Hooks         = "HookProc|Interceptor|Detour|CorrectionSystem"
}

foreach ($cat in $agentPatterns.Keys) {
    $pattern = $agentPatterns[$cat]
    $matches = [regex]::Matches($AllContent, "(?:class|struct)\s+(\w*(?:$pattern)\w*)")
    foreach ($m in $matches) {
        [void]$Dataset.Agents.$cat.Add(@{
            Name = $m.Groups[1].Value
            Type = $cat
        })
    }
}

$totalAgents = 0
foreach ($cat in $agentPatterns.Keys) { $totalAgents += $Dataset.Agents.$cat.Count }
Write-Host "Total agent components: $totalAgents"

# ============================================================
# 7. MASM BRIDGE EXTRACTION
# ============================================================
Write-Header "PHASE 6: MASM BRIDGE ANALYSIS"

$asmFiles = $AllFiles | Where-Object { $_.Extension -eq ".asm" }
foreach ($asmFile in $asmFiles) {
    $content = Get-Content $asmFile.FullName -Raw -ErrorAction SilentlyContinue
    if (-not $content) { continue }

    # PUBLIC exports
    $exports = [regex]::Matches($content, "PUBLIC\s+(\w+)")
    foreach ($e in $exports) {
        [void]$Dataset.AsmBridges.Exports.Add(@{
            Name   = $e.Groups[1].Value
            Source = $asmFile.Name
            Type   = "AssemblyExport"
        })
    }

    # EXTERN imports
    $imports = [regex]::Matches($content, "(?:EXTERN|EXTRN)\s+(\w+)")
    foreach ($i in $imports) {
        [void]$Dataset.AsmBridges.Imports.Add(@{
            Name   = $i.Groups[1].Value
            Source = $asmFile.Name
            Type   = "AssemblyImport"
        })
    }

    # Procedure definitions
    $procs = [regex]::Matches($content, "(\w+)\s+PROC(?:\s+FRAME)?")
    foreach ($p in $procs) {
        [void]$Dataset.AsmBridges.CallConv.Add(@{
            Name       = $p.Groups[1].Value
            Convention = "MASM64/x64"
            Source     = $asmFile.Name
        })
    }
}

# C++ extern "C" declarations that reference ASM
$externC = [regex]::Matches($AllContent, 'extern\s+"C"\s+(?:void|int|uint64_t|float\*?|bool)\s+(\w+)\s*\(')
foreach ($e in $externC) {
    [void]$Dataset.AsmBridges.Stubs.Add(@{
        Name = $e.Groups[1].Value
        Type = "CppExternC_Decl"
    })
}

Write-Host "ASM Exports: $($Dataset.AsmBridges.Exports.Count)"
Write-Host "ASM Imports: $($Dataset.AsmBridges.Imports.Count)"
Write-Host "ASM Procs:   $($Dataset.AsmBridges.CallConv.Count)"
Write-Host "C++ extern C: $($Dataset.AsmBridges.Stubs.Count)"

# ============================================================
# 8. RANDOM / ENTROPY CATCHER (Efficient single-pass)
# ============================================================
Write-Header "PHASE 7: ENTROPY ANALYSIS (RANDOM BUCKET)"

# Build a global word-frequency table in ONE pass (no per-symbol regex)
Write-Host "Building word frequency index (per-file)..."
$wordFreq = [System.Collections.Generic.Dictionary[string,int]]::new()
$wordRx = [regex]::new('\b([A-Za-z_]\w{2,})\b', 'Compiled')

# Process each file individually instead of one giant string
foreach ($part in $AllContentParts) {
    if ([string]::IsNullOrEmpty($part)) { continue }
    # Split into lines to avoid regex on massive single strings
    foreach ($line in ($part -split "`n")) {
        $lineMatches = $wordRx.Matches($line)
        foreach ($wm in $lineMatches) {
            $w = $wm.Groups[1].Value
            if ($wordFreq.ContainsKey($w)) { $wordFreq[$w]++ } else { $wordFreq[$w] = 1 }
        }
    }
}
Write-Host "Indexed $($wordFreq.Count) unique identifiers"

# All function definitions
$allFunctions = [System.Collections.Generic.HashSet[string]]::new()
$fnRx = [regex]::new('(?:void|int|bool|BOOL|LRESULT|HRESULT|size_t|float|double|uint32_t|int64_t|std::string)\s+(\w+)\s*\(', 'Compiled')
$fnMatches = $fnRx.Matches($AllContent)
foreach ($m in $fnMatches) { [void]$allFunctions.Add($m.Groups[1].Value) }

# Exclusion sets
$wellKnown = [System.Collections.Generic.HashSet[string]]::new(
    [string[]]@("main","WinMain","wWinMain","DllMain","wmain","_tmain","sizeof","memcpy",
                "memset","malloc","free","printf","fprintf","strlen","strcmp","strcpy",
                "calloc","realloc","abort","exit","return","break","continue","switch",
                "static_cast","dynamic_cast","reinterpret_cast","const_cast")
)

$handlerNames = [System.Collections.Generic.HashSet[string]]::new()
foreach ($h in $Dataset.Commands.Handlers) { [void]$handlerNames.Add($h.Name) }
foreach ($p in $Dataset.EntryPoints.WinProc) { [void]$handlerNames.Add($p.Name) }
foreach ($t in $Dataset.EntryPoints.ThreadProcs) { [void]$handlerNames.Add($t.Name) }

# Check function usage via pre-built word frequency (O(1) per lookup)
foreach ($fn in $allFunctions) {
    if ($wellKnown.Contains($fn)) { continue }
    if ($handlerNames.Contains($fn)) { continue }
    if ($fn.Length -le 3) { continue }

    $freq = 0
    if ($wordFreq.ContainsKey($fn)) { $freq = $wordFreq[$fn] }
    if ($freq -le 1) {
        [void]$Dataset.Random.UnmappedFunctions.Add(@{
            Name   = $fn
            Reason = "Defined once, no external calls detected"
            Orphan = $true
        })
    }
}

# Unreferenced ASM exports — check via word frequency (O(1) per export)
foreach ($exp in $Dataset.AsmBridges.Exports) {
    $name = $exp.Name
    # An ASM export is "orphan" if it appears only in .asm files (freq in AllContent
    # minus asm occurrences <= 0). Simpler: check if name appears in cpp content.
    # Use word freq: if total freq == asm-only occurrences, it's orphaned from C++.
    $freq = 0
    if ($wordFreq.ContainsKey($name)) { $freq = $wordFreq[$name] }
    # If the symbol appears <= 2 times total (PUBLIC decl + PROC decl), no C++ refs
    if ($freq -le 2) {
        [void]$Dataset.Random.OrphanSymbols.Add(@{
            Name   = $name
            Type   = "ASMExport_NoC++Ref"
            Source = $exp.Source
        })
    }
}

# Mystery structs — via word frequency
$structRx = [regex]::new('(?:struct|class)\s+(\w{3,})\s*[{:]', 'Compiled')
$structMatches = $structRx.Matches($AllContent)
foreach ($s in $structMatches) {
    $name = $s.Groups[1].Value
    $freq = 0
    if ($wordFreq.ContainsKey($name)) { $freq = $wordFreq[$name] }
    if ($freq -le 1) {
        [void]$Dataset.Random.MysteryStructs.Add(@{
            Name   = $name
            Reason = "Defined but never instantiated"
        })
    }
}

$entropyScore = $Dataset.Random.UnmappedFunctions.Count +
                $Dataset.Random.OrphanSymbols.Count +
                $Dataset.Random.MysteryStructs.Count

Write-Host "Unmapped Functions: $($Dataset.Random.UnmappedFunctions.Count)" -ForegroundColor $(if($Dataset.Random.UnmappedFunctions.Count -eq 0){"Green"}else{"Yellow"})
Write-Host "Orphan ASM Exports: $($Dataset.Random.OrphanSymbols.Count)" -ForegroundColor $(if($Dataset.Random.OrphanSymbols.Count -eq 0){"Green"}else{"Yellow"})
Write-Host "Mystery Structs:    $($Dataset.Random.MysteryStructs.Count)" -ForegroundColor $(if($Dataset.Random.MysteryStructs.Count -eq 0){"Green"}else{"Yellow"})
Write-Host "Total Entropy:      $entropyScore" -ForegroundColor $(if($entropyScore -eq 0){"Green"}else{"Red"})

# ============================================================
# 9. PE BINARY ANALYSIS
# ============================================================
Write-Header "PHASE 8: BINARY FORENSICS"

if (Test-Path $Binary) {
    try {
        $peBytes = [System.IO.File]::ReadAllBytes($Binary)
        $isPE = ($peBytes[0] -eq 0x4D -and $peBytes[1] -eq 0x5A)

        $Dataset.Meta.BinaryAnalysis = @{
            Size     = $peBytes.Length
            SizeKB   = [math]::Round($peBytes.Length / 1024, 1)
            SizeMB   = [math]::Round($peBytes.Length / 1048576, 2)
            IsPE     = $isPE
            MZHeader = [System.BitConverter]::ToString($peBytes[0..1])
        }

        if ($isPE) {
            # PE offset at 0x3C
            $peOff = [System.BitConverter]::ToInt32($peBytes, 0x3C)
            $peSig = [System.BitConverter]::ToString($peBytes[$peOff..($peOff+3)])
            $Dataset.Meta.BinaryAnalysis.PESignature = $peSig

            # Machine type at PE+4
            $machine = [System.BitConverter]::ToUInt16($peBytes, $peOff + 4)
            $machineStr = switch ($machine) {
                0x8664 { "x64 (AMD64)" }
                0x14C  { "x86 (i386)" }
                0xAA64 { "ARM64" }
                default { "Unknown (0x$($machine.ToString('X4')))" }
            }
            $Dataset.Meta.BinaryAnalysis.Machine = $machineStr

            # Number of sections
            $numSections = [System.BitConverter]::ToUInt16($peBytes, $peOff + 6)
            $Dataset.Meta.BinaryAnalysis.NumSections = $numSections

            Write-Host "PE: $machineStr, $numSections sections, $($Dataset.Meta.BinaryAnalysis.SizeMB) MB"
        }
    } catch {
        Write-Warning "Binary analysis failed: $_"
    }
} else {
    Write-Host "No binary found at $Binary" -ForegroundColor Yellow
}

# ============================================================
# 10. DATASET SERIALIZATION
# ============================================================
Write-Header "PHASE 9: DATASET SERIALIZATION"

if (!(Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

$Dataset.Topology   | ConvertTo-Json -Depth 10 -Compress:$false | Out-File "$OutDir\topology.json" -Encoding utf8
$Dataset.Commands   | ConvertTo-Json -Depth 10 -Compress:$false | Out-File "$OutDir\commands.json" -Encoding utf8
$Dataset.Subsystems | ConvertTo-Json -Depth 10 -Compress:$false | Out-File "$OutDir\subsystems.json" -Encoding utf8
$Dataset.EntryPoints| ConvertTo-Json -Depth 10 -Compress:$false | Out-File "$OutDir\entrypoints.json" -Encoding utf8
$Dataset.Agents     | ConvertTo-Json -Depth 10 -Compress:$false | Out-File "$OutDir\agents.json" -Encoding utf8
$Dataset.AsmBridges | ConvertTo-Json -Depth 10 -Compress:$false | Out-File "$OutDir\asm_bridges.json" -Encoding utf8
$Dataset.Random     | ConvertTo-Json -Depth 10 -Compress:$false | Out-File "$OutDir\random.json" -Encoding utf8
$Dataset            | ConvertTo-Json -Depth 10 -Compress:$false | Out-File "$OutDir\complete_manifest.json" -Encoding utf8

Write-Host "8 JSON files written to $OutDir"

# ============================================================
# 11. REBUILD INSTRUCTIONS
# ============================================================
Write-Header "PHASE 10: REBUILD BLUEPRINT"

$wmStatus = @{}
foreach ($kb in $Dataset.Commands.KeyBindings) { $wmStatus[$kb.Message] = $kb.Status }

$rebuildMd = @"
# RawrXD Deterministic Rebuild Instructions
Generated: $($Dataset.Meta.Timestamp)
Version: $($Dataset.Meta.Version)
Files: $($Dataset.Meta.TotalFiles) | Lines: $($Dataset.Meta.TotalLines)

## Boot Sequence (White-Screen Prevention Checklist)

### 1. Entry Points
- WinMain: $(if($Dataset.EntryPoints.WinMain){"PRESENT"}else{"**MISSING**"})
- WindowProc count: $($Dataset.EntryPoints.WinProc.Count)
- Thread procs: $($Dataset.EntryPoints.ThreadProcs.Count)

### 2. Critical Win32 Messages
$(foreach ($wm in $criticalWM) {
    $st = if ($wmStatus[$wm]) { $wmStatus[$wm] } else { "Unknown" }
    "- $wm : $st"
})

### 3. Rendering Subsystem
Status: $($Dataset.Subsystems.Rendering.Status)
$(if ($Dataset.Subsystems.Rendering.Status -eq "Absent") {
    "**CRITICAL**: No paint handler. Window will be all-white."
})

### 4. Command Dispatch
Handlers registered: $($Dataset.Commands.Handlers.Count)
Table entries: $($Dataset.Commands.TableEntries.Count)
$(if ($Dataset.Commands.Handlers.Count -eq 0) {
    "**CRITICAL**: No command handlers = dead UI."
})

### 5. Subsystem Status
| Subsystem | Status |
|-----------|--------|
$(foreach ($key in ($Dataset.Subsystems.Keys | Sort-Object)) {
    "| $key | $($Dataset.Subsystems[$key].Status) |"
})

### 6. MASM Bridges
- ASM Exports: $($Dataset.AsmBridges.Exports.Count)
- ASM Imports: $($Dataset.AsmBridges.Imports.Count)
- ASM Procedures: $($Dataset.AsmBridges.CallConv.Count)
- C++ extern "C" decls: $($Dataset.AsmBridges.Stubs.Count)

### 7. Agent System
$(foreach ($cat in $agentPatterns.Keys) {
    "- $cat : $($Dataset.Agents.$cat.Count)"
})

### 8. Entropy / Random Bucket
- Unmapped Functions: $($Dataset.Random.UnmappedFunctions.Count)
- Orphan ASM Exports: $($Dataset.Random.OrphanSymbols.Count)
- Mystery Structs: $($Dataset.Random.MysteryStructs.Count)
- **Total Entropy: $entropyScore**

> **Rule**: ``random.json`` must be empty before release.
> If it is not empty, the build has undefined behavior.
"@

$rebuildMd | Out-File "$OutDir\rebuild_instructions.md" -Encoding utf8

# ============================================================
# 12. DETERMINISTIC REBUILD SCRIPT
# ============================================================

$rebuildScript = @'
# Deterministic-RawrXD-Rebuild.ps1
# Auto-generated preflight check before build

param(
    [string]$DatasetDir = ".\rawrxd_dataset",
    [switch]$StrictMode
)

$manifest = Get-Content "$DatasetDir\complete_manifest.json" -Raw | ConvertFrom-Json
$random   = Get-Content "$DatasetDir\random.json" -Raw | ConvertFrom-Json

$entropyCount = 0
if ($random.UnmappedFunctions)   { $entropyCount += $random.UnmappedFunctions.Count }
if ($random.OrphanSymbols)       { $entropyCount += $random.OrphanSymbols.Count }
if ($random.MysteryStructs)      { $entropyCount += $random.MysteryStructs.Count }

Write-Host "=== RawrXD Pre-Build Validation ===" -ForegroundColor Cyan
Write-Host "Files: $($manifest.Meta.TotalFiles)"
Write-Host "Lines: $($manifest.Meta.TotalLines)"
Write-Host "Entropy Score: $entropyCount"

if ($entropyCount -gt 0) {
    Write-Warning "RANDOM BUCKET NOT EMPTY ($entropyCount items)."
    if ($StrictMode) {
        Write-Error "StrictMode: Refusing non-deterministic build."
        exit 1
    }
}

# Validate critical messages
$cmds = Get-Content "$DatasetDir\commands.json" -Raw | ConvertFrom-Json
foreach ($kb in $cmds.KeyBindings) {
    if ($kb.Status -eq "MISSING") {
        Write-Warning "Critical message handler MISSING: $($kb.Message)"
    }
}

# Validate subsystems
$subs = Get-Content "$DatasetDir\subsystems.json" -Raw | ConvertFrom-Json
if ($subs.Rendering.Status -eq "Absent") {
    Write-Error "CRITICAL: No rendering subsystem. IDE will show white screen."
}

Write-Host "`nPreflight complete." -ForegroundColor Green
'@

$rebuildScript | Out-File "$OutDir\deterministic_rebuild.ps1" -Encoding utf8

# ============================================================
# 13. ONE-LINER HOTPATCH GENERATOR
# ============================================================

$patches = [System.Collections.ArrayList]::new()

if (-not ($wmSet.Contains("WM_PAINT"))) {
    [void]$patches.Add('# Inject WM_PAINT fallback into first WndProc file found:')
    [void]$patches.Add('$f = Get-ChildItem D:\rawrxd\src -Filter *.cpp | Where-Object { Select-String -Path $_.FullName -Pattern "WndProc" -Quiet } | Select-Object -First 1; if ($f) { (Get-Content $f.FullName) -replace "(case WM_DESTROY:)", "case WM_PAINT:{PAINTSTRUCT ps;HDC hdc=BeginPaint(hwnd,&ps);RECT r;GetClientRect(hwnd,&r);FillRect(hdc,&r,(HBRUSH)GetStockObject(WHITE_BRUSH));SetBkMode(hdc,TRANSPARENT);DrawTextA(hdc,`"RawrXD IDE`",-1,&r,0x25);EndPaint(hwnd,&ps);return 0;}`n`$1" | Set-Content $f.FullName }')
}

if ($Dataset.Random.OrphanSymbols.Count -gt 0) {
    [void]$patches.Add("")
    [void]$patches.Add("# Orphan ASM exports needing C++ extern declarations:")
    foreach ($sym in $Dataset.Random.OrphanSymbols) {
        [void]$patches.Add("# Missing: extern `"C`" void $($sym.Name)(...); // from $($sym.Source)")
    }
}

$patches | Out-File "$OutDir\one_liner_patches.ps1" -Encoding utf8

# ============================================================
# 14. FINAL REPORT
# ============================================================
Write-Header "DATASET COMPLETE"

$report = @"
RAWRXD STRUCTURAL DATASET v14.2
================================
Files:      $($Dataset.Meta.TotalFiles)
Lines:      $($Dataset.Meta.TotalLines)
Entropy:    $entropyScore (Target: 0)

Subsystems:
$(foreach ($key in ($Dataset.Subsystems.Keys | Sort-Object)) {
    "  $($key.PadRight(12)): $($Dataset.Subsystems[$key].Status)"
})

Bridges:
  ASM Exports:   $($Dataset.AsmBridges.Exports.Count)
  ASM Imports:   $($Dataset.AsmBridges.Imports.Count)
  ASM Procs:     $($Dataset.AsmBridges.CallConv.Count)
  C++ extern C:  $($Dataset.AsmBridges.Stubs.Count)

Random Bucket:
  Unmapped Funcs:  $($Dataset.Random.UnmappedFunctions.Count)
  Orphan Symbols:  $($Dataset.Random.OrphanSymbols.Count)
  Mystery Structs: $($Dataset.Random.MysteryStructs.Count)

Outputs:
  $OutDir\topology.json
  $OutDir\commands.json
  $OutDir\subsystems.json
  $OutDir\entrypoints.json
  $OutDir\agents.json
  $OutDir\asm_bridges.json
  $OutDir\random.json           <-- MUST BE EMPTY FOR RELEASE
  $OutDir\complete_manifest.json
  $OutDir\rebuild_instructions.md
  $OutDir\deterministic_rebuild.ps1
  $OutDir\one_liner_patches.ps1
  $OutDir\DATASET_REPORT.txt
"@

$report | Out-File "$OutDir\DATASET_REPORT.txt" -Encoding utf8
Write-Host $report -ForegroundColor $(if($entropyScore -eq 0){"Green"}else{"Yellow"})

if ($FailOnRandom -and $entropyScore -gt 0) {
    Write-Error "FAILURE: Random bucket contains $entropyScore unclassified items."
    exit 1
}

Write-Host "`nDataset extraction complete. RawrXD is structurally known." -ForegroundColor Green
