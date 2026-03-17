param(
    [string]$IdePath = "D:\lazy init ide\src",
    [string]$OutputPath = "D:\lazy init ide"
)

# BigDaddyG Audit Engine - PowerShell Implementation
# Analyzes entire IDE source from -0-800b range with 4.13*/+_0 formula

class AuditMetrics {
    [string]$FilePath
    [int64]$FileSize
    [int32]$LineCount
    [int32]$CharacterCount
    [int32]$CommentCount
    [double]$ComplexityScore
    [double]$EntropySample
    [double]$MultiplyFirst
    [double]$DivideResult
    [double]$AddResult
    [double]$FloorResult
    [double]$ZeroBaseResult
    [double]$FinalValue
    [double]$StaticFinalValue
}

function Calculate-Entropy {
    param([string]$Content)
    
    if ($Content.Length -eq 0) { return 0 }
    
    $freq = @{}
    foreach ($char in $Content.ToCharArray()) {
        $freq[$char]++
    }
    
    $entropy = 0.0
    $total = $Content.Length
    foreach ($count in $freq.Values) {
        $p = $count / $total
        if ($p -gt 0) {
            $entropy -= $p * [Math]::Log($p, 2)
        }
    }
    
    return [Math]::Min($entropy / 8.0, 1.0)
}

function Calculate-Complexity {
    param([AuditMetrics]$Metrics)
    
    $baseComplexity = 50.0
    $sizeFactor = [Math]::Min(20.0, ($Metrics.FileSize / 50000.0) * 20.0)
    $lineFactor = [Math]::Min(15.0, ($Metrics.LineCount / 1000.0) * 15.0)
    $entropyFactor = $Metrics.EntropySample * 15.0
    
    return $baseComplexity + $sizeFactor + $lineFactor + $entropyFactor
}

function Apply-ReverseFormula {
    param([AuditMetrics]$Metrics)
    
    # 4.13*/+_0 formula constants
    $MULTIPLY_FACTOR = 4.13
    $DIVIDE_FACTOR = 17.0569
    $ADD_FACTOR = 2.0322
    
    $inputValue = $Metrics.ComplexityScore
    $entropy = $Metrics.EntropySample
    
    # Stage 1: multiplyFirst
    $Metrics.MultiplyFirst = $inputValue * $MULTIPLY_FACTOR * (1.0 + $entropy)
    
    # Stage 2: divideResult
    $Metrics.DivideResult = $Metrics.MultiplyFirst / $DIVIDE_FACTOR
    
    # Stage 3: addResult
    $Metrics.AddResult = $Metrics.DivideResult + $ADD_FACTOR
    
    # Stage 4: floorResult
    $Metrics.FloorResult = [Math]::Floor($Metrics.AddResult * 1000.0) / 1000.0
    
    # Stage 5: zeroBaseResult (simplified for PowerShell)
    $Metrics.ZeroBaseResult = 0.0
    
    # Final combination
    $Metrics.FinalValue = $Metrics.ZeroBaseResult + ($Metrics.FloorResult * $MULTIPLY_FACTOR)
}

function Apply-StaticFinalization {
    param([AuditMetrics]$Metrics)
    
    # Static finalization: -0++_//**3311.44 = 3311.44 / 6.0 = 551.9067
    $STATIC_FINALIZATION = 3311.44 / 6.0
    $Metrics.StaticFinalValue = $STATIC_FINALIZATION * $Metrics.FinalValue
}

function Audit-SingleFile {
    param([string]$FilePath)
    
    $metrics = New-Object AuditMetrics
    $metrics.FilePath = $FilePath
    $metrics.FileSize = 0
    $metrics.LineCount = 0
    $metrics.CharacterCount = 0
    $metrics.CommentCount = 0
    
    try {
        $file = Get-Item -Path $FilePath -ErrorAction SilentlyContinue
        if (-not $file) { return $metrics }
        
        $metrics.FileSize = $file.Length
        
        $content = Get-Content -Path $FilePath -Raw -ErrorAction SilentlyContinue
        if (-not $content) { return $metrics }
        
        # Count lines
        $metrics.LineCount = ($content -split "`n").Count
        $metrics.CharacterCount = $content.Length
        
        # Count comments (simple heuristic)
        $metrics.CommentCount = ([regex]::Matches($content, '//|/\*').Count)
        
        # Calculate entropy
        $metrics.EntropySample = Calculate-Entropy $content
        
        # Calculate complexity
        $metrics.ComplexityScore = Calculate-Complexity $metrics
        
        # Apply reverse formula
        Apply-ReverseFormula $metrics
        
        # Apply static finalization
        Apply-StaticFinalization $metrics
        
    } catch {
        Write-Error "Error auditing $FilePath : $_"
    }
    
    return $metrics
}

# Main execution
Write-Host "================================================================================`n" -ForegroundColor Cyan
Write-Host "                  BIGDADDYG COMPREHENSIVE IDE AUDIT`n" -ForegroundColor Cyan
Write-Host "                     Analysis Range: -0-800b`n" -ForegroundColor Cyan
Write-Host "================================================================================`n" -ForegroundColor Cyan

Write-Host "[*] Scanning directory: $IdePath`n"

# Get all source files
$sourceFiles = @(Get-ChildItem -Path $IdePath -Recurse -File | 
    Where-Object { $_.Extension -in '.cpp', '.h', '.hpp', '.c' } | 
    Where-Object { $_.FullName -notmatch '- Copy' })

