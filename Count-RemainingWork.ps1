param(
    [string]$Src = 'D:\rawrxd\src',
    [string]$Inc = 'D:\rawrxd\include',
    [int]$MaxFileMB = 16,
    [int]$MaxAsmFileMB = 2,
    [int]$MaxHitsPerFile = 200,
    [int]$MaxLinesPerFile = 50000,
    [switch]$NoProgress
)

# Count-RemainingWork.ps1
# Scans source/include for stubs, TODOs, empty returns, and incomplete implementations.
# Groups by severity: CRITICAL (pipeline), MEDIUM (features), LOW (cosmetic)

$src = $Src
$inc = $Inc

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
$stubBodyRx = [regex]::new('return;$|return false;.*stub|return true;.*stub|return "";$|/\*\s*stub\s*\*/|// stub',
    [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)

$compiledPatterns = @()
foreach ($p in $patterns) {
    $compiledPatterns += [PSCustomObject]@{
        Pat = $p.Pat
        Rx  = [regex]::new($p.Pat, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
    }
}

$extSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
@('.cpp', '.hpp', '.h', '.asm') | ForEach-Object { [void]$extSet.Add($_) }

# Exclude very heavy/generated/vendor trees that make scanning appear frozen.
$excludeDirRx = '(\\|/)(build|build_prod|build_smoke_auto|obj|history|Full Source|RawrXD_FULL_DRIVE_BACKUP|node_modules|\.git|ggml|nlohmann)(\\|/)'

function Test-IsCriticalFile {
    param([string]$Name, [string[]]$Masks)
    foreach ($m in $Masks) {
        if ($Name -like $m) { return $true }
    }
    return $false
}

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "  RawrXD Remaining Work Scanner" -ForegroundColor Cyan
Write-Host "  Scanning: $src" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

# --- Pass 1: Pattern scan ---
$allFiles = Get-ChildItem $src,$inc -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object {
        $extSet.Contains($_.Extension) -and
        ($_.FullName -notmatch $excludeDirRx)
    }

$totalFiles = $allFiles.Count
$hitFiles = @{}
$perFileHits = @{}
$truncatedFiles = @{}
$skippedLarge = 0
$skippedUnreadable = 0
$skippedLineCap = 0
$maxBytes = [int64]$MaxFileMB * 1MB
$maxAsmBytes = [int64]$MaxAsmFileMB * 1MB
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$lastHeartbeatSec = 0

for ($fileIndex = 0; $fileIndex -lt $allFiles.Count; $fileIndex++) {
    $f = $allFiles[$fileIndex]

    if (-not $NoProgress) {
        $pct = if ($totalFiles -gt 0) { [int](($fileIndex + 1) * 100 / $totalFiles) } else { 100 }
        Write-Progress -Activity 'RawrXD Remaining Work Scanner' -Status "Scanning $($f.Name) ($($fileIndex + 1)/$totalFiles)" -PercentComplete $pct
    }

    if (($fileIndex + 1) % 250 -eq 0) {
        Write-Host ("[progress] {0}/{1} files, hits={2}, elapsed={3:n1}s" -f ($fileIndex + 1), $totalFiles, $hitFiles.Count, $sw.Elapsed.TotalSeconds) -ForegroundColor DarkCyan
    }

    if ($f.Length -gt $maxBytes) {
        $skippedLarge++
        continue
    }

    if ($f.Extension -ieq '.asm' -and $f.Length -gt $maxAsmBytes) {
        $skippedLarge++
        continue
    }

    $name = $f.Name
    $isCrit = Test-IsCriticalFile -Name $name -Masks $criticalFiles

    try {
        if (-not $perFileHits.ContainsKey($name)) { $perFileHits[$name] = 0 }
        $lineNumber = 0
        $stopFile = $false
        foreach ($line in [System.IO.File]::ReadLines($f.FullName)) {
            $lineNumber++

            if ($lineNumber -ge $MaxLinesPerFile) {
                $truncatedFiles[$name] = $true
                $skippedLineCap++
                break
            }

            if ($sw.Elapsed.TotalSeconds -ge ($lastHeartbeatSec + 5)) {
                $lastHeartbeatSec = [int]$sw.Elapsed.TotalSeconds
                Write-Host ("[heartbeat] scanning {0} (line {1}), elapsed={2:n1}s" -f $name, $lineNumber, $sw.Elapsed.TotalSeconds) -ForegroundColor DarkGray
            }

            # Check main patterns
            foreach ($p in $compiledPatterns) {
                if ($p.Rx.IsMatch($line)) {
                    # Skip comments that are just section headers or file descriptions
                    $trimmed = $line.Trim()
                    if ($trimmed -match '^\*\s' -and $trimmed -notmatch 'TODO|FIXME|STUB|HACK') { continue }

                    $entry = [PSCustomObject]@{
                        File     = $name
                        Line     = $lineNumber
                        Pattern  = $p.Pat
                        Severity = if ($isCrit) { 'CRITICAL' } elseif ($trimmed -match 'TODO|FIXME') { 'MEDIUM' } else { 'LOW' }
                        Text     = $trimmed.Substring(0, [Math]::Min(90, $trimmed.Length))
                    }

                    if ($isCrit) { $critical += $entry }
                    elseif ($entry.Severity -eq 'MEDIUM') { $medium += $entry }
                    else { $low += $entry }

                    if (-not $hitFiles[$name]) { $hitFiles[$name] = 0 }
                    $hitFiles[$name]++
                    $perFileHits[$name]++

                    if ($perFileHits[$name] -ge $MaxHitsPerFile) {
                        $truncatedFiles[$name] = $true
                        $stopFile = $true
                    }
                    break  # one match per line
                }
            }

            if ($stopFile) { break }

            # Check stub bodies
            if ($stubBodyRx.IsMatch($line)) {
                $trimmed = $line.Trim()
                $entry = [PSCustomObject]@{
                    File     = $name
                    Line     = $lineNumber
                    Pattern  = 'STUB_BODY'
                    Severity = if ($isCrit) { 'CRITICAL' } else { 'LOW' }
                    Text     = $trimmed.Substring(0, [Math]::Min(90, $trimmed.Length))
                }
                if ($isCrit) { $critical += $entry } else { $low += $entry }
                if (-not $hitFiles[$name]) { $hitFiles[$name] = 0 }
                $hitFiles[$name]++
                $perFileHits[$name]++

                if ($perFileHits[$name] -ge $MaxHitsPerFile) {
                    $truncatedFiles[$name] = $true
                    break
                }
            }
        }
    } catch {
        $skippedUnreadable++
        continue
    }
}

if (-not $NoProgress) {
    Write-Progress -Activity 'RawrXD Remaining Work Scanner' -Completed
}

# --- Pass 2: Count compiled .obj files from today ---
$todayObjs = Get-ChildItem D:\rawrxd\build_prod -Filter '*.obj' -ErrorAction SilentlyContinue |
    Where-Object { $_.LastWriteTime.Date -eq (Get-Date).Date }

# --- Output ---
Write-Host "=== CRITICAL (pipeline blockers) ===" -ForegroundColor Red
if ($critical.Count -gt 0) {
    $maxCriticalDetailsPerFile = 25
    $critical | Group-Object File | Sort-Object Count -Descending | ForEach-Object {
        Write-Host "  $($_.Name): $($_.Count) items" -ForegroundColor Yellow
        $preview = $_.Group | Select-Object -First $maxCriticalDetailsPerFile
        $preview | ForEach-Object {
            Write-Host "    L$($_.Line) [$($_.Pattern)] $($_.Text)" -ForegroundColor Gray
        }

        if ($_.Count -gt $maxCriticalDetailsPerFile) {
            Write-Host "    ... +$($_.Count - $maxCriticalDetailsPerFile) more entries (output truncated)" -ForegroundColor DarkGray
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
Write-Host "  Skipped (large):   $skippedLarge (> $MaxFileMB MB, .asm > $MaxAsmFileMB MB)" -ForegroundColor DarkYellow
Write-Host "  Skipped (errors):  $skippedUnreadable" -ForegroundColor DarkYellow
Write-Host "  Hit line cap:      $skippedLineCap (max $MaxLinesPerFile lines/file)" -ForegroundColor DarkYellow
Write-Host "  Truncated files:   $($truncatedFiles.Count) (max $MaxHitsPerFile hits/file)" -ForegroundColor DarkYellow
Write-Host "  CRITICAL items:    $($critical.Count)" -ForegroundColor $(if($critical.Count -eq 0){'Green'}else{'Red'})
Write-Host "  MEDIUM items:      $($medium.Count)" -ForegroundColor $(if($medium.Count -lt 10){'Green'}else{'Yellow'})
Write-Host "  LOW items:         $($low.Count)" -ForegroundColor DarkGray
Write-Host "  Total remaining:   $($critical.Count + $medium.Count + $low.Count)" -ForegroundColor White
Write-Host "  Elapsed:           $([math]::Round($sw.Elapsed.TotalSeconds,2)) sec" -ForegroundColor White
Write-Host ""
Write-Host "  Built today:       $($todayObjs.Count) .obj files" -ForegroundColor Green
$todayObjs | ForEach-Object {
    $sz = if ($_.Length -gt 1MB) { '{0:N1} MB' -f ($_.Length/1MB) } else { '{0:N0} KB' -f ($_.Length/1KB) }
    Write-Host "    $($_.Name) ($sz)" -ForegroundColor DarkGreen
}
Write-Host "========================================`n" -ForegroundColor Cyan
