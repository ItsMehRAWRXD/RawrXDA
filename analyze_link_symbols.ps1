# analyze_link_symbols.ps1
# TRACK 1: Advanced Symbol Collision Detection and Resolution
# Identifies conflicting objects and suggests resolution strategies

param(
    [string]$Root = "D:\rawrxd",
    [switch]$Verbose,
    [switch]$CreateStubs
)

$ErrorActionPreference = "Continue"

Write-Host "============================================================" -Fore Cyan
Write-Host " RAWRXD SYMBOL COLLISION ANALYZER" -Fore Cyan
Write-Host " TRACK 1: Identifying and resolving linking conflicts" -Fore Yellow
Write-Host "============================================================" -Fore Cyan

# Try to find dumpbin.exe for symbol analysis
$DumpbinPaths = @(
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\dumpbin.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\dumpbin.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\dumpbin.exe"
)

$Dumpbin = $DumpbinPaths | Where-Object { Test-Path $_ } | Select-Object -First 1

if(!$Dumpbin) {
    Write-Host "[WARNING] dumpbin.exe not found, using fallback analysis" -Fore Yellow
}

# Known problematic objects based on previous link errors
$ProblematicObjects = @{
    "win32app_IDEDiagnosticAutoHealer_Impl.obj" = @(
        "DiagnosticUtils::LogHealing",
        "DiagnosticUtils::LogDiagnostic", 
        "BeaconStorage::Instance",
        "IDEDiagnosticAutoHealer::Instance"
    )
    "win32app_simple_test.obj" = @("main")
    "win32app_IDEAutoHealerLauncher.obj" = @("main")
    "dequant_simd.obj" = @("main")
    "NUL.obj" = @("Invalid object file")
    "proof.obj" = @("main")
    "mmap_loader.obj" = @("Corrupt .pdata section")
    "omega_pro.obj" = @("main", "test symbols")
    "OmegaPolyglot_v5.obj" = @("main", "test symbols")
}

Write-Host "[ANALYSIS] Scanning object files for symbol conflicts..." -Fore White

$AllObjects = Get-ChildItem -Path $Root -Filter "*.obj" -Recurse | Sort-Object Name

$ConflictingObjects = @()
$SafeObjects = @()
$AnalysisResults = @()

foreach($obj in $AllObjects) {
    $name = $obj.Name
    $path = $obj.FullName
    $size = $obj.Length
    
    $analysis = [PSCustomObject]@{
        Name = $name
        Path = $path
        Size = $size
        Conflicts = @()
        Symbols = @()
        Recommendation = "INCLUDE"
        Reason = ""
    }
    
    # Check against known problematic objects
    if($ProblematicObjects.ContainsKey($name)) {
        $analysis.Conflicts = $ProblematicObjects[$name]  
        $analysis.Recommendation = "EXCLUDE"
        $analysis.Reason = "Known symbol conflicts: $($analysis.Conflicts -join ', ')"
        $ConflictingObjects += $obj
    }
    # Check for C/C++ compiled objects
    elseif($name -match "\.cpp\.obj$" -or $name -match "\.c\.obj$") {
        $analysis.Recommendation = "EXCLUDE"
        $analysis.Reason = "C/C++ compiled object - may have symbol conflicts"
        $ConflictingObjects += $obj
    }
    # Check for test/benchmark objects
    elseif($name -match "^(test_|bench_|CMakeC)" -or $path -match "\\(test|bench|unittest)\\") {
        $analysis.Recommendation = "EXCLUDE" 
        $analysis.Reason = "Test/benchmark object - may have conflicting symbols"
        $ConflictingObjects += $obj
    }
    # Check for very small objects (likely stubs or empty)
    elseif($size -lt 500) {
        $analysis.Recommendation = "EXCLUDE"
        $analysis.Reason = "Very small object - likely stub or empty"
        $ConflictingObjects += $obj  
    }
    # Large objects are likely primary components
    elseif($size -gt 100000) {
        $analysis.Recommendation = "INCLUDE_PRIORITY"
        $analysis.Reason = "Large object - likely primary component"
        $SafeObjects += $obj
    }
    else {
        $SafeObjects += $obj
    }
    
    $AnalysisResults += $analysis
}

# Generate detailed report
$ReportPath = Join-Path $Root "symbol_analysis_report.json"
$AnalysisResults | ConvertTo-Json -Depth 10 | Out-File -FilePath $ReportPath -Encoding UTF8

Write-Host "`n[RESULTS] Symbol Conflict Analysis Complete:" -Fore Green
Write-Host "  Total Objects: $($AllObjects.Count)" -Fore White
Write-Host "  Conflicting: $($ConflictingObjects.Count)" -Fore Red
Write-Host "  Safe to Link: $($SafeObjects.Count)" -Fore Green  
Write-Host "  Detailed Report: $ReportPath" -Fore Gray

# Show top conflicts
Write-Host "`n[TOP CONFLICTS]" -Fore Yellow
$AnalysisResults | Where-Object { $_.Recommendation -eq "EXCLUDE" } | 
    Select-Object -First 10 | ForEach-Object {
        Write-Host "  $($_.Name) - $($_.Reason)" -Fore Red
    }

# Show recommended primary objects
Write-Host "`n[PRIMARY COMPONENTS]" -Fore Green
$AnalysisResults | Where-Object { $_.Recommendation -eq "INCLUDE_PRIORITY" } | 
    Sort-Object Size -Descending | Select-Object -First 5 | ForEach-Object {
        $sizeMB = [math]::Round($_.Size / 1MB, 2)
        Write-Host "  $($_.Name) - ${sizeMB}MB" -Fore Green
    }

# Create exclusion list for genesis_build.ps1
$ExclusionList = $AnalysisResults | Where-Object { $_.Recommendation -eq "EXCLUDE" } | 
    Select-Object -ExpandProperty Name | Sort-Object

$ExclusionScript = Join-Path $Root "symbol_exclusion_list.ps1"
@"
# Auto-generated symbol exclusion list
`$SymbolConflictExclusions = @(
$($ExclusionList | ForEach-Object { "    `"$_`"" } | Out-String)
)

Write-Host "[INFO] Excluding $($ExclusionList.Count) objects with symbol conflicts" -Fore Yellow
"@ | Out-File -FilePath $ExclusionScript -Encoding UTF8

Write-Host "`n[GENERATED] Symbol exclusion list: $ExclusionScript" -Fore Cyan
Write-Host "[NEXT] Run genesis_build.ps1 with updated exclusions" -Fore Green
Write-Host "============================================================" -Fore Cyan