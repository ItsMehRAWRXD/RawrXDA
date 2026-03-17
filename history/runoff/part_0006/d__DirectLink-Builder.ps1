# DirectLink-Builder.ps1
# Custom compilation architecture: Source → Direct Linking (no assembly step)
# Implements: source = linking - assembling

param(
    [string]$Root = "D:\rawrxd",
    [string]$CustomLinker = "D:\rawrxd\bin\custom-linker.exe",
    [string]$OutDir = "$Root\build",
    [switch]$AutoDiscover,
    [switch]$PlanOnly,
    [int]$BatchSize = 50,
    [int]$MaxParallel = 0,
    [string[]]$ExcludeDirNames = @(".git", "build", "dist", "node_modules", "temp", "source_audit", "crash_dumps"),
    [string[]]$SourceExtensions = @("*.asm", "*.obj", "*.o")  # Pre-assembled source formats
)

$ErrorActionPreference = "Stop"

# ============================================================
# DIRECT LINKING ENGINE (No Assembly Required)
# ============================================================

function Invoke-DirectLink($sourceFile, $outputFile) {
    $fileName = Split-Path $sourceFile -Leaf
    
    Write-Host "`n[LINK] Processing $fileName..." -Fore White
    
    # Verify custom linker exists
    if(!(Test-Path $CustomLinker)) { 
        throw "Custom linker not found at '$CustomLinker'. Pass -CustomLinker or create the linker tool." 
    }
    
    # Read source (already assembled)
    $source = Get-Content $sourceFile -Raw -Encoding UTF8
    Write-Host "  [INFO] Source size: $(($source.Length / 1024).ToString('F2')) KB" -Fore Gray
    
    # Link using custom compiler (direct linking, no assembly)
    $logFile = Join-Path $OutDir "$($fileName).link.log"
    
    $args = @(
        "/in:`"$sourceFile`"",
        "/out:`"$outputFile`"",
        "/log:`"$logFile`"",
        "/mode:direct-link"
    )
    
    Write-Host "  [EXEC] $CustomLinker $($args -join ' ')" -Fore Cyan
    
    $proc = Start-Process -FilePath $CustomLinker -ArgumentList $args -Wait -PassThru `
        -RedirectStandardOutput $logFile -RedirectStandardError "$logFile.err" -NoNewWindow
    
    # Check for errors
    $errLog = Get-Content "$logFile.err" -Raw -ErrorAction SilentlyContinue
    $stdLog = Get-Content $logFile -Raw -ErrorAction SilentlyContinue
    
    if($proc.ExitCode -ne 0 -or $errLog -match "error") {
        Write-Host "`n[ERROR] Linking failed for $fileName" -Fore Red
        Write-Host "STDOUT:`n$stdLog" -Fore Red
        Write-Host "STDERR:`n$errLog" -Fore Red
        throw "Linker returned exit code: $($proc.ExitCode)"
    }
    
    if($stdLog -match "warning") {
        Write-Host "  [WARN]" -Fore Yellow
        Write-Host $stdLog -Fore Yellow
    } else {
        Write-Host "  [OK] Linked: $outputFile" -Fore Green
    }
    
    # Cleanup logs unless debugging
    if(Test-Path $logFile) { Remove-Item $logFile -Force -ErrorAction SilentlyContinue }
    if(Test-Path "$logFile.err") { Remove-Item "$logFile.err" -Force -ErrorAction SilentlyContinue }
    
    return $outputFile
}

# ============================================================
# AUTODISCOVER + BATCH LINKING
# ============================================================

function Get-AutoDiscoverSourceFiles([string]$rootPath, [string[]]$excludeDirNames, [string[]]$extensions) {
    $rootItem = Get-Item -LiteralPath $rootPath -ErrorAction Stop
    $files = @()
    
    foreach($ext in $extensions) {
        $files += Get-ChildItem -LiteralPath $rootItem.FullName -Recurse -File -Filter $ext -ErrorAction SilentlyContinue |
            Where-Object {
                $fullPath = $_.FullName
                $excluded = $false
                foreach($excludeName in $excludeDirNames) {
                    if($fullPath -like "*\$excludeName\*" -or $fullPath -like "*\$excludeName") {
                        $excluded = $true
                        break
                    }
                }
                return -not $excluded
            }
    }

    # Deterministic order helps debugging and caching
    return $files | Sort-Object FullName -Unique
}

function Get-OutputPathForSource([string]$sourceFile, [string]$rootPath, [string]$outDir) {
    $rel = [IO.Path]::GetRelativePath($rootPath, $sourceFile)
    $linkedExt = ".linked.asm"
    $relLinked = [IO.Path]::ChangeExtension($rel, $linkedExt)
    $linkedPath = Join-Path $outDir (Join-Path "linked" $relLinked)
    $linkedParent = Split-Path $linkedPath -Parent
    if(!(Test-Path $linkedParent)) { New-Item -ItemType Directory -Path $linkedParent -Force | Out-Null }
    return $linkedPath
}

function Split-IntoBatches([object[]]$items, [int]$batchSize) {
    if($batchSize -lt 1) { throw "BatchSize must be >= 1" }
    $batches = New-Object System.Collections.Generic.List[object[]]
    for($i = 0; $i -lt $items.Count; $i += $batchSize) {
        $batch = $items[$i..([Math]::Min($i + $batchSize - 1, $items.Count - 1))]
        $batches.Add(, $batch)
    }
    return $batches
}

function Resolve-CustomLinker([string]$preferredPath) {
    if($preferredPath -and (Test-Path $preferredPath)) { return $preferredPath }

    # Search in common locations
    $searchPaths = @(
        "D:\rawrxd\bin\custom-linker.exe",
        "C:\bin\custom-linker.exe",
        "D:\build\bin\linker.exe"
    )

    foreach($p in $searchPaths) {
        if(Test-Path $p) { return $p }
    }

    return $null
}

# ============================================================
# MAIN LINKING PIPELINE
# ============================================================

Write-Host "RAW RXD DIRECT-LINK BUILDER v1.0 (No Assembly)" -Fore Cyan
Write-Host "Architecture: source = linking - assembling" -Fore Cyan
Write-Host "Root: $Root" -Fore Gray

if(!(Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

if($MaxParallel -le 0) { $MaxParallel = [Math]::Max(1, [Environment]::ProcessorCount) }

$ResolvedLinker = $null
if(-not $PlanOnly) {
    $ResolvedLinker = Resolve-CustomLinker $CustomLinker
    if(!$ResolvedLinker) { throw "Custom linker not found. Pass -CustomLinker or place linker at D:\rawrxd\bin\custom-linker.exe" }
    $CustomLinker = $ResolvedLinker
    Write-Host "Linker: $CustomLinker" -Fore Green
}

$linkedOutputs = New-Object System.Collections.Generic.List[string]

if($AutoDiscover) {
    Write-Host "Mode: Auto-discover (source) + batched direct linking" -Fore Cyan
    Write-Host "SourceExtensions: $($SourceExtensions -join ', ')" -Fore Gray
    Write-Host "BatchSize: $BatchSize | MaxParallel: $MaxParallel" -Fore Gray
    Write-Host "ExcludeDirNames: $($ExcludeDirNames -join ', ')" -Fore Gray

    $sourceFiles = Get-AutoDiscoverSourceFiles $Root $ExcludeDirNames $SourceExtensions
    Write-Host "Discovered: $($sourceFiles.Count) source files" -Fore White
    if($sourceFiles.Count -eq 0) { 
        Write-Host "No source files found. Exiting." -Fore Yellow
        exit 0 
    }

    $batches = Split-IntoBatches $sourceFiles $BatchSize
    Write-Host "Batches: $($batches.Count) ($BatchSize sources per batch)" -Fore White

    if($PlanOnly) {
        Write-Host "`n=== PLAN ONLY (No linking) ===" -Fore Cyan
        foreach($i = 0; $i -lt $batches.Count; $i++) {
            $batch = $batches[$i]
            Write-Host "Batch $($i+1)/$($batches.Count): $($batch.Count) files" -Fore White
            foreach($file in $batch) {
                $out = Get-OutputPathForSource $file.FullName $Root $OutDir
                Write-Host "  $($file.Name) → $(Split-Path $out -Leaf)" -Fore Gray
            }
        }
        Write-Host "`nTo execute: Remove -PlanOnly flag" -Fore Cyan
        exit 0
    }

    # Execute batches in parallel
    $results = $batches | ForEach-Object -Parallel {
        param($batch, $root, $outDir, $customLinker)
        
        $localLinked = New-Object System.Collections.Generic.List[string]
        
        foreach($sourceFile in $batch) {
            try {
                $output = Get-OutputPathForSource $sourceFile.FullName $root $outDir
                $linked = Invoke-DirectLink $sourceFile.FullName $output
                $localLinked.Add($linked)
            }
            catch {
                Write-Host "ERROR in batch: $_" -Fore Red
            }
        }
        
        return $localLinked.ToArray()
    } -ArgumentList $_, $Root, $OutDir, $CustomLinker -ThrottleLimit $MaxParallel

    foreach($linked in ($results | Where-Object { $_ })) {
        $linkedOutputs.Add($linked)
    }

} else {
    # Legacy: Manual source list
    $BuildQueue = @(
        "D:\rawrxd\src\masm\genesis.asm",
        "D:\rawrxd\src\masm\WidgetEngine.asm",
        "D:\rawrxd\src\masm\HeadlessWidgets.asm"
    )

    foreach($sourceFile in $BuildQueue) {
        if(Test-Path $sourceFile) {
            $output = Get-OutputPathForSource $sourceFile $Root $OutDir
            $linked = Invoke-DirectLink $sourceFile $output
            $linkedOutputs.Add($linked)
        } else {
            Write-Host "[SKIP] Not found: $sourceFile" -Fore Yellow
        }
    }
}

Write-Host "`n=== LINKING COMPLETE ===" -Fore Green
Write-Host "Linked outputs: $($linkedOutputs.Count)" -Fore White
if($linkedOutputs.Count -gt 0) {
    Write-Host "Output directory: $OutDir" -Fore Green
    Write-Host "Files ready for execution or further processing." -Fore Green
}

# Export linked file list
$linkedOutputs.ToArray() -join "`n" | Out-File "$OutDir\linked.txt" -Encoding UTF8
Write-Host "`nLinked file list: $OutDir\linked.txt" -Fore Cyan
