param(
    [string]$RepoRoot = "d:\RawrXD",
    [string[]]$SourceDirs = @("src", "include")
)

$ErrorActionPreference = "Stop"
Set-Location $RepoRoot

$ts = Get-Date -Format 'yyyyMMdd_HHmmss'
$outDir = Join-Path (Get-Location) 'reports'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$csv = Join-Path $outDir ("source_file_status_audit_" + $ts + ".csv")
$json = Join-Path $outDir ("source_file_status_audit_" + $ts + ".json")

$exts = @('.c','.cc','.cpp','.cxx','.h','.hpp','.hh','.inl','.asm','.s','.ps1','.py','.js','.ts')
$excludeDirPattern = '(\\|/)(\.git|build|build-win32|build-ninja|build_prod|\.build|external|third_party|vendor|node_modules|out|logs|reports|memories|\.vs|\.vscode)(\\|/)'

$allFiles = @()
foreach ($dir in $SourceDirs) {
    $dirPath = Join-Path (Get-Location) $dir
    if (Test-Path $dirPath) {
        $allFiles += Get-ChildItem -Path $dirPath -Recurse -File -ErrorAction SilentlyContinue
    }
}

$files = $allFiles | Where-Object {
    $exts -contains $_.Extension.ToLowerInvariant() -and $_.FullName -notmatch $excludeDirPattern
}

$results = foreach ($f in $files) {
    $rel = [System.IO.Path]::GetRelativePath((Get-Location).Path, $f.FullName).Replace('\\','/')
    $content = ''
    try {
        $content = [string](Get-Content -LiteralPath $f.FullName -Raw -ErrorAction Stop)
    }
    catch {
        $content = ''
    }

    if ($null -eq $content) {
        $content = ''
    }

    $lc = $content.ToLowerInvariant()

    $hasTodo = $lc -match '\btodo\b|\bfixme\b|\bxxx\b|\bnot\s+yet\b|\bunimplemented\b'
    $stubbed = $lc -match '\bstub\b|notimplemented|not\s+implemented|placeholder\s+implementation|todo\s*:\s*implement|throw\s+std::(logic_error|runtime_error)\s*\(\s*"not implemented|return\s+\{\s*\}\s*;\s*(//.*)?$'
    $scaffolded = $lc -match '\bscaffold\b|\bskeleton\b|boilerplate|generated\s+by' -or (($f.Length -lt 900) -and ($hasTodo -or $stubbed))
    $dead = $lc -match '#if\s+0\b|\bdead\s+code\b|\bdeprecated\b|\bobsolete\b|\bnever\s+used\b|\bunused\b'
    $junkPath = $rel.ToLowerInvariant() -match '(^|/)(tmp|temp|junk|trash|backup|old|copy|scratch|wip)(/|$)|\.(bak|old|tmp)$'
    $junkContent = $lc -match 'lorem ipsum|asdf|qwer|testtest|do not use'
    $junk = $junkPath -or $junkContent

    $notFinished = $hasTodo -or $stubbed -or $scaffolded
    $finished = -not ($notFinished -or $dead -or $junk)

    $ev = @()
    if ($finished) { $ev += 'no stub/scaffold/todo/dead/junk markers' }
    if ($hasTodo) { $ev += 'todo/fixme markers' }
    if ($stubbed) { $ev += 'stub/not-implemented markers' }
    if ($scaffolded) { $ev += 'scaffold/skeleton/boilerplate or tiny placeholder' }
    if ($dead) { $ev += 'dead/deprecated/unused markers' }
    if ($junk) { $ev += 'junk/temp naming/content markers' }

    [pscustomobject]@{
        file = $rel
        finished = $finished
        not_finished = $notFinished
        scaffolded = $scaffolded
        stubbed = $stubbed
        junk_code = $junk
        dead_code = $dead
        evidence = ($ev -join '; ')
    }
}

$sorted = $results | Sort-Object file
$sorted | Export-Csv -NoTypeInformation -Encoding UTF8 -Path $csv
$sorted | ConvertTo-Json -Depth 4 | Out-File -Encoding UTF8 $json

$summary = [pscustomobject]@{
    total = $sorted.Count
    finished = ($sorted | Where-Object finished).Count
    not_finished = ($sorted | Where-Object not_finished).Count
    scaffolded = ($sorted | Where-Object scaffolded).Count
    stubbed = ($sorted | Where-Object stubbed).Count
    junk_code = ($sorted | Where-Object junk_code).Count
    dead_code = ($sorted | Where-Object dead_code).Count
    csv = $csv
    json = $json
}

$summary | ConvertTo-Json -Depth 3
