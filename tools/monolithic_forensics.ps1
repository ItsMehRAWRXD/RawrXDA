# Monolithic MASM COFF Forensics & Conflict Detection
# Analyzes .obj files for section overflow, symbol collisions, and alignment violations

param(
    [string]$ObjDir = "D:\rawrxd\obj",
    [string]$VCToolsDir = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64"
)

$dumpbin = Join-Path $VCToolsDir "dumpbin.exe"
if (-not (Test-Path $dumpbin)) {
    Write-Host "❌ dumpbin.exe not found at $dumpbin" -ForegroundColor Red
    exit 1
}

Write-Host "🔍 COFF Forensics Analysis - Monolithic Objects" -ForegroundColor Cyan
Write-Host "=" * 80

$objs = Get-ChildItem "$ObjDir\asm_monolithic_*.obj" | Sort-Object Length -Descending
$report = @()

foreach ($obj in $objs) {
    Write-Host "`n📦 Analyzing: $($obj.Name)" -ForegroundColor Yellow
    
    # Run dumpbin to get COFF headers
    $dump = & $dumpbin /HEADERS $obj.FullName 2>&1 | Out-String
    
    # Parse section count
    $sectionMatches = [regex]::Matches($dump, "SECTION HEADER #(\d+)")
    $sectionCount = $sectionMatches.Count
    
    # Parse symbol count
    $symbolMatch = [regex]::Match($dump, "(\d+) symbols")
    $symbolCount = if ($symbolMatch.Success) { [int]$symbolMatch.Groups[1].Value } else { 0 }
    
    # Check for BIGOBJ flag
    $isBigObj = $dump -match "BIGOBJ"
    
    # Parse sections for size info
    $textSize = 0
    $dataSize = 0
    $rdataSize = 0
    
    $sectionLines = $dump -split "`n" | Where-Object { $_ -match "^\s+\.text|^\s+\.data|^\s+\.rdata" }
    foreach ($line in $sectionLines) {
        if ($line -match "\.text\s+([0-9A-F]+)") {
            $textSize += [Convert]::ToInt64($matches[1], 16)
        }
        if ($line -match "\.data\s+([0-9A-F]+)") {
            $dataSize += [Convert]::ToInt64($matches[1], 16)
        }
        if ($line -match "\.rdata\s+([0-9A-F]+)") {
            $rdataSize += [Convert]::ToInt64($matches[1], 16)
        }
    }
    
    # Detect issues
    $issues = @()
    if ($sectionCount -gt 65535) { $issues += "SECTION_OVERFLOW" }
    if ($symbolCount -gt 60000) { $issues += "SYMBOL_OVERFLOW" }
    if ($textSize -gt 1932735283) { $issues += "TEXT_SECTION_TOO_LARGE" }
    if ($dataSize -gt 1932735283) { $issues += "DATA_SECTION_TOO_LARGE" }
    if (-not $isBigObj -and $sectionCount -gt 32767) { $issues += "NEEDS_BIGOBJ_FLAG" }
    
    $entry = [PSCustomObject]@{
        File         = $obj.Name
        SizeKB       = [math]::Round($obj.Length / 1KB, 2)
        Sections     = $sectionCount
        Symbols      = $symbolCount
        TextSizeKB   = [math]::Round($textSize / 1KB, 2)
        DataSizeKB   = [math]::Round($dataSize / 1KB, 2)
        RDataSizeKB  = [math]::Round($rdataSize / 1KB, 2)
        BigObjFlag   = $isBigObj
        Issues       = ($issues -join ", ")
    }
    
    $report += $entry
    
    # Display entry
    $entry | Format-List
}

Write-Host "`n" + ("=" * 80)
Write-Host "📊 SUMMARY TABLE" -ForegroundColor Cyan
$report | Format-Table -AutoSize

# Save JSON report
$jsonPath = Join-Path $ObjDir "forensics_report.json"
$report | ConvertTo-Json -Depth 3 | Out-File $jsonPath
Write-Host "`n✅ Report saved to: $jsonPath" -ForegroundColor Green

# Identify worst offender
$worst = $report | Sort-Object { $_.TextSizeKB + $_.DataSizeKB } -Descending | Select-Object -First 1
if ($worst) {
    Write-Host "`n🎯 PRIORITY TARGET: $($worst.File)" -ForegroundColor Red
    Write-Host "   Sections: $($worst.Sections), Symbols: $($worst.Symbols)" -ForegroundColor Yellow
    if ($worst.Issues) {
        Write-Host "   Issues: $($worst.Issues)" -ForegroundColor Magenta
    }
}

# Now analyze the LARGE C++ objects too
Write-Host "`n" + ("=" * 80)
Write-Host "🔍 Analyzing Large C++ Objects (Top 5)" -ForegroundColor Cyan

$largeObjs = Get-ChildItem "$ObjDir\*.obj" | Sort-Object Length -Descending | Select-Object -First 5
foreach ($obj in $largeObjs) {
    Write-Host "`n📦 $($obj.Name) - $([math]::Round($obj.Length/1MB, 2)) MB" -ForegroundColor Yellow
    
    $dump = & $dumpbin /SUMMARY $obj.FullName 2>&1 | Out-String
    $sectionMatches = [regex]::Matches($dump, "SECTION HEADER")
    $sectionCount = $sectionMatches.Count
    
    Write-Host "   Sections: $sectionCount" -ForegroundColor Gray
    
    if ($sectionCount -gt 65535) {
        Write-Host "   ⚠️  SECTION OVERFLOW DETECTED!" -ForegroundColor Red
    }
}

Write-Host "`n✅ Forensics Complete" -ForegroundColor Green
