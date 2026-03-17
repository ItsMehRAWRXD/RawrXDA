# COFF Forensics and Symbol Collision Detection
# Analyzes all .obj files for corruption, duplicate symbols, and section alignment issues

param(
    [string]$ObjDir = "D:\rawrxd\obj",
    [string]$ReportsDir = "D:\rawrxd\tools\reports",
    [switch]$FullScan,
    [switch]$FixCorrupted
)

$ErrorActionPreference = "Continue"

# Ensure reports directory exists
New-Item -ItemType Directory -Force -Path $ReportsDir | Out-Null

# VS Tools detection
$possibleVSPaths = @(
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC",
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC"
)

$vcToolsDir = $null
foreach ($basePath in $possibleVSPaths) {
    if (Test-Path $basePath) {
        $versions = Get-ChildItem $basePath | Sort-Object Name -Descending
        if ($versions.Count -gt 0) {
            $vcToolsDir = Join-Path $versions[0].FullName "bin\Hostx64\x64"
            break
        }
    }
}

if (-not $vcToolsDir -or -not (Test-Path (Join-Path $vcToolsDir "dumpbin.exe"))) {
    Write-Host "❌ Cannot find VS Build Tools! Please install VS 2022 Build Tools." -ForegroundColor Red
    exit 1
}

$dumpbin = Join-Path $vcToolsDir "dumpbin.exe"
$lib = Join-Path $vcToolsDir "lib.exe"

Write-Host "🔍 COFF Forensics Analysis Starting..." -ForegroundColor Cyan
Write-Host "Using VC Tools: $vcToolsDir" -ForegroundColor Gray
Write-Host "=" * 80

# Find all .obj files in build directories
$objFiles = @()
$searchPaths = @($ObjDir, "D:\rawrxd\build", "D:\rawrxd\build_prod", "D:\rawrxd\obj-native")
foreach ($path in $searchPaths) {
    if (Test-Path $path) {
        $objFiles += Get-ChildItem "$path\*.obj" -Recurse
    }
}

$report = @{
    CorruptedFiles = @()
    DuplicateSymbols = @{}
    SymbolConflicts = @()
    LargeObjects = @()
    InvalidSections = @()
    TotalSymbols = @{}
    ProcessedFiles = 0
    FailedFiles = @()
}

Write-Host "📂 Found $($objFiles.Count) object files to analyze" -ForegroundColor Yellow

foreach ($obj in $objFiles) {
    $report.ProcessedFiles++
    $fileName = $obj.Name
    Write-Host "  📦 [$($report.ProcessedFiles)/$($objFiles.Count)] $fileName" -ForegroundColor Gray
    
    try {
        # Basic COFF header analysis
        $dumpOutput = & $dumpbin /HEADERS $obj.FullName 2>&1 | Out-String
        
        # Check for corruption indicators
        $isCorrupted = $false
        $corruptionReason = ""
        
        if ($dumpOutput -match "fatal error|invalid|corrupt|truncated") {
            $isCorrupted = $true
            $corruptionReason = "COFF header corruption detected"
        }
        
        if ($dumpOutput -match "\.pdata.*invalid") {
            $isCorrupted = $true
            $corruptionReason += ";Invalid .pdata contributions"
        }
        
        if ($obj.Length -eq 0) {
            $isCorrupted = $true
            $corruptionReason += ";Zero-byte file"
        }
        
        # Check for section overflow (BIGOBJ needed)
        $sectionMatches = [regex]::Matches($dumpOutput, "SECTION HEADER #(\d+)")
        $sectionCount = $sectionMatches.Count
        $isBigObj = $dumpOutput -match "BIGOBJ"
        
        if ($sectionCount -gt 65279 -and -not $isBigObj) {
            $isCorrupted = $true
            $corruptionReason += ";Needs BIGOBJ flag (sections: $sectionCount)"
        }
        
        if ($isCorrupted) {
            $report.CorruptedFiles += @{
                File = $obj.FullName
                Reason = $corruptionReason.Trim(';')
                Size = $obj.Length
            }
        }
        
        # Symbol analysis
        if (-not $isCorrupted) {
            $symbolOutput = & $dumpbin /SYMBOLS $obj.FullName 2>&1 | Out-String
            
            # Extract symbol names
            $symbolMatches = [regex]::Matches($symbolOutput, "External\s+\|\s+([^\s\|]+)")
            foreach ($match in $symbolMatches) {
                $symbolName = $match.Groups[1].Value
                
                if (-not $report.TotalSymbols.ContainsKey($symbolName)) {
                    $report.TotalSymbols[$symbolName] = @()
                }
                $report.TotalSymbols[$symbolName] += $fileName
                
                # Check for known problematic symbols
                $problemSymbols = @("main", "DllMain", "write_con", "strlen_s", "itoa_u64", 
                                   "ClassifyPattern", "WinMain", "_start")
                
                if ($symbolName -in $problemSymbols) {
                    if (-not $report.DuplicateSymbols.ContainsKey($symbolName)) {
                        $report.DuplicateSymbols[$symbolName] = @()
                    }
                    $report.DuplicateSymbols[$symbolName] += $fileName
                }
            }
        }
        
        # Check object size (identify heavyweight objects)
        if ($obj.Length -gt 10MB) {
            $report.LargeObjects += @{
                File = $fileName
                Size = [math]::Round($obj.Length / 1MB, 2)
                Sections = $sectionCount
            }
        }
        
    } catch {
        $report.FailedFiles += @{
            File = $fileName
            Error = $_.Exception.Message
        }
    }
    
    # Progress indicator for large scans
    if ($report.ProcessedFiles % 10 -eq 0) {
        Write-Host "    Progress: $($report.ProcessedFiles)/$($objFiles.Count)" -ForegroundColor DarkGray
    }
}

