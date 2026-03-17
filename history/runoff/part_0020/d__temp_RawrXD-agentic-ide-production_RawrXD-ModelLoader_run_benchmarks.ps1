# RawrXD Benchmark Suite Runner - PowerShell Edition
# Complete utility to build and run all benchmarks

param(
    [string]$ModelPath = "models/ministral-3b-instruct-v0.3-Q4_K_M.gguf",
    [string]$BuildDir = "build",
    [switch]$ListOnly,
    [switch]$Verbose,
    [switch]$Rebuild,
    [int]$TimeoutMin = 5
)

$ErrorActionPreference = "Stop"

# ============================================================================
# CONFIGURATION
# ============================================================================

$BinDir = Join-Path $BuildDir "bin\Release"
$SourceDir = Get-Location

function Write-Header {
    param([string]$Text)
    Write-Host ""
    Write-Host "=" * 70 -ForegroundColor Cyan
    Write-Host $Text -ForegroundColor Cyan
    Write-Host "=" * 70 -ForegroundColor Cyan
}

function Write-Success {
    param([string]$Text)
    Write-Host "✓ $Text" -ForegroundColor Green
}

function Write-Error {
    param([string]$Text)
    Write-Host "✗ $Text" -ForegroundColor Red
}

function Write-Info {
    param([string]$Text)
    Write-Host "ℹ $Text" -ForegroundColor Yellow
}

# ============================================================================
# MAIN FUNCTIONS
# ============================================================================

function Find-Benchmarks {
    if (-not (Test-Path $BinDir)) {
        Write-Error "Build directory not found: $BinDir"
        return @()
    }
    
    $benchmarks = @()
    
    # Find all *benchmark*.exe files
    $benchmarks += @(Get-ChildItem -Path $BinDir -Filter "*benchmark*.exe" -ErrorAction SilentlyContinue)
    
    # Find other *bench*.exe files
    $benchmarks += @(Get-ChildItem -Path $BinDir -Filter "*bench*.exe" -ErrorAction SilentlyContinue | 
                     Where-Object { $_.Name -notmatch "benchmark" })
    
    return $benchmarks | Sort-Object Name
}

function Build-Project {
    Write-Header "BUILDING PROJECT"
    
    if (-not (Test-Path "CMakeLists.txt")) {
        Write-Error "CMakeLists.txt not found in current directory"
        return $false
    }
    
    # Create build directory
    if (-not (Test-Path $BuildDir)) {
        Write-Info "Creating build directory..."
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
    }
    
    # Configure CMake
    Write-Info "Configuring CMake..."
    Push-Location $BuildDir
    try {
        cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release ..
        if ($LASTEXITCODE -ne 0) {
            Write-Error "CMake configuration failed"
            return $false
        }
        
        # Build
        Write-Info "Building benchmarks..."
        cmake --build . --config Release --target benchmark_completions
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Build failed"
            return $false
        }
        
        Write-Success "Build completed"
        return $true
    }
    finally {
        Pop-Location
    }
}

function Run-Benchmark {
    param(
        [System.IO.FileInfo]$Exe,
        [string]$ModelPath,
        [hashtable]$Results
    )
    
    Write-Host ""
    Write-Host "─" * 70 -ForegroundColor DarkGray
    Write-Host "Running: $($Exe.Name)" -ForegroundColor Cyan
    Write-Host "─" * 70 -ForegroundColor DarkGray
    
    $startTime = Get-Date
    $timeout = New-TimeSpan -Minutes $TimeoutMin
    
    try {
        # Build command
        $cmd = @($Exe.FullName)
        if ($ModelPath -and (Test-Path $ModelPath)) {
            $cmd += @("-m", $ModelPath)
        }
        
        # Run with timeout
        $process = Start-Process -FilePath $cmd[0] `
                                  -ArgumentList $cmd[1..($cmd.Length-1)] `
                                  -NoNewWindow `
                                  -RedirectStandardOutput ".\temp_stdout.txt" `
                                  -RedirectStandardError ".\temp_stderr.txt" `
                                  -PassThru
        
        $process | Wait-Process -Timeout $TimeoutMin.TotalSeconds -ErrorAction Stop
        
        if ($process.ExitCode -ne 0) {
            Write-Error "$($Exe.Name) exited with code $($process.ExitCode)"
            $Results[$Exe.Name] = @{
                Success = $false
                ExitCode = $process.ExitCode
                Error = "Non-zero exit code"
            }
        } else {
            Write-Success "$($Exe.Name) completed successfully"
            $Results[$Exe.Name] = @{
                Success = $true
                ExitCode = 0
            }
        }
        
        # Print output
        if (Test-Path ".\temp_stdout.txt") {
            $output = Get-Content ".\temp_stdout.txt"
            Write-Host $output
            Remove-Item ".\temp_stdout.txt" -Force
        }
        
        if ((Test-Path ".\temp_stderr.txt") -and $Verbose) {
            $errors = Get-Content ".\temp_stderr.txt"
            if ($errors) {
                Write-Host "STDERR:" -ForegroundColor DarkYellow
                Write-Host $errors
            }
            Remove-Item ".\temp_stderr.txt" -Force
        }
        
    }
    catch [System.TimeoutException] {
        Write-Error "Timeout: $($Exe.Name) exceeded $TimeoutMin minutes"
        $Results[$Exe.Name] = @{
            Success = $false
            Error = "Timeout"
        }
    }
    catch {
        Write-Error "Error running $($Exe.Name): $_"
        $Results[$Exe.Name] = @{
            Success = $false
            Error = $_.Exception.Message
        }
    }
}

function Print-Summary {
    param([hashtable]$Results)
    
    Write-Header "BENCHMARK SUITE SUMMARY"
    
    $passed = ($Results.Values | Where-Object { $_.Success } | Measure-Object).Count
    $total = $Results.Count
    
    Write-Host ""
    Write-Host "Total Benchmarks:  $total" -ForegroundColor Cyan
    Write-Host "Passed:            $passed" -ForegroundColor Green
    Write-Host "Failed:            $($total - $passed)" -ForegroundColor Red
    Write-Host ""
    
    Write-Host "Results:" -ForegroundColor Cyan
    Write-Host ""
    Write-Host ("{0,-45} {1,-10}" -f "Benchmark", "Status") -ForegroundColor Cyan
    Write-Host "-" * 70 -ForegroundColor DarkGray
    
    foreach ($name in $Results.Keys | Sort-Object) {
        $result = $Results[$name]
        $status = if ($result.Success) { "✅ PASS" } else { "❌ FAIL" }
        Write-Host ("{0,-45} {1,-10}" -f $name, $status)
    }
    
    Write-Host ""
    Write-Host "-" * 70 -ForegroundColor DarkGray
    Write-Host ""
    
    if ($passed -eq $total) {
        Write-Success "ALL BENCHMARKS PASSED!"
    }
    elseif ($passed -ge ($total * 0.75)) {
        Write-Info "MOST BENCHMARKS PASSED - SOME ISSUES"
    }
    else {
        Write-Error "MULTIPLE BENCHMARK FAILURES"
    }
    
    Write-Host ""
}

# ============================================================================
# MAIN EXECUTION
# ============================================================================

Write-Header "RawrXD BENCHMARK SUITE RUNNER"

# Check if model exists
if ($ModelPath) {
    if (Test-Path $ModelPath) {
        Write-Success "Model found: $ModelPath"
    } else {
        Write-Error "Model not found: $ModelPath"
        Write-Info "Run without -ModelPath to use default locations"
    }
}

# Find benchmarks
$benchmarks = Find-Benchmarks

if ($ListOnly) {
    Write-Host ""
    Write-Host "Available benchmarks:" -ForegroundColor Cyan
    Write-Host ""
    if ($benchmarks.Count -eq 0) {
        Write-Error "No benchmarks found in $BinDir"
        exit 1
    }
    foreach ($bench in $benchmarks) {
        Write-Host "  • $($bench.Name)" -ForegroundColor Green
    }
    Write-Host ""
    exit 0
}

if ($benchmarks.Count -eq 0) {
    Write-Error "No benchmarks found in $BinDir"
    
    Write-Host ""
    Write-Host "Available options:" -ForegroundColor Yellow
    Write-Host "  1. Rebuild the project: .\run_benchmarks.ps1 -Rebuild"
    Write-Host "  2. Manually build: cmake --build $BuildDir --config Release"
    Write-Host "  3. List benchmarks: .\run_benchmarks.ps1 -ListOnly"
    Write-Host ""
    exit 1
}

Write-Host ""
Write-Host "Found $($benchmarks.Count) benchmark(s):" -ForegroundColor Green
Write-Host ""
for ($i = 0; $i -lt $benchmarks.Count; $i++) {
    Write-Host "  $($i + 1). $($benchmarks[$i].Name)" -ForegroundColor Green
}

# Ask to rebuild if requested
if ($Rebuild) {
    if (-not (Build-Project)) {
        exit 1
    }
}

# Run benchmarks
Write-Header "RUNNING BENCHMARK SUITE"

$results = @{}
foreach ($bench in $benchmarks) {
    Run-Benchmark -Exe $bench -ModelPath $ModelPath -Results $results
}

# Print summary
Print-Summary $results

# Export JSON results
$jsonOutput = @{
    Timestamp = (Get-Date).ToIso8601String()
    TotalBenchmarks = $results.Count
    Passed = ($results.Values | Where-Object { $_.Success } | Measure-Object).Count
    Results = $results
}

$jsonPath = "benchmark_suite_results.json"
$jsonOutput | ConvertTo-Json | Out-File $jsonPath -Encoding UTF8
Write-Success "Results exported to: $jsonPath"

Write-Host ""

# Exit with appropriate code
$allPassed = $results.Values | Where-Object { $_.Success } | Measure-Object
if ($allPassed.Count -eq $results.Count) {
    exit 0
} else {
    exit 1
}
