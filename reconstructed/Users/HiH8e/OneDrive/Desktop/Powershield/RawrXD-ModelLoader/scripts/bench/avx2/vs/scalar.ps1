# Benchmark script for v0.3.0 AVX2/Q4_0 performance
# Measures tok/s throughput and compares Scalar vs AVX2 builds

param(
    [string]$ScalarBinary = "build-scalar\bin-msvc\Release\RawrXD-QtShell.exe",
    [string]$Avx2Binary = "build-avx2\bin-msvc\Release\RawrXD-QtShell.exe",
    [int]$Tokens = 128,
    [int]$Warmup = 3
)

Write-Host ""
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "   AVX2 vs Scalar Benchmark - v0.3.0" -ForegroundColor White
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host ""

function Test-Binary {
    param([string]$Path)
    if (-not (Test-Path $Path)) {
        Write-Host "❌ Binary not found: $Path" -ForegroundColor Red
        return $false
    }
    return $true
}

function Get-BinarySize {
    param([string]$Path)
    $sizeKB = (Get-Item $Path).Length / 1KB
    return [math]::Round($sizeKB, 1)
}

function Measure-Throughput {
    param(
        [string]$BinaryPath,
        [int]$Tokens,
        [int]$Warmup
    )
    
    Write-Host "🔥 Warmup ($Warmup runs)..." -ForegroundColor Yellow
    for ($i = 0; $i -lt $Warmup; $i++) {
        & $BinaryPath --benchmark-gqa --tokens 32 | Out-Null
    }
    
    Write-Host "⏱️  Measuring throughput ($Tokens tokens)..." -ForegroundColor Yellow
    $elapsed = Measure-Command {
        & $BinaryPath --benchmark-gqa --tokens $Tokens | Out-Null
    }
    
    $tokPerSec = $Tokens / $elapsed.TotalSeconds
    return [math]::Round($tokPerSec, 1)
}

# Check binaries exist
$scalarOk = Test-Binary $ScalarBinary
$avx2Ok = Test-Binary $Avx2Binary

if (-not $scalarOk -and -not $avx2Ok) {
    Write-Host "❌ No binaries found. Build first with:" -ForegroundColor Red
    Write-Host "   cmake --preset win-rel-scalar && cmake --build build-scalar" -ForegroundColor Gray
    Write-Host "   cmake --preset win-rel-avx2 && cmake --build build-avx2" -ForegroundColor Gray
    exit 1
}

# Benchmark results
$results = @()

if ($scalarOk) {
    Write-Host "📊 Testing Scalar Build..." -ForegroundColor Cyan
    $scalarSize = Get-BinarySize $ScalarBinary
    $scalarSpeed = Measure-Throughput -BinaryPath $ScalarBinary -Tokens $Tokens -Warmup $Warmup
    
    $results += [PSCustomObject]@{
        Build = "Scalar"
        Size = $scalarSize
        Speed = $scalarSpeed
    }
    
    Write-Host "   Size: $scalarSize KB" -ForegroundColor White
    Write-Host "   Speed: $scalarSpeed tok/s" -ForegroundColor Green
    Write-Host ""
}

if ($avx2Ok) {
    Write-Host "📊 Testing AVX2 Build..." -ForegroundColor Cyan
    $avx2Size = Get-BinarySize $Avx2Binary
    $avx2Speed = Measure-Throughput -BinaryPath $Avx2Binary -Tokens $Tokens -Warmup $Warmup
    
    $results += [PSCustomObject]@{
        Build = "AVX2"
        Size = $avx2Size
        Speed = $avx2Speed
    }
    
    Write-Host "   Size: $avx2Size KB" -ForegroundColor White
    Write-Host "   Speed: $avx2Speed tok/s" -ForegroundColor Green
    Write-Host ""
}

# Comparison
if ($scalarOk -and $avx2Ok) {
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "   Comparison" -ForegroundColor White
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host ""
    
    $speedup = $avx2Speed / $scalarSpeed
    $sizeDelta = $avx2Size - $scalarSize
    
    Write-Host "   Speedup: $([math]::Round($speedup, 2))x" -ForegroundColor $(if ($speedup -ge 1.8) { "Green" } else { "Yellow" })
    Write-Host "   Size Delta: +$sizeDelta KB" -ForegroundColor White
    Write-Host ""
    
    # Gate check
    if ($speedup -ge 1.8) {
        Write-Host "✅ GATE PASSED: AVX2 is $([math]::Round($speedup, 2))x faster (≥1.8x required)" -ForegroundColor Green
    } else {
        Write-Host "⚠️  GATE WARNING: AVX2 is only $([math]::Round($speedup, 2))x faster (<1.8x)" -ForegroundColor Yellow
    }
    
    if ($avx2Size -lt 400) {
        Write-Host "✅ SIZE CHECK: AVX2 binary is $avx2Size KB (<400 KB target)" -ForegroundColor Green
    } else {
        Write-Host "⚠️  SIZE WARNING: AVX2 binary is $avx2Size KB (>400 KB target)" -ForegroundColor Yellow
    }
    Write-Host ""
}

# Results table
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
$results | Format-Table -AutoSize
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host ""
