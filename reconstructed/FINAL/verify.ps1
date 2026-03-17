$isEmpty = (Get-ChildItem -Path D:\rawrxd\src -Recurse -Include "*.cpp" | Select-String -Pattern "\.isEmpty\(\)" | Measure-Object).Count
$qint = (Get-ChildItem -Path D:\rawrxd\src -Recurse -Include "*.cpp" | Select-String -Pattern "qint64|qint32" | Measure-Object).Count
$qinc = (Get-ChildItem -Path D:\rawrxd\src -Recurse -Include "*.cpp" | Select-String -Pattern "#include.*<Q|#include.*\"Q" | Measure-Object).Count

Write-Host ""
Write-Host "Qt Dependency Cleanup - Final Verification" -ForegroundColor Green
Write-Host ""
Write-Host "Remaining Qt References in Source:"
Write-Host "  isEmpty() calls: $isEmpty" -ForegroundColor $(if ($isEmpty -eq 0) { 'Green' } else { 'Red' })
Write-Host "  Qt integer types: $qint" -ForegroundColor $(if ($qint -eq 0) { 'Green' } else { 'Red' })
Write-Host "  Qt includes: $qinc" -ForegroundColor $(if ($qinc -eq 0) { 'Green' } else { 'Red' })
Write-Host ""

if ($isEmpty -eq 0 -and $qint -eq 0 -and $qinc -eq 0) {
    Write-Host "SUCCESS: ALL Qt REFERENCES REMOVED" -ForegroundColor Green
} else {
    Write-Host "WARNING: Still have Qt references" -ForegroundColor Yellow
}
Write-Host ""