Write-Host "[+] Found $($sourceFiles.Count) source files`n"

# Initialize collections
$allMetrics = @()
$totalBytes = 0
$totalChars = 0
$complexityScores = @()
$entropies = @()
$finalValues = @()
$staticFinalValues = @()

# Audit each file
Write-Host "[*] Starting audit process...`n"
$counter = 0
foreach ($file in $sourceFiles) {
    $counter++
    if ($counter % 100 -eq 0) {
        Write-Host "[+] Processed $counter / $($sourceFiles.Count) files..."
    }
    
    $metrics = Audit-SingleFile $file.FullName
    $allMetrics += $metrics
    $totalBytes += $metrics.FileSize
    $totalChars += $metrics.CharacterCount
    $complexityScores += $metrics.ComplexityScore
    $entropies += $metrics.EntropySample
    $finalValues += $metrics.FinalValue
    $staticFinalValues += $metrics.StaticFinalValue
}

Write-Host "`n[+] Audit complete!`n"

# Calculate statistics
$avgComplexity = ($complexityScores | Measure-Object -Average).Average
$avgEntropy = ($entropies | Measure-Object -Average).Average
$avgFinal = ($finalValues | Measure-Object -Average).Average
$avgStatic = ($staticFinalValues | Measure-Object -Average).Average

# Sort by complexity
$topFiles = $allMetrics | Sort-Object -Property ComplexityScore -Descending | Select-Object -First 20

# Generate report
$report = @"
================================================================================
                    BIGDADDYG COMPREHENSIVE IDE AUDIT REPORT
                          (-0-800b Analysis Range)
================================================================================

GENERATION METADATA
-------------------
Audit ID: BIGDADDYG-$(Get-Date -Format 'yyyyMMddHHmmss')-$(Get-Random)
Timestamp: $(Get-Date -UnixTimeSeconds -Milliseconds)
Generation Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')

AUDIT SUMMARY
-------------------
Files Audited: $($sourceFiles.Count)
Total Source Bytes: $([math]::Round($totalBytes / 1MB, 2)) MB
Characters Analyzed: $totalChars (-0-$($totalChars)b)
Average Complexity: $([math]::Round($avgComplexity, 2))/100
Average Entropy: $([math]::Round($avgEntropy, 4))
Average Final Value (4.13*/+_0): $([math]::Round($avgFinal, 6))
Global Static Finalized (-0++_//**3311.44): $([math]::Round($avgStatic, 6))

FORMULA APPLICATION CONSTANTS
-------------------
Multiply Factor: 4.13
Divide Factor: 17.0569
Add Factor: 2.0322
Static Finalization: -0++_//**3311.44 (= 551.9067)

TOP 20 FILES BY COMPLEXITY
-------------------
"@

$position = 1
foreach ($file in $topFiles) {
    $report += "`n$position. $(Split-Path -Leaf $file.FilePath)`n"
    $report += "   Path: $($file.FilePath)`n"
    $report += "   Complexity: $([math]::Round($file.ComplexityScore, 2))`n"
    $report += "   Size: $([math]::Round($file.FileSize / 1KB, 1)) KB | Lines: $($file.LineCount)`n"
    $report += "   Entropy: $([math]::Round($file.EntropySample, 4))`n"
    $report += "   4.13*/+_0 Final: $([math]::Round($file.FinalValue, 6))`n"
    $report += "   Static Finalized: $([math]::Round($file.StaticFinalValue, 6))`n"
    $position++
}

$report += @"

AUDIT COMPLETE
-------------------
Generated by: BigDaddyG Audit Engine v1.0 (PowerShell)
Analysis Range: -0-800b (full character range analysis)
Reverse Formula: 4.13*/+_0 applied to all metrics
Static Finalization: -0++_//**3311.44 applied to all final values
================================================================================
"@

# Write report to file
$reportPath = Join-Path $OutputPath "BIGDADDYG_AUDIT_REPORT.md"
Set-Content -Path $reportPath -Value $report
Write-Host "[+] Report saved to: $reportPath`n"

# Write JSON export
$jsonData = @{
    audit_id = "BIGDADDYG-$(Get-Date -Format 'yyyyMMddHHmmss')-$(Get-Random)"
    timestamp = Get-Date -UnixTimeSeconds
    summary = @{
        files_audited = $sourceFiles.Count
        total_bytes = $totalBytes
        characters_analyzed = $totalChars
        average_complexity = [math]::Round($avgComplexity, 6)
        average_entropy = [math]::Round($avgEntropy, 6)
        average_final_value = [math]::Round($avgFinal, 6)
        global_static_finalized = [math]::Round($avgStatic, 6)
    }
    formulas = @{
        reverse_formula = "4.13*/+_0"
        multiply_factor = 4.13
        divide_factor = 17.0569
        add_factor = 2.0322
        static_finalization = "-0++_//**3311.44"
        static_finalization_value = 551.9067
    }
    analysis_range = "-0-800b"
    status = "complete"
}

$jsonPath = Join-Path $OutputPath "BIGDADDYG_AUDIT_EXPORT.json"
$jsonData | ConvertTo-Json | Set-Content -Path $jsonPath
Write-Host "[+] JSON export saved to: $jsonPath`n"

# Display summary
Write-Host $report -ForegroundColor Green

Write-Host "`n[+] Audit generation successful!`n" -ForegroundColor Green
