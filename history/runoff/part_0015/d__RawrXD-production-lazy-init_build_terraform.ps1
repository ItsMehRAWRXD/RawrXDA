# =============================================================================
# RXUC Process Agent Engine — Build Orchestrator
# Reverse-engineered engine that creates process agents, adjusts limits,
# temp-patches each compile to each source
# =============================================================================

param(
    [string]$AssemblerPath = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe",
    [string]$LinkerPath = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe",
    [string]$SourceFile = "terraform.asm",
    [string]$OutputFile = "terraform.exe",
    [string]$InputSource = "",           # The .tf source file to compile
    [switch]$BuildEngine,                # Build the engine_core instead
    [switch]$FullPipeline,               # Build engine + run on source
    [string[]]$SourceFiles = @()         # Multiple source files
)

# Function to adjust limits in terraform.asm
function Adjust-Limits {
    param([long]$SourceSize)

    # Calculate dynamic limits
    $maxSrc = [math]::Max($SourceSize * 2, 1048576)  # At least 1MB
    $maxTok = [math]::Max($SourceSize / 10, 16384)   # Estimate tokens
    $maxSym = [math]::Max($maxTok / 4, 4096)         # Estimate symbols
    $maxCode = [math]::Max($SourceSize * 4, 1048576) # Code buffer
    $maxData = [math]::Max($SourceSize * 2, 65536)   # Data buffer

    # Backup original
    Copy-Item $SourceFile "$SourceFile.backup" -Force

    # Read content
    $content = Get-Content $SourceFile -Raw

    # Replace limits
    $content = $content -replace 'MAX_SRC equ \S+', "MAX_SRC equ $([math]::Floor($maxSrc))"
    $content = $content -replace 'MAX_TOK equ \S+', "MAX_TOK equ $([math]::Floor($maxTok))"
    $content = $content -replace 'MAX_SYM equ \S+', "MAX_SYM equ $([math]::Floor($maxSym))"
    $content = $content -replace 'MAX_CODE equ \S+', "MAX_CODE equ $([math]::Floor($maxCode))"
    $content = $content -replace 'MAX_DATA equ \S+', "MAX_DATA equ $([math]::Floor($maxData))"

    # Write back
    Set-Content $SourceFile $content

    Write-Host "  Limits adjusted: MAX_SRC=$maxSrc, MAX_TOK=$maxTok, MAX_SYM=$maxSym" -ForegroundColor Cyan
}

# Function to restore original
function Restore-Original {
    if (Test-Path "$SourceFile.backup") {
        Move-Item "$SourceFile.backup" $SourceFile -Force
    }
}