# Generate conflict analysis
Write-Host "`n🔍 Analyzing symbol conflicts..." -ForegroundColor Yellow
foreach ($symbol in $report.DuplicateSymbols.Keys) {
    if ($report.DuplicateSymbols[$symbol].Count -gt 1) {
        $report.SymbolConflicts += @{
            Symbol = $symbol
            Files = $report.DuplicateSymbols[$symbol]
            Count = $report.DuplicateSymbols[$symbol].Count
        }
    }
}

# Output detailed report
$reportFile = Join-Path $ReportsDir "coff_forensics_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
$report | ConvertTo-Json -Depth 10 | Out-File $reportFile

Write-Host "`n📊 FORENSICS SUMMARY" -ForegroundColor Cyan
Write-Host "=" * 50
Write-Host "✅ Files analyzed: $($report.ProcessedFiles)" -ForegroundColor Green
Write-Host "❌ Corrupted files: $($report.CorruptedFiles.Count)" -ForegroundColor Red
Write-Host "⚠️ Symbol conflicts: $($report.SymbolConflicts.Count)" -ForegroundColor Yellow
Write-Host "📈 Large objects (>10MB): $($report.LargeObjects.Count)" -ForegroundColor Yellow
Write-Host "💥 Failed analyses: $($report.FailedFiles.Count)" -ForegroundColor Red

if ($report.CorruptedFiles.Count -gt 0) {
    Write-Host "`n🚨 CORRUPTED FILES:" -ForegroundColor Red
    foreach ($corrupt in $report.CorruptedFiles) {
        Write-Host "   🗃️ $($corrupt.File)" -ForegroundColor Red
        Write-Host "      Reason: $($corrupt.Reason)" -ForegroundColor DarkRed
    }
}

if ($report.SymbolConflicts.Count -gt 0) {
    Write-Host "`n⚠️ SYMBOL CONFLICTS:" -ForegroundColor Yellow
    foreach ($conflict in $report.SymbolConflicts) {
        Write-Host "   🔄 $($conflict.Symbol) ($($conflict.Count) files)" -ForegroundColor Yellow
        foreach ($file in $conflict.Files) {
            Write-Host "      - $file" -ForegroundColor DarkYellow
        }
    }
}

if ($report.LargeObjects.Count -gt 0) {
    Write-Host "`n📈 LARGE OBJECTS:" -ForegroundColor Yellow
    foreach ($large in $report.LargeObjects) {
        Write-Host "   📦 $($large.File): $($large.Size) MB ($($large.Sections) sections)" -ForegroundColor Yellow
    }
}

# Generate remediation script
$fixScript = Join-Path $ReportsDir "auto_fix_corrupted.ps1"
$fixCommands = @()

$fixCommands += "# Auto-generated COFF corruption fixes"
$fixCommands += "# Generated on $(Get-Date)"
$fixCommands += ""

foreach ($corrupt in $report.CorruptedFiles) {
    $fixCommands += "# Fix: $($corrupt.File)"
    $fixCommands += "# Reason: $($corrupt.Reason)"
    
    if ($corrupt.Reason -match "Zero-byte") {
        $fixCommands += "Remove-Item '$($corrupt.File)'"
    } elseif ($corrupt.Reason -match "BIGOBJ") {
        $asmFile = $corrupt.File -replace "\.obj$", ".asm"
        if (Test-Path $asmFile) {
            $fixCommands += "ml64.exe /c /Fo'$($corrupt.File)' /bigobj '$asmFile'"
        }
    } elseif ($corrupt.Reason -match "pdata") {
        $fixCommands += "# Rebuild with /PDBALTPATH flag"
        $fixCommands += "Write-Host 'Manual intervention needed for $($corrupt.File)'"
    }
    $fixCommands += ""
}

$fixCommands -join "`n" | Out-File $fixScript

Write-Host "`n📄 Detailed report saved: $reportFile" -ForegroundColor Green
Write-Host "🛠️ Fix script generated: $fixScript" -ForegroundColor Green

# Execute fixes if requested
if ($FixCorrupted -and $report.CorruptedFiles.Count -gt 0) {
    Write-Host "`n🔧 Executing automatic fixes..." -ForegroundColor Yellow
    & $fixScript
}

Write-Host "`n✅ COFF Forensics Analysis Complete!" -ForegroundColor Green