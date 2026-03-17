param(
  [string]$SourceDir = (Resolve-Path ".").Path,
  [string]$BuildDir = (Join-Path (Resolve-Path ".").Path "build"),
  [string]$OutDir = (Join-Path (Resolve-Path ".").Path "bin/reverse-cmake")
)

$ErrorActionPreference = 'Stop'

function Ensure-Dir($p) { if (-not (Test-Path $p)) { New-Item -ItemType Directory -Force -Path $p | Out-Null } }

Ensure-Dir $BuildDir
Ensure-Dir $OutDir

# 1) Request codemodel via CMake File API
$QueryDir = Join-Path $BuildDir ".cmake/api/v1/query"
$ReplyDir = Join-Path $BuildDir ".cmake/api/v1/reply"
Ensure-Dir $QueryDir
Ensure-Dir $ReplyDir

# create codemodel-v2 query file
$cmQuery = Join-Path $QueryDir "codemodel-v2"
if (-not (Test-Path $cmQuery)) { New-Item -ItemType File -Force -Path $cmQuery | Out-Null }

# 2) Re-configure to populate reply
Write-Host "[reverse-cmake] Configuring CMake to produce File API replies..."
& cmake -S $SourceDir -B $BuildDir | Out-Null

# 3) Find latest index json
$index = Get-ChildItem -Path (Join-Path $ReplyDir "index-*.json") -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $index) { throw "CMake File API index not found in $ReplyDir" }
$indexJson = Get-Content -Raw -Path $index.FullName | ConvertFrom-Json

# 4) Load codemodel json (first config) or fallback
$cmReply = $indexJson.reply.codemodel
if (-not $cmReply) {
  Write-Warning "codemodel reply missing; falling back to graphviz + target help summary"
  & cmake ("--graphviz=" + (Join-Path $OutDir "fallback_graph.dot")) $BuildDir | Out-Null
  $helpOut = & cmake --build $BuildDir --target help 2>&1
  $helpPath = Join-Path $OutDir "fallback_targets.txt"
  $helpOut | Out-File -Encoding UTF8 $helpPath
  Write-Host "[reverse-cmake] Wrote fallback outputs:" -ForegroundColor Yellow
  Write-Host "  $helpPath"
  Write-Host "  $(Join-Path $OutDir 'fallback_graph.dot')"
  exit 0
}
$cmFile = $cmReply[0].jsonFile
$cmPath = Join-Path $ReplyDir $cmFile
$cm = Get-Content -Raw -Path $cmPath | ConvertFrom-Json

# 5) Gather targets across configurations
$cfg = $cm.configurations[0]
$targets = @()
$targetMap = @{}

foreach ($t in $cfg.targets) {
  $tPath = Join-Path $ReplyDir $t.jsonFile
  $tJson = Get-Content -Raw -Path $tPath | ConvertFrom-Json
  $name = $tJson.name
  $type = $tJson.type
  $project = $tJson.project?.name
  $srcs = @()
  $incs = @()
  $defs = @()
  $deps = @()
  $langs = @()

  foreach ($cg in ($tJson.compileGroups | ForEach-Object { $_ })) {
    $langs += @($cg.language) | Where-Object { $_ }
    $incs += @($cg.includes | ForEach-Object { $_.path }) | Where-Object { $_ }
    $defs += @($cg.defines | ForEach-Object { $_.define }) | Where-Object { $_ }
    $srcs += @($cg.sourceIndexes | ForEach-Object { $tJson.sources[$_]?.path }) | Where-Object { $_ }
  }
  $langs = $langs | Sort-Object -Unique
  $incs = $incs | Sort-Object -Unique
  $defs = $defs | Sort-Object -Unique
  $srcs = $srcs | Sort-Object -Unique

  # dependencies (link or target depends)
  $depNames = @()
  foreach ($bt in ($tJson.backtraceGraph?.commands | ForEach-Object { $_ })) { }
  if ($tJson.dependencies) { $depNames = $tJson.dependencies.name }

  $obj = [ordered]@{
    name = $name
    type = $type
    project = $project
    languages = $langs
    sources = $srcs
    includeDirs = $incs
    defines = $defs
    dependencies = $depNames
  }
  $targets += [pscustomobject]$obj
  $targetMap[$name] = $obj
}

# 6) Write outputs
$targetsPath = Join-Path $OutDir "reverse_cmake_targets.json"
$targets | ConvertTo-Json -Depth 6 | Out-File -Encoding UTF8 $targetsPath

# Markdown summary
$md = @()
$md += "# Reverse CMake Summary"
$md += "Generated: $(Get-Date -Format o)"
$md += "\n## Targets"
foreach ($t in $targets) {
  $langs = [string]::Join(',', $t.languages)
  $line = ('- Name: `' + $t.name + '` — Type: ' + $t.type + ' — Langs: ' + $langs + ' — Srcs: ' + $t.sources.Count)
  $md += $line
}
$md += "\n## Include Dirs (by Target)"
foreach ($t in $targets) {
  $md += ('### `' + $t.name + '`')
  foreach ($i in $t.includeDirs) { $md += ('- `' + $i + '`') }
}
$mdPath = Join-Path $OutDir "reverse_cmake_summary.md"
$md -join "`n" | Out-File -Encoding UTF8 $mdPath

# DOT graph
$dot = @()
$dot += "digraph cmake {"
$dot += "  rankdir=LR; graph [fontname=Consolas]; node [shape=box, fontname=Consolas]; edge [fontname=Consolas];"
foreach ($t in $targets) {
  $safe = ($t.name -replace "[^A-Za-z0-9_]", "_")
  $dot += ('  ' + $safe + ' [label="' + $t.name + '\n' + $t.type + '"];')
}
foreach ($t in $targets) {
  $from = ($t.name -replace "[^A-Za-z0-9_]", "_")
  foreach ($d in ($t.dependencies | Where-Object { $_ })) {
    $to = ($d -replace "[^A-Za-z0-9_]", "_")
    $dot += ('  ' + $from + ' -> ' + $to + ';')
  }
}
$dot += "}"
$dotPath = Join-Path $OutDir "reverse_cmake_graph.dot"
$dot -join "`n" | Out-File -Encoding UTF8 $dotPath

Write-Host "[reverse-cmake] Wrote:" -ForegroundColor Green
Write-Host "  $targetsPath"
Write-Host "  $mdPath"
Write-Host "  $dotPath"