# ============================================================
# Source Complexity Analyzer (PowerShell-side pre-scan)
# Mirrors the MASM digest_source for orchestration decisions
# ============================================================
function Analyze-SourceComplexity {
    param([string]$Path)

    $content = Get-Content $Path -Raw -ErrorAction SilentlyContinue
    if (-not $content) { return $null }

    $result = [PSCustomObject]@{
        FileSize       = (Get-Item $Path).Length
        FnCount        = ([regex]::Matches($content, '\bfn\b')).Count
        WhileCount     = ([regex]::Matches($content, '\bwhile\b')).Count
        IfCount        = ([regex]::Matches($content, '\bif\b')).Count
        LetCount       = ([regex]::Matches($content, '\blet\b')).Count
        ReturnCount    = ([regex]::Matches($content, '\breturn\b')).Count
        AllocCount     = ([regex]::Matches($content, '\balloc\b')).Count
        ThreadCount    = ([regex]::Matches($content, '\b(thread|spawn)\b')).Count
        StructCount    = ([regex]::Matches($content, '\bstruct\b')).Count
        MaxBraceDepth  = 0
        Signals        = @()
        Complexity     = 0
        StackReserve   = 0x100000
        HeapReserve    = 0x100000
        WorkingSet     = 0
    }

    # Calculate brace depth
    $depth = 0; $maxDepth = 0
    foreach ($char in $content.ToCharArray()) {
        if ($char -eq '{') { $depth++; if ($depth -gt $maxDepth) { $maxDepth = $depth } }
        if ($char -eq '}') { $depth-- }
    }
    $result.MaxBraceDepth = $maxDepth

    # Complexity score
    $result.Complexity = ($result.FnCount * 10) + ($result.WhileCount * 20) +
                         ($result.IfCount * 5) + ($result.LetCount * 3) +
                         ($result.ReturnCount * 15) + ($result.AllocCount * 25) +
                         ($result.ThreadCount * 30) + ($result.MaxBraceDepth * 40) +
                         [math]::Floor($result.FileSize / 256)

    # Signal detection
    if ($result.FnCount -gt 0 -and ($result.ReturnCount / [math]::Max($result.FnCount, 1)) -gt 3) {
        $result.Signals += "SIG_RECURSE"
        $result.StackReserve = [math]::Max($result.StackReserve, 0xA00000) # 10MB
    }
    if ($result.FileSize -gt 0x10000) {
        $result.Signals += "SIG_BIGDATA"
        $result.HeapReserve = [math]::Max($result.HeapReserve, 0x10000000) # 256MB
    }
    if ($result.ThreadCount -gt 0) {
        $result.Signals += "SIG_THREADS"
    }
    if ($result.MaxBraceDepth -gt 5) {
        $result.Signals += "SIG_NESTED_LOOPS"
    }
    if ($result.AllocCount -gt 5) {
        $result.Signals += "SIG_HEAVY_ALLOC"
        $result.HeapReserve *= 2
    }

    # Dynamic limits
    $result.StackReserve = [math]::Max($result.StackReserve, ($result.Complexity * 4096) + 0x100000)
    $result.HeapReserve = [math]::Max($result.HeapReserve, ($result.Complexity * 8192) + 0x100000)
    $result.WorkingSet = $result.StackReserve + $result.HeapReserve + 0x400000

    return $result
}

# ============================================================
# Build a single MASM file
# ============================================================
function Build-MASMFile {
    param(
        [string]$AsmFile,
        [string]$OutExe,
        [string]$Entry = "_start",
        [string[]]$ExtraLibs = @("kernel32.lib"),
        [switch]$DynamicBase
    )

    Write-Host "`n[BUILD] Assembling $AsmFile..." -ForegroundColor Yellow
    & $AssemblerPath /c /nologo /Cp $AsmFile 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[FAIL] Assembly of $AsmFile failed" -ForegroundColor Red
        return $false
    }

    $objFile = [System.IO.Path]::ChangeExtension($AsmFile, ".obj")
    $linkArgs = @("/ENTRY:$Entry", "/SUBSYSTEM:CONSOLE", "/OUT:$OutExe") + $ExtraLibs + @($objFile)
    if ($DynamicBase) {
        $linkArgs += "/DYNAMICBASE"
        $linkArgs += "/NXCOMPAT"
    }

    Write-Host "[BUILD] Linking $objFile -> $OutExe..." -ForegroundColor Yellow
    & $LinkerPath @linkArgs 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[FAIL] Linking failed" -ForegroundColor Red
        return $false
    }

    Write-Host "[OK]    Built: $OutExe" -ForegroundColor Green
    return $true
}

# ============================================================
# Process Agent: Compile one source through the engine
# ============================================================
function Invoke-ProcessAgent {
    param(
        [string]$SourcePath,
        [string]$EngineExe = "engine_core.exe"
    )

    Write-Host "`n===== PROCESS AGENT: $SourcePath =====" -ForegroundColor Magenta

    # Phase 1: Pre-scan in PowerShell
    $analysis = Analyze-SourceComplexity -Path $SourcePath
    if (-not $analysis) {
        Write-Host "[!] Cannot analyze $SourcePath" -ForegroundColor Red
        return
    }

    Write-Host "[DIGEST] File: $SourcePath ($($analysis.FileSize) bytes)" -ForegroundColor Cyan
    Write-Host "[DIGEST] Complexity Score: $($analysis.Complexity)" -ForegroundColor Cyan
    Write-Host "[DIGEST] Functions: $($analysis.FnCount), Loops: $($analysis.WhileCount), Depth: $($analysis.MaxBraceDepth)" -ForegroundColor Cyan

    if ($analysis.Signals.Count -gt 0) {
        Write-Host "[SIGNAL] $($analysis.Signals -join ', ')" -ForegroundColor Yellow
    }

    $stackMB = [math]::Round($analysis.StackReserve / 1MB, 2)
    $heapMB = [math]::Round($analysis.HeapReserve / 1MB, 2)
    $wsMB = [math]::Round($analysis.WorkingSet / 1MB, 2)
    Write-Host "[AGENT]  Limit-Key: STACK=${stackMB}MB HEAP=${heapMB}MB WS=${wsMB}MB" -ForegroundColor Green

    # Phase 2: Run the engine
    if (Test-Path $EngineExe) {
        Write-Host "[INJECT] Running engine on $SourcePath..." -ForegroundColor Yellow
        $job = Start-Job -ScriptBlock {
            param($exe, $src)
            & $exe $src 2>&1
        } -ArgumentList (Resolve-Path $EngineExe).Path, (Resolve-Path $SourcePath).Path

        $job | Wait-Job -Timeout 60 | Out-Null
        $output = $job | Receive-Job
        $job | Remove-Job -Force

        if ($output) {
            $output | ForEach-Object { Write-Host "  $_" }
        }
    } else {
        Write-Host "[SKIP] Engine not built yet. Build with -BuildEngine first." -ForegroundColor DarkYellow
    }

    Write-Host "[RESET] Zero-state memory manifest applied." -ForegroundColor DarkCyan
    Write-Host "========================================`n" -ForegroundColor Magenta
}

# ============================================================
# MAIN EXECUTION
# ============================================================

# Validate tools
if (-not (Test-Path $AssemblerPath)) {
    # Try fallback paths
    $fallbacks = @(
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
    )
    foreach ($fb in $fallbacks) {
        if (Test-Path $fb) {
            $AssemblerPath = $fb
            $LinkerPath = [System.IO.Path]::Combine([System.IO.Path]::GetDirectoryName($fb), "link.exe")
            break
        }
    }
}

if (-not (Test-Path $AssemblerPath)) {
    Write-Error "ml64.exe not found. Set -AssemblerPath."
    exit 1
}

# --- Mode: Build Engine ---
if ($BuildEngine -or $FullPipeline) {
    Write-Host "`n=== Building RXUC Process Agent Engine ===" -ForegroundColor Cyan
    $ok = Build-MASMFile -AsmFile "engine_core.asm" -OutExe "engine_core.exe" -DynamicBase
    if (-not $ok) {
        Write-Host "Engine build failed. Attempting terraform.asm fallback..." -ForegroundColor Yellow
    }
}

# --- Mode: Build TerraForm compiler ---
if (-not $BuildEngine -or $FullPipeline) {
    # If we have an input source, adjust limits first
    if ($InputSource -and (Test-Path $InputSource)) {
        $sourceSize = (Get-Item $InputSource).Length
        Write-Host "[AGENT] Adjusting limits for $InputSource ($sourceSize bytes)..." -ForegroundColor Cyan
        Adjust-Limits -SourceSize $sourceSize
    }

    try {
        $ok = Build-MASMFile -AsmFile $SourceFile -OutExe $OutputFile
        if (-not $ok) { exit 1 }
    } finally {
        Restore-Original
    }
}

# --- Mode: Process single input source ---
if ($InputSource -and (Test-Path $InputSource)) {
    Invoke-ProcessAgent -SourcePath $InputSource
}

# --- Mode: Process multiple source files ---
if ($SourceFiles.Count -gt 0) {
    Write-Host "`n=== Batch Processing: $($SourceFiles.Count) sources ===" -ForegroundColor Cyan
    foreach ($src in $SourceFiles) {
        if (Test-Path $src) {
            Invoke-ProcessAgent -SourcePath $src
        } else {
            Write-Host "[SKIP] Not found: $src" -ForegroundColor DarkYellow
        }
    }
}

Write-Host "`n[DONE] All operations complete." -ForegroundColor Green