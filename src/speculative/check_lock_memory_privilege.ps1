# RawrXD SeLockMemoryPrivilege Check and Fix Script
# Run this script to diagnose and get fix instructions for large page support

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "RawrXD SeLockMemoryPrivilege Check" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check if running as Administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Write-Host "[WARNING] Not running as Administrator" -ForegroundColor Yellow
    Write-Host "         Some checks may be incomplete" -ForegroundColor Yellow
    Write-Host ""
}

# Check current privileges
Write-Host "Current User Privileges:" -ForegroundColor White
$privs = whoami /priv 2>$null
$lockMemoryPriv = $privs | Select-String "SeLockMemoryPrivilege"

if ($lockMemoryPriv) {
    Write-Host "[OK] SeLockMemoryPrivilege found in token" -ForegroundColor Green
    Write-Host "     $lockMemoryPriv" -ForegroundColor Gray
} else {
    Write-Host "[FAIL] SeLockMemoryPrivilege NOT in user token" -ForegroundColor Red
    Write-Host ""
    Write-Host "This means you CANNOT use large pages (2MB) for memory allocation." -ForegroundColor Yellow
    Write-Host "Without large pages, the DDR5-to-GPU aperture bypass will be limited." -ForegroundColor Yellow
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Memory Information:" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# Get memory info
$os = Get-CimInstance Win32_OperatingSystem
$cs = Get-CimInstance Win32_ComputerSystem
$totalRAM = [math]::Round($cs.TotalPhysicalMemory / 1GB, 2)
$freeRAM = [math]::Round($os.FreePhysicalMemory / 1MB / 1024, 2)
$usedRAM = [math]::Round($totalRAM - $freeRAM, 2)
$usedPercent = [math]::Round(($totalRAM - $freeRAM) / $totalRAM * 100, 1)

Write-Host "Total RAM:     $totalRAM GB" -ForegroundColor White
Write-Host "Used RAM:      $usedRAM GB ($usedPercent%)" -ForegroundColor White
Write-Host "Free RAM:      $freeRAM GB" -ForegroundColor White

# Check large page support
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Large Page Support:" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$largePageSize = (Get-CimInstance Win32_Processor).MaxClockSpeed
Write-Host "System Large Page Size: 2 MB (standard)" -ForegroundColor White

# Try to allocate a large page (test)
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public class LargePageTest {
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr VirtualAlloc(IntPtr lpAddress, ulong dwSize, 
        uint flAllocationType, uint flProtect);
    
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool VirtualFree(IntPtr lpAddress, ulong dwSize, uint dwFreeType);
    
    public const uint MEM_COMMIT = 0x1000;
    public const uint MEM_RESERVE = 0x2000;
    public const uint MEM_LARGE_PAGES = 0x20000000;
    public const uint PAGE_READWRITE = 0x04;
    public const uint MEM_RELEASE = 0x8000;
    
    public static IntPtr TryAllocateLargePage() {
        ulong size = 2UL * 1024 * 1024; // 2MB
        IntPtr ptr = VirtualAlloc(IntPtr.Zero, size, 
            MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
        int err = Marshal.GetLastWin32Error();
        
        if (ptr != IntPtr.Zero) {
            VirtualFree(ptr, 0, MEM_RELEASE);
            return new IntPtr(1); // Success
        }
        return new IntPtr(err); // Return error code
    }
}
"@

$result = [LargePageTest]::TryAllocateLargePage()
if ($result.ToInt64() -eq 1) {
    Write-Host "[OK] Large page allocation SUCCEEDED" -ForegroundColor Green
    Write-Host "     2MB pages are available for use" -ForegroundColor Green
} else {
    $errCode = $result.ToInt64()
    Write-Host "[FAIL] Large page allocation FAILED" -ForegroundColor Red
    Write-Host "       Error code: $errCode" -ForegroundColor Red
    
    if ($errCode -eq 1314) { # ERROR_PRIVILEGE_NOT_HELD
        Write-Host "       ERROR_PRIVILEGE_NOT_HELD (1314)" -ForegroundColor Red
        Write-Host "       SeLockMemoryPrivilege is not assigned to your user" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "FIX INSTRUCTIONS:" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

if ($lockMemoryPriv -and $result.ToInt64() -eq 1) {
    Write-Host "No fix needed - large pages are working!" -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "To enable large pages, follow these steps:" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "1. Press Win+R and type: secpol.msc" -ForegroundColor White
    Write-Host "2. Navigate to: Local Policies > User Rights Assignment" -ForegroundColor White
    Write-Host "3. Find: 'Lock pages in memory'" -ForegroundColor White
    Write-Host "4. Double-click it and click 'Add User or Group...'" -ForegroundColor White
    Write-Host "5. Type your username and click 'Check Names', then OK" -ForegroundColor White
    Write-Host "6. Click OK to close the dialog" -ForegroundColor White
    Write-Host ""
    Write-Host "7. LOG OUT and LOG BACK IN (or reboot)" -ForegroundColor Yellow
    Write-Host "   This is REQUIRED - the token is only updated at login" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "8. Run this script again to verify" -ForegroundColor White
    Write-Host ""
    Write-Host "Alternative (PowerShell as Admin):" -ForegroundColor Cyan
    Write-Host "   ntrights +r SeLockMemoryPrivilege -u `"`$env:USERNAME`"" -ForegroundColor Gray
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "RawrXD Thresholds for 64GB System:" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "NORMAL:   70% (44.8 GB) - Start prefetch" -ForegroundColor White
Write-Host "WARNING:  85% (54.4 GB) - Enable bypass" -ForegroundColor Yellow
Write-Host "CRITICAL: 95% (60.8 GB) - Direct DDR5 path" -ForegroundColor Red
Write-Host ""
Write-Host "With large pages enabled, these tiers will activate properly." -ForegroundColor Gray
Write-Host "Without large pages, tier promotion is capped at Tier 1." -ForegroundColor Gray
Write-Host ""