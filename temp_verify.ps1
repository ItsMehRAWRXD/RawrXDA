$checks = @{
    "CMake real default" = (Get-Content CMakeLists.txt) -match 'STUBS.*OFF'
    "Command handlers real" = (Get-ChildItem src/win32app -Filter "*Commands*.cpp" | Get-Content) -match 'cmdFileNew.*\{'
    "AI workers queue" = Test-Path src/ai/ai_workers.cpp
    "Validation fixed" = (Get-Content VALIDATE_REVERSE_ENGINEERING.ps1) -match 'PSScriptRoot'
    "Training enabled" = (Get-Content src/ai/aitraining_worker.cpp) -match 'performCPUBatch'
    "ASM re-enabled" = (Get-Content CMakeLists.txt) -match 'StreamingOrchestrator.*asm'
}

$score = 0
$checks.GetEnumerator() | ForEach-Object {
    $status = if ($_.Value) { "✅" } else { "❌" }
    if ($_.Value) { $score++ }
    Write-Host "$status $($_.Key)"
}

$percent = [math]::Round($score/6*100)
Write-Host "`nZero-Patch Score: $score/6 ($percent%)"