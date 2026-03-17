$ErrorActionPreference = "Continue"
$script:passed = 0; $script:failed = 0; $script:warnings = 0
function Test-Pass($n) { $script:passed++; Write-Host "  [PASS] $n" -ForegroundColor Green }
function Test-Fail($n,$d) { $script:failed++; Write-Host "  [FAIL] $n -- $d" -ForegroundColor Red }
function Test-Warn($n,$d) { $script:warnings++; Write-Host "  [WARN] $n -- $d" -ForegroundColor Yellow }

