# Comprehensive Codebase Audit for Win32/x64 MASM IDE (Qt-Free)

Write-Host "=== RawrXD Codebase Audit ===" -ForegroundColor Cyan
Write-Host ""

# Count all source files
$allFiles = Get-ChildItem -Path "D:\rawrxd" -Recurse -File -Include *.cpp,*.c,*.h,*.hpp,*.asm,*.inc -ErrorAction SilentlyContinue
$totalCount = $allFiles.Count
Write-Host "Total source files: $totalCount" -ForegroundColor Green

# Breakdown by extension
Write-Host "`nBreakdown by extension:" -ForegroundColor Yellow
$allFiles | Group-Object Extension | Sort-Object Count -Descending | ForEach-Object {
    Write-Host "  $($_.Name): $($_.Count)"
}

# Check for Qt dependencies
Write-Host "`n=== Qt Dependency Analysis ===" -ForegroundColor Cyan
$qtPatterns = @('QWidget', 'QObject', 'QString', 'QList', 'QVector', 'QHash', 'QMap', '#include.*<Q', '#include.*"Q')
$qtFiles = @()
foreach ($pattern in $qtPatterns) {
    $matches = $allFiles | Select-String -Pattern $pattern -List
    $qtFiles += $matches
}
$qtFileCount = ($qtFiles | Select-Object -Unique Path).Count
Write-Host "Files with Qt dependencies: $qtFileCount" -ForegroundColor $(if ($qtFileCount -gt 0) { "Red" } else { "Green" })

# Check for Win32 API usage
Write-Host "`n=== Win32 API Analysis ===" -ForegroundColor Cyan
$win32Patterns = @('HWND', 'WPARAM', 'LPARAM', 'CreateWindow', 'RegisterClass', 'DefWindowProc')
$win32Files = @()
foreach ($pattern in $win32Patterns) {
    $matches = $allFiles | Select-String -Pattern $pattern -List
    $win32Files += $matches
}
$win32FileCount = ($win32Files | Select-Object -Unique Path).Count
Write-Host "Files with Win32 API: $win32FileCount" -ForegroundColor Green

# Check for MASM files
Write-Host "`n=== MASM Assembly Analysis ===" -ForegroundColor Cyan
$asmFiles = $allFiles | Where-Object { $_.Extension -eq '.asm' }
Write-Host "MASM assembly files: $($asmFiles.Count)" -ForegroundColor Green

# Check for configuration issues
Write-Host "`n=== Configuration Analysis ===" -ForegroundColor Cyan
$cmakeFiles = Get-ChildItem -Path "D:\rawrxd" -Recurse -File -Filter "CMakeLists.txt" -ErrorAction SilentlyContinue
Write-Host "CMakeLists.txt files: $($cmakeFiles.Count)"

# Check for build system files
$buildFiles = Get-ChildItem -Path "D:\rawrxd" -Recurse -File -Include "*.vcxproj","*.sln","Makefile","*.mk" -ErrorAction SilentlyContinue
Write-Host "Build system files: $($buildFiles.Count)"

# Summary
Write-Host "`n=== AUDIT SUMMARY ===" -ForegroundColor Cyan
Write-Host "Total source files: $totalCount" -ForegroundColor $(if ($totalCount -gt 3500) { "Red" } else { "Yellow" })
Write-Host "Target threshold: 3500 files"
Write-Host "Status: $(if ($totalCount -gt 3500) { 'EXCEEDS' } else { 'BELOW' }) threshold" -ForegroundColor $(if ($totalCount -gt 3500) { "Red" } else { "Green" })
Write-Host ""
Write-Host "Qt-Free Status: $(if ($qtFileCount -eq 0) { 'CLEAN' } else { "$qtFileCount files need cleanup" })" -ForegroundColor $(if ($qtFileCount -eq 0) { "Green" } else { "Red" })
Write-Host "Win32 Integration: $(if ($win32FileCount -gt 0) { 'ACTIVE' } else { 'MISSING' })" -ForegroundColor $(if ($win32FileCount -gt 0) { "Green" } else { "Red" })
Write-Host "MASM Support: $(if ($asmFiles.Count -gt 0) { 'PRESENT' } else { 'MISSING' })" -ForegroundColor $(if ($asmFiles.Count -gt 0) { "Green" } else { "Red" })

# Export detailed report
$reportPath = "D:\rawrxd\CODEBASE_AUDIT_REPORT.txt"
@"
RawrXD Codebase Audit Report
Generated: $(Get-Date)
========================================

TOTAL SOURCE FILES: $totalCount
Target Threshold: 3500
Status: $(if ($totalCount -gt 3500) { 'EXCEEDS' } else { 'BELOW' }) threshold

FILE BREAKDOWN:
$($allFiles | Group-Object Extension | Sort-Object Count -Descending | ForEach-Object { "  $($_.Name): $($_.Count)" } | Out-String)

QT DEPENDENCIES: $qtFileCount files
WIN32 API USAGE: $win32FileCount files
MASM FILES: $($asmFiles.Count) files
CMAKE FILES: $($cmakeFiles.Count) files
BUILD FILES: $($buildFiles.Count) files

CONFIGURATION REQUIREMENTS FOR QT-FREE WIN32/X64 MASM IDE:
- Remove all Qt dependencies from $qtFileCount files
- Ensure Win32 API is properly integrated
- Configure MASM build system for $($asmFiles.Count) assembly files
- Update $($cmakeFiles.Count) CMakeLists.txt files for Qt-free builds
- Verify $($buildFiles.Count) build system files are configured correctly

RECOMMENDATION:
$(if ($totalCount -lt 3500) {
    "Codebase is BELOW 3500 files threshold. Current count: $totalCount files.
    Focus on:
    1. Removing Qt dependencies from $qtFileCount files
    2. Ensuring Win32 API integration is complete
    3. Configuring MASM build system properly"
} else {
    "Codebase EXCEEDS 3500 files threshold. Current count: $totalCount files.
    Consider:
    1. Consolidating duplicate code
    2. Removing unused files
    3. Modularizing the codebase"
})
"@ | Out-File -FilePath $reportPath -Encoding UTF8

Write-Host "`nDetailed report saved to: $reportPath" -ForegroundColor Green
