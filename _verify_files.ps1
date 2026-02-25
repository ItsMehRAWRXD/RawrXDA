cd d:\rawrxd

Write-Host "`n=== RAWRXD SOURCE FILE VERIFICATION ===" -ForegroundColor Cyan
Write-Host "Date: $(Get-Date)" 

# 1. Directory-level counts
Write-Host "`n--- DIRECTORY INVENTORY ---" -ForegroundColor Yellow
$dirs = @(
    "src", "src\core", "src\win32app", "src\asm", "src\asm\monolithic",
    "src\modules", "src\cli", "src\agentic", "src\agent",
    "include", "include\nlohmann", "include\ide", "include\logging",
    "include\kernel_dispatch", "include\plugin_system",
    "3rdparty", "Full Source", "reconstructed", ".archived_orphans"
)
foreach ($d in $dirs) {
    if (Test-Path $d) {
        $c = (Get-ChildItem $d -File -ErrorAction SilentlyContinue).Count
        Write-Host ("  {0,-40} {1,5} files" -f $d, $c) -ForegroundColor Green
    } else {
        Write-Host ("  {0,-40} MISSING!" -f $d) -ForegroundColor Red
    }
}

# 2. File type totals
Write-Host "`n--- FILE TYPE TOTALS (src/ + include/) ---" -ForegroundColor Yellow
$cpp = @(Get-ChildItem src -Include *.cpp -Recurse -EA SilentlyContinue).Count
$c   = @(Get-ChildItem src -Include *.c -Recurse -EA SilentlyContinue).Count
$h   = @(Get-ChildItem src,include -Include *.h,*.hpp -Recurse -EA SilentlyContinue).Count
$asm = @(Get-ChildItem src -Include *.asm -Recurse -EA SilentlyContinue).Count
$inc = @(Get-ChildItem src -Include *.inc -Recurse -EA SilentlyContinue).Count
Write-Host "  .cpp files:  $cpp"
Write-Host "  .c files:    $c"
Write-Host "  .h/.hpp:     $h"
Write-Host "  .asm files:  $asm"
Write-Host "  .inc files:  $inc"
Write-Host "  TOTAL:       $($cpp + $c + $h + $asm + $inc)"

# 3. Critical files spot check 
Write-Host "`n--- CRITICAL FILES CHECK ---" -ForegroundColor Yellow
$critical = @(
    "src\win32app\Win32IDE.h",
    "src\win32app\Win32IDE_AIBackend.cpp",
    "src\win32app\Win32IDE_BeaconWiring.cpp",
    "src\win32app\Win32IDE_HotpatchWiring.cpp",
    "src\win32app\Win32IDE_InitSequence.cpp",
    "src\win32app\Win32IDE_Build.cpp",
    "src\win32app\Win32IDE_CircularArchStub.cpp",
    "src\win32app\Win32IDE_SidebarBridge.cpp",
    "src\asm\monolithic\main.asm",
    "src\asm\monolithic\beacon.asm",
    "src\asm\monolithic\inference.asm",
    "src\asm\monolithic\model_loader.asm",
    "src\asm\monolithic\rawrxd.inc",
    "src\modules\copilot_gap_closer.h",
    "include\editor_engine.h",
    "include\nlohmann\json.hpp",
    "include\logging\logger.h",
    "include\BATCH2_CONTEXT.h",
    "include\renderer.h",
    "include\sqlite3.h",
    "compile_fix_orchestrator.ps1",
    "subagent_compile_fixer.ps1",
    "spawn_subagent_fixers.ps1",
    "failed_files.json"
)
$ok = 0; $miss = 0
foreach ($f in $critical) {
    if (Test-Path $f) {
        $sz = (Get-Item $f).Length
        Write-Host ("  OK     {0,10:N0} B  {1}" -f $sz, $f) -ForegroundColor Green
        $ok++
    } else {
        Write-Host ("  MISS   {0,10}    {1}" -f "---", $f) -ForegroundColor Red
        $miss++
    }
}
Write-Host "`n  Result: $ok OK, $miss MISSING out of $($critical.Count) critical files"

# 4. Full Source backup check 
Write-Host "`n--- BACKUP DIRECTORIES ---" -ForegroundColor Yellow
$backups = @("Full Source", "reconstructed", ".archived_orphans", "history", ".ultra_archived")
foreach ($b in $backups) {
    if (Test-Path $b) {
        $total = @(Get-ChildItem $b -Recurse -File -EA SilentlyContinue).Count
        Write-Host ("  {0,-25} {1,6} files" -f $b, $total) -ForegroundColor Green
    } else {
        Write-Host ("  {0,-25} NOT FOUND" -f $b) -ForegroundColor DarkGray
    }
}

# 5. Grand total
Write-Host "`n--- GRAND TOTAL (all files in repo) ---" -ForegroundColor Yellow
$allFiles = @(Get-ChildItem . -Recurse -File -EA SilentlyContinue -Exclude .git).Count
Write-Host "  Total files in d:\rawrxd: $allFiles" -ForegroundColor Cyan
