# Count-RemainingWork.ps1
# Scans D:\rawrxd\src for stubs, TODOs, empty returns, and incomplete implementations
# Groups by severity: CRITICAL (pipeline), MEDIUM (features), LOW (cosmetic)

$src = 'D:\rawrxd\src'
$inc = 'D:\rawrxd\include'

$critical = @()  # On inference/chat pipeline
$medium   = @()  # Feature gaps
$low      = @()  # Cosmetic / logging

$criticalFiles = @(
    'ai_model_caller*','ollama_proxy*','enhanced_model_loader*',
    'chat_interface*','chatpanel*','cpu_inference*','rawrxd_inference*',
    'rawrxd_transformer*','rawrxd_tokenizer*','rawrxd_sampler*',
    'rawrxd_model_loader*','gguf_loader*','gguf_parser*','gguf.cpp',
    'dml_inference*','model_source*','agentic_configuration*',
    'ModelDigestion*'
)

$patterns = @(
    @{ Pat='TODO';         Sev='find' }
    @{ Pat='FIXME';        Sev='find' }
    @{ Pat='STUB';         Sev='find' }
    @{ Pat='PLACEHOLDER';  Sev='find' }
    @{ Pat='HACK';         Sev='find' }
    @{ Pat='XXX';          Sev='find' }
    @{ Pat='NOT.?IMPL';    Sev='find' }
    @{ Pat='return \{\}';  Sev='find' }
)

# Regex for empty/stub bodies: { return; } or { return false; } or { return ""; } or { /* stub */ }
$stubBodyRx = 'return;$|return false;.*stub|return true;.*stub|return "";$|/\*\s*stub\s*\*/|// stub'

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "  RawrXD Remaining Work Scanner" -ForegroundColor Cyan
Write-Host "  Scanning: $src" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

# --- Pass 1: Pattern scan ---
$allFiles = Get-ChildItem $src,$inc -Include '*.cpp','*.hpp','*.h','*.asm' -Recurse -ErrorAction SilentlyContinue
$totalFiles = $allFiles.Count
$hitFiles = @{}

foreach ($f in $allFiles) {
    $name = $f.Name
    $isCrit = $false
    foreach ($cp in $criticalFiles) {
        if ($name -like $cp) { $isCrit = $true; break }
    }

    $lines = Get-Content $f.FullName -ErrorAction SilentlyContinue
    if (-not $lines) { continue }

    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        $ln = $i + 1

        # Check main patterns
        foreach ($p in $patterns) {
            if ($line -match $p.Pat) {
                # Skip comments that are just section headers or file descriptions
                $trimmed = $line.Trim()
                if ($trimmed -match '^\*\s' -and $trimmed -notmatch 'TODO|FIXME|STUB|HACK') { continue }
                
                $entry = [PSCustomObject]@{
                    File     = $name
                    Line     = $ln
                    Pattern  = $p.Pat
                    Severity = if ($isCrit) { 'CRITICAL' } elseif ($trimmed -match 'TODO|FIXME') { 'MEDIUM' } else { 'LOW' }
                    Text     = $trimmed.Substring(0, [Math]::Min(90, $trimmed.Length))
                }

                if ($isCrit) { $critical += $entry }
                elseif ($entry.Severity -eq 'MEDIUM') { $medium += $entry }
                else { $low += $entry }

                if (-not $hitFiles[$name]) { $hitFiles[$name] = 0 }
                $hitFiles[$name]++
                break  # one match per line
            }
        }

        # Check stub bodies
        if ($line -match $stubBodyRx) {
            $trimmed = $line.Trim()
            $entry = [PSCustomObject]@{
                File     = $name
                Line     = $ln
                Pattern  = 'STUB_BODY'
                Severity = if ($isCrit) { 'CRITICAL' } else { 'LOW' }
                Text     = $trimmed.Substring(0, [Math]::Min(90, $trimmed.Length))
            }
            if ($isCrit) { $critical += $entry } else { $low += $entry }
            if (-not $hitFiles[$name]) { $hitFiles[$name] = 0 }
            $hitFiles[$name]++
        }
    }
}

# --- Pass 2: Count compiled .obj files from today ---
$todayObjs = Get-ChildItem D:\rawrxd\build_prod -Filter '*.obj' -ErrorAction SilentlyContinue |
    Where-Object { $_.LastWriteTime.Date -eq (Get-Date).Date }

# --- Output ---
Write-Host "=== CRITICAL (pipeline blockers) ===" -ForegroundColor Red
if ($critical.Count -gt 0) {
    $critical | Group-Object File | Sort-Object Count -Descending | ForEach-Object {
        Write-Host "  $($_.Name): $($_.Count) items" -ForegroundColor Yellow
        $_.Group | ForEach-Object {
            Write-Host "    L$($_.Line) [$($_.Pattern)] $($_.Text)" -ForegroundColor Gray
        }
    }
} else {
    Write-Host "  None found!" -ForegroundColor Green
}

Write-Host "`n=== MEDIUM (feature gaps) ===" -ForegroundColor Yellow
if ($medium.Count -gt 0) {
    $medium | Group-Object File | Sort-Object Count -Descending | ForEach-Object {
        Write-Host "  $($_.Name): $($_.Count) items" -ForegroundColor DarkYellow
    }
} else {
    Write-Host "  None found!" -ForegroundColor Green
}

Write-Host "`n=== LOW (cosmetic/logging) ===" -ForegroundColor DarkGray
Write-Host "  $($low.Count) items across $( ($low | Select-Object -ExpandProperty File -Unique).Count ) files" -ForegroundColor DarkGray

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "  SUMMARY" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Files scanned:     $totalFiles" -ForegroundColor White
Write-Host "  Files with hits:   $($hitFiles.Count)" -ForegroundColor White
Write-Host "  CRITICAL items:    $($critical.Count)" -ForegroundColor $(if($critical.Count -eq 0){'Green'}else{'Red'})
Write-Host "  MEDIUM items:      $($medium.Count)" -ForegroundColor $(if($medium.Count -lt 10){'Green'}else{'Yellow'})
Write-Host "  LOW items:         $($low.Count)" -ForegroundColor DarkGray
Write-Host "  Total remaining:   $($critical.Count + $medium.Count + $low.Count)" -ForegroundColor White
Write-Host ""
Write-Host "  Built today:       $($todayObjs.Count) .obj files" -ForegroundColor Green
$todayObjs | ForEach-Object {
    $sz = if ($_.Length -gt 1MB) { '{0:N1} MB' -f ($_.Length/1MB) } else { '{0:N0} KB' -f ($_.Length/1KB) }
    Write-Host "    $($_.Name) ($sz)" -ForegroundColor DarkGreen
}
Write-Host "========================================`n" -ForegroundColor Cyan
