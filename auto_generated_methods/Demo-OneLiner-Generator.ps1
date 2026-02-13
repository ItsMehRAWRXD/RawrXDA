<#
.SYNOPSIS
    Demonstration of RawrXD OMEGA-X One-Liner Generator System
    
.DESCRIPTION
    Shows advanced usage of the one-liner generator with various configurations
    including shellcode injection, hardened mode, and custom payloads.
#>

# Import the generator module
Import-Module "$PSScriptRoot\RawrXD-OneLiner-Generator.ps1" -Force

Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║          RawrXD OMEGA-X: ONE-LINER GENERATOR DEMO            ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# --- DEMO 1: Basic One-Liner ---
Write-Host "🧪 DEMO 1: Basic One-Liner Generation" -ForegroundColor Yellow
$basicResult = Generate-OneLiner -Tasks @("create-dir", "write-file", "execute", "telemetry")
Write-Host "✅ Generated Basic One-Liner:" -ForegroundColor Green
Write-Host $basicResult.OneLiner -ForegroundColor Gray
Write-Host ""

# --- DEMO 2: Advanced Agent with Shellcode ---
Write-Host "🧪 DEMO 2: Advanced Agent with Shellcode Injection" -ForegroundColor Yellow
$advancedResult = Generate-OneLiner -Tasks @("create-dir", "write-file", "execute", "loop", "self-mutate", "beacon") -EmitShellcode
Write-Host "✅ Generated Advanced One-Liner:" -ForegroundColor Green
Write-Host $advancedResult.OneLiner.Substring(0, 200) + "..." -ForegroundColor Gray
Write-Host ""

# --- DEMO 3: Hardened System ---
Write-Host "🧪 DEMO 3: Hardened System One-Liner" -ForegroundColor Yellow
$hardenedResult = Generate-OneLiner -Tasks @("create-dir", "hardware-key", "sandbox-check", "memory-protect", "execute", "integrity-check") -Hardened -MutationLevel 3
Write-Host "✅ Generated Hardened One-Liner:" -ForegroundColor Green
Write-Host $hardenedResult.OneLiner.Substring(0, 200) + "..." -ForegroundColor Gray
Write-Host ""

# --- DEMO 4: Analysis ---
Write-Host "🧪 DEMO 4: One-Liner Analysis" -ForegroundColor Yellow
$analysis = Invoke-OneLinerAnalysis -OneLiner $basicResult.OneLiner
Write-Host "📊 Analysis Results:" -ForegroundColor Cyan
$analysis.GetEnumerator() | ForEach-Object {
    Write-Host "  $($_.Key): $($_.Value)" -ForegroundColor White
}
Write-Host ""

# --- DEMO 5: Template Usage ---
Write-Host "🧪 DEMO 5: Available Templates" -ForegroundColor Yellow
$templates = Get-OneLinerTemplates
Write-Host "📋 Available Templates:" -ForegroundColor Cyan
$templates.GetEnumerator() | ForEach-Object {
    Write-Host "  $($_.Key):" -ForegroundColor White
    Write-Host "    Tasks: $($_.Value -join ', ')" -ForegroundColor Gray
}
Write-Host ""

# --- DEMO 6: Custom Template Generation ---
Write-Host "🧪 DEMO 6: Custom Template Generation" -ForegroundColor Yellow
$customTasks = @("create-dir", "execute", "telemetry", "self-mutate")
$customResult = Generate-OneLiner -Tasks $customTasks -MutationLevel 2
Write-Host "✅ Generated Custom One-Liner:" -ForegroundColor Green
Write-Host $customResult.OneLiner.Substring(0, 150) + "..." -ForegroundColor Gray
Write-Host ""

# --- SUMMARY ---
Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║                      DEMO COMPLETE                           ║" -ForegroundColor Green
Write-Host "╚════════════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host ""
Write-Host "📊 SUMMARY:" -ForegroundColor Cyan
Write-Host "  ✅ Basic One-Liner: Generated successfully" -ForegroundColor Green
Write-Host "  ✅ Advanced Agent: Shellcode injection enabled" -ForegroundColor Green
Write-Host "  ✅ Hardened System: Anti-analysis features active" -ForegroundColor Green
Write-Host "  ✅ Analysis: Comprehensive metrics available" -ForegroundColor Green
Write-Host "  ✅ Templates: Multiple pre-built configurations" -ForegroundColor Green
Write-Host "  ✅ Custom Generation: Flexible task-based creation" -ForegroundColor Green
Write-Host ""
Write-Host "🎯 NEXT STEPS:" -ForegroundColor Cyan
Write-Host "  1. Use Generate-OneLiner with your own task lists" -ForegroundColor White
Write-Host "  2. Experiment with -Hardened and -EmitShellcode switches" -ForegroundColor White
Write-Host "  3. Analyze generated one-liners with Invoke-OneLinerAnalysis" -ForegroundColor White
Write-Host "  4. Integrate with RawrXD OMEGA-1 deployment system" -ForegroundColor White
Write-Host ""

# Save demo results to file
$demoResults = @{
    BasicOneLiner = $basicResult.OneLiner
    AdvancedOneLiner = $advancedResult.OneLiner
    HardenedOneLiner = $hardenedResult.OneLiner
    Analysis = $analysis
    GeneratedAt = Get-Date
}

$resultsPath = "$PSScriptRoot\one-liner-demo-results.json"
$demoResults | ConvertTo-Json -Depth 5 | Set-Content $resultsPath
Write-Host "📁 Demo results saved to: $resultsPath" -ForegroundColor Cyan