#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Desktop Copilot Demo - Quick showcase of capabilities

.DESCRIPTION
    Demonstrates what desktop copilots can do and how they work
#>

Set-StrictMode -Version Latest

Write-Host @"

╔════════════════════════════════════════════════════════════════════════════╗
║                                                                            ║
║              🤖 DESKTOP COPILOT SYSTEM - DEMONSTRATION 🤖                 ║
║                                                                            ║
║              Build Custom Taskbar Assistants for ANY Purpose              ║
║                                                                            ║
╚════════════════════════════════════════════════════════════════════════════╝

"@ -ForegroundColor Cyan

Write-Host "✨ What You Can Build:" -ForegroundColor Yellow
Write-Host ""
Write-Host "  🎮 Gaming Copilot" -ForegroundColor Green
Write-Host "     → Close background apps for max FPS"
Write-Host "     → Monitor temps to prevent overheating"
Write-Host "     → Auto-optimize network for low latency"
Write-Host ""

Write-Host "  📁 File Manager Copilot" -ForegroundColor Green
Write-Host "     → Auto-organize downloads by type"
Write-Host "     → Find and delete duplicate files"
Write-Host "     → Clean temp files automatically"
Write-Host ""

Write-Host "  🔐 Security Copilot" -ForegroundColor Green
Write-Host "     → Monitor for suspicious processes"
Write-Host "     → Track unusual network connections"
Write-Host "     → Alert on security threats"
Write-Host ""

Write-Host "  💾 Backup Copilot" -ForegroundColor Green
Write-Host "     → Schedule automated backups"
Write-Host "     → Verify backup integrity"
Write-Host "     → Quick file restore"
Write-Host ""

Write-Host "  💪 Health Copilot" -ForegroundColor Green
Write-Host "     → Remind to take breaks (20-20-20 rule)"
Write-Host "     → Hydration reminders"
Write-Host "     → Track screen time"
Write-Host ""

Write-Host "  🌐 Network Copilot" -ForegroundColor Green
Write-Host "     → Monitor bandwidth usage"
Write-Host "     → Track data caps"
Write-Host "     → Speed test and diagnostics"
Write-Host ""

Write-Host "`n" + "="*80 -ForegroundColor Gray

Write-Host "`n🔑 Key Features:" -ForegroundColor Yellow
Write-Host ""
Write-Host "  ✅ Lives in system tray (taskbar) - always accessible"
Write-Host "  ✅ Global hotkey (Ctrl+Shift+A) to open anywhere"
Write-Host "  ✅ Full system access (files, processes, network, registry)"
Write-Host "  ✅ Background monitoring and smart alerts"
Write-Host "  ✅ Customizable for ANY purpose (not just coding!)"
Write-Host "  ✅ Can diagnose and FIX problems automatically"
Write-Host ""

Write-Host "="*80 -ForegroundColor Gray

Write-Host "`n📦 What We Built for You:" -ForegroundColor Yellow
Write-Host ""
Write-Host "  1. desktop_copilot.ps1" -ForegroundColor Cyan
Write-Host "     → Base copilot with system diagnostics & fixes"
Write-Host ""
Write-Host "  2. copilot_generator.ps1" -ForegroundColor Cyan
Write-Host "     → Interactive template generator"
Write-Host "     → 6 pre-built templates (Gaming, Files, Security, etc.)"
Write-Host ""
Write-Host "  3. Complete Documentation" -ForegroundColor Cyan
Write-Host "     → Customization guide (13,000+ words)"
Write-Host "     → Quick reference with code examples"
Write-Host "     → Step-by-step tutorials"
Write-Host ""

Write-Host "="*80 -ForegroundColor Gray

Write-Host "`n🚀 Quick Demo - System Diagnostics:" -ForegroundColor Yellow
Write-Host ""

# CPU Usage
Write-Host "  📊 CPU Usage: " -NoNewline -ForegroundColor Cyan
try {
    $cpu = (Get-WmiObject Win32_Processor).LoadPercentage
    if ($cpu -gt 80) {
        Write-Host "$cpu% ⚠️  HIGH" -ForegroundColor Red
    }
    elseif ($cpu -gt 50) {
        Write-Host "$cpu% ⚡ MODERATE" -ForegroundColor Yellow
    }
    else {
        Write-Host "$cpu% ✅ GOOD" -ForegroundColor Green
    }
}
catch {
    Write-Host "N/A" -ForegroundColor Gray
}

# Memory Usage
Write-Host "  💾 Memory Usage: " -NoNewline -ForegroundColor Cyan
try {
    $os = Get-WmiObject Win32_OperatingSystem
    $memPercent = [Math]::Round((($os.TotalVisibleMemorySize - $os.FreePhysicalMemory) / $os.TotalVisibleMemorySize) * 100, 1)
    if ($memPercent -gt 85) {
        Write-Host "$memPercent% ⚠️  HIGH" -ForegroundColor Red
    }
    elseif ($memPercent -gt 70) {
        Write-Host "$memPercent% ⚡ MODERATE" -ForegroundColor Yellow
    }
    else {
        Write-Host "$memPercent% ✅ GOOD" -ForegroundColor Green
    }
}
catch {
    Write-Host "N/A" -ForegroundColor Gray
}

# Disk Space
Write-Host "  💿 Disk Space (C:): " -NoNewline -ForegroundColor Cyan
try {
    $disk = Get-PSDrive C
    $freeGB = [Math]::Round($disk.Free / 1GB, 1)
    $usedGB = [Math]::Round($disk.Used / 1GB, 1)
    $totalGB = $freeGB + $usedGB
    $usedPercent = [Math]::Round(($usedGB / $totalGB) * 100, 1)
    
    Write-Host "$freeGB GB free " -NoNewline
    if ($usedPercent -gt 90) {
        Write-Host "⚠️  CRITICAL" -ForegroundColor Red
    }
    elseif ($usedPercent -gt 80) {
        Write-Host "⚡ LOW" -ForegroundColor Yellow
    }
    else {
        Write-Host "✅ GOOD" -ForegroundColor Green
    }
}
catch {
    Write-Host "N/A" -ForegroundColor Gray
}

# Network
Write-Host "  🌐 Network: " -NoNewline -ForegroundColor Cyan
try {
    $ping = Test-Connection -ComputerName 8.8.8.8 -Count 1 -Quiet -ErrorAction Stop
    if ($ping) {
        Write-Host "✅ CONNECTED" -ForegroundColor Green
    }
    else {
        Write-Host "❌ DISCONNECTED" -ForegroundColor Red
    }
}
catch {
    Write-Host "❌ DISCONNECTED" -ForegroundColor Red
}

# Top Process
Write-Host "  ⚙️  Top Process: " -NoNewline -ForegroundColor Cyan
try {
    $topProc = Get-Process | Sort-Object CPU -Descending | Select-Object -First 1
    $cpu = [Math]::Round($topProc.CPU, 1)
    $memMB = [Math]::Round($topProc.WorkingSet64 / 1MB, 0)
    Write-Host "$($topProc.ProcessName) (CPU: $cpu`s, RAM: $memMB MB)" -ForegroundColor White
}
catch {
    Write-Host "N/A" -ForegroundColor Gray
}

Write-Host ""
Write-Host "  💡 This is what your copilot can monitor in real-time!" -ForegroundColor Magenta
Write-Host ""

Write-Host "="*80 -ForegroundColor Gray

Write-Host "`n🎯 Try It Now:" -ForegroundColor Yellow
Write-Host ""
Write-Host "  Option 1: Run Base Copilot" -ForegroundColor Cyan
Write-Host "  ─────────────────────────────"
Write-Host "  cd `"D:\lazy init ide\desktop`""
Write-Host "  .\desktop_copilot.ps1"
Write-Host ""

Write-Host "  Option 2: Generate Custom Copilot" -ForegroundColor Cyan
Write-Host "  ──────────────────────────────────"
Write-Host "  cd `"D:\lazy init ide\desktop`""
Write-Host "  .\copilot_generator.ps1"
Write-Host ""

Write-Host "  Option 3: Generate Gaming Copilot" -ForegroundColor Cyan
Write-Host "  ──────────────────────────────────"
Write-Host "  .\copilot_generator.ps1 -Template Gaming"
Write-Host ""

Write-Host "="*80 -ForegroundColor Gray

Write-Host "`n📚 Documentation:" -ForegroundColor Yellow
Write-Host ""
Write-Host "  • Complete Guide: D:\lazy init ide\docs\DESKTOP_COPILOT_COMPLETE_SUMMARY.md"
Write-Host "  • Customization: D:\lazy init ide\docs\DESKTOP_COPILOT_CUSTOMIZATION_GUIDE.md"
Write-Host "  • Quick Reference: D:\lazy init ide\docs\DESKTOP_COPILOT_QUICK_REFERENCE.md"
Write-Host ""

Write-Host "="*80 -ForegroundColor Gray

Write-Host "`n🎉 The Answer to Your Question:" -ForegroundColor Yellow
Write-Host ""
Write-Host "  ❓ Question: `"Can I reverse engineer this to build custom desktop copilots" -ForegroundColor White
Write-Host "              that sit in taskbar with full access for ANY purpose?`"" -ForegroundColor White
Write-Host ""
Write-Host "  ✅ Answer: ABSOLUTELY YES!" -ForegroundColor Green
Write-Host ""
Write-Host "  You can build copilots that help with:" -ForegroundColor Cyan
Write-Host "    • Gaming performance optimization" -ForegroundColor White
Write-Host "    • File organization and cleanup" -ForegroundColor White
Write-Host "    • Security monitoring and alerts" -ForegroundColor White
Write-Host "    • Automated backups" -ForegroundColor White
Write-Host "    • Health and wellness reminders" -ForegroundColor White
Write-Host "    • Network monitoring" -ForegroundColor White
Write-Host "    • System diagnostics and repairs" -ForegroundColor White
Write-Host "    • ANYTHING ELSE you can imagine!" -ForegroundColor White
Write-Host ""
Write-Host "  These copilots have:" -ForegroundColor Cyan
Write-Host "    ✅ Full system access (files, processes, network, registry)" -ForegroundColor White
Write-Host "    ✅ System tray integration (always accessible)" -ForegroundColor White
Write-Host "    ✅ Global hotkeys" -ForegroundColor White
Write-Host "    ✅ Background monitoring" -ForegroundColor White
Write-Host "    ✅ Smart notifications" -ForegroundColor White
Write-Host "    ✅ Can diagnose AND fix problems automatically" -ForegroundColor White
Write-Host ""
Write-Host "  NOT just for coding - for ANY computer task!" -ForegroundColor Magenta
Write-Host ""

Write-Host "="*80 -ForegroundColor Gray

Write-Host "`n💡 Pro Tip:" -ForegroundColor Yellow
Write-Host "  Start with a template, customize it, and package as EXE for distribution!" -ForegroundColor Cyan
Write-Host ""

Write-Host "🚀 Ready to build your own copilot? Run copilot_generator.ps1 now!" -ForegroundColor Green
Write-Host ""
