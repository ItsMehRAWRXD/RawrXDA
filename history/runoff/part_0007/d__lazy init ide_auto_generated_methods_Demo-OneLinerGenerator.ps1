#Requires -Version 7.4

<#
.SYNOPSIS
    RawrXD One-Liner Generator - Interactive Demonstration
.DESCRIPTION
    Demonstrates all capabilities of the OMEGA-1 one-liner generation system
#>

# Import the generator module
$GeneratorPath = Join-Path $PSScriptRoot "RawrXD-OneLiner-Generator.ps1"
if (Test-Path $GeneratorPath) {
    . $GeneratorPath
} else {
    Write-Error "Generator module not found at: $GeneratorPath"
    exit 1
}

Write-Host "`n╔═══════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  RawrXD OMEGA-1 One-Liner Generator - Interactive Demo           ║" -ForegroundColor Cyan
Write-Host "║  Production-Tier Polymorphic Code Emitter                         ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# Demo 1: Basic One-Liner
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
Write-Host "🔹 DEMO 1: Basic One-Liner (Simple Task Chain)" -ForegroundColor Green
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Gray

$demo1 = Generate-OneLiner -Tasks @("create-dir","write-file","execute") -Verbose
Write-Host "`n✓ Basic one-liner generated:" -ForegroundColor Green
Write-Host "  Tasks: $($demo1.TaskCount)" -ForegroundColor Cyan
Write-Host "  Size: $($demo1.EncodedSize) bytes" -ForegroundColor Cyan
Write-Host "  Path: $($demo1.ScriptPath)`n" -ForegroundColor Cyan

Start-Sleep -Seconds 2

# Demo 2: Advanced One-Liner with Encoding
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
Write-Host "🔹 DEMO 2: Advanced One-Liner (Base64 Encoded)" -ForegroundColor Yellow
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Gray

$demo2 = Generate-OneLiner -Tasks @("create-dir","write-file","execute","loop","cpu-info","memory-info") -EncodeBase64 -Verbose
Write-Host "`n✓ Advanced one-liner generated:" -ForegroundColor Green
Write-Host "  Tasks: $($demo2.TaskCount)" -ForegroundColor Cyan
Write-Host "  Encoded Size: $($demo2.EncodedSize) bytes" -ForegroundColor Cyan
Write-Host "  Encoded: Yes`n" -ForegroundColor Cyan

Start-Sleep -Seconds 2

# Demo 3: Hardened Mode
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
Write-Host "🔹 DEMO 3: Hardened One-Liner (Anti-Debug + Self-Mutation)" -ForegroundColor Magenta
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Gray

$demo3 = Generate-OneLiner -Tasks @("create-dir","write-file","self-mutate","telemetry") -Hardened -EncodeBase64 -Verbose
Write-Host "`n✓ Hardened one-liner generated:" -ForegroundColor Green
Write-Host "  Tasks: $($demo3.TaskCount)" -ForegroundColor Cyan
Write-Host "  Hardened: $($demo3.Hardened)" -ForegroundColor Cyan
Write-Host "  Anti-Debug: Enabled" -ForegroundColor Magenta
Write-Host "  Hardware-Keyed: Yes`n" -ForegroundColor Magenta

Start-Sleep -Seconds 2

# Demo 4: Custom Tasks
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
Write-Host "🔹 DEMO 4: Custom Task Integration" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Gray

$customTasks = @{
    "scan-ports" = "1..100 | ForEach-Object { Test-NetConnection -ComputerName localhost -Port `$_ -InformationLevel Quiet }"
    "gather-env" = "`$env:USERNAME + '@' + `$env:COMPUTERNAME | Out-File 'D:\RawrXD\env.txt'"
}

$demo4 = Generate-OneLiner -Tasks @("create-dir","scan-ports","gather-env") -CustomTasks $customTasks -Verbose
Write-Host "`n✓ Custom task one-liner generated:" -ForegroundColor Green
Write-Host "  Custom Tasks: $($customTasks.Count)" -ForegroundColor Cyan
Write-Host "  Total Tasks: $($demo4.TaskCount)`n" -ForegroundColor Cyan

Start-Sleep -Seconds 2

# Demo 5: OmegaX Hardware-Keyed Payload
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
Write-Host "🔹 DEMO 5: OmegaX Hardware-Keyed Payload (Polymorphic JIT)" -ForegroundColor Red
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Gray

$demo5 = New-OmegaXPayload -Intent @("synapse-check","jit-init") -Hardened -Verbose
Write-Host "`n✓ OmegaX payload generated:" -ForegroundColor Green
Write-Host "  Complexity: $($demo5.Complexity)" -ForegroundColor Cyan
Write-Host "  Hardware-Bound: Yes" -ForegroundColor Red
Write-Host "  Intent: $($demo5.Intent -join ', ')`n" -ForegroundColor Cyan

Start-Sleep -Seconds 2

# Demo 6: Interactive Generator
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
Write-Host "🔹 DEMO 6: Interactive Generator (All Modes)" -ForegroundColor White
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Gray

$modes = @('Basic','Advanced','Hardened')
foreach ($mode in $modes) {
    Write-Host "  Generating $mode mode..." -ForegroundColor Gray
    $result = Invoke-OneLinerGenerator -Mode $mode -ErrorAction SilentlyContinue
    if ($result) {
        Write-Host "  ✓ $mode mode completed" -ForegroundColor Green
    }
    Start-Sleep -Milliseconds 500
}

Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
Write-Host "✨ DEMONSTRATION COMPLETE" -ForegroundColor Green
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Gray

# Summary
Write-Host "📊 Summary:" -ForegroundColor Cyan
Write-Host "  ✓ 6 different generation modes demonstrated" -ForegroundColor Green
Write-Host "  ✓ Basic, Advanced, Hardened, Custom, OmegaX, Interactive" -ForegroundColor Green
Write-Host "  ✓ All one-liners saved to $env:TEMP\RawrXD_OneLiners\" -ForegroundColor Green
Write-Host ""

# Show available commands
Write-Host "🎯 Available Commands:" -ForegroundColor Yellow
Write-Host "  1. Generate-OneLiner -Tasks @('task1','task2') [-Hardened] [-EncodeBase64]" -ForegroundColor Gray
Write-Host "  2. New-OmegaXPayload -Intent @('synapse-check','jit-init') [-Hardened]" -ForegroundColor Gray
Write-Host "  3. Invoke-OneLinerGenerator -Mode [Basic|Advanced|Hardened|Shellcode|OmegaX]" -ForegroundColor Gray
Write-Host ""

Write-Host "✨ Ready for production deployment!" -ForegroundColor Green
Write-Host ""
