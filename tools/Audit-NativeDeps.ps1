param(
  [string]$RepoRoot = $PSScriptRoot | Split-Path -Parent,
  [string]$OutJson = (Join-Path ($PSScriptRoot | Split-Path -Parent) "native_audit.json")
)

$ErrorActionPreference = "Stop"

$cmakePatterns = @(
  'find_package\s*\(',
  'FetchContent_(Declare|MakeAvailable)\s*\(',
  'ExternalProject_Add\s*\(',
  'add_subdirectory\s*\(',
  'vcpkg|conan|hunter|CPM\.cmake'
)

$banIncludePatterns = @(
  '^\s*#\s*include\s*[<"](Qt|Q[A-Za-z]+)',
  '^\s*#\s*include\s*[<"](vulkan|Vulkan)',
  '^\s*#\s*include\s*[<"](ggml|gguf|llama|glm|fmt|spdlog|nlohmann|asio|boost|curl|opencv)'
)

$banStringPatterns = @(
  '\bstd::filesystem\b',
  '\bQApplication\b|\bQWidget\b|\bQObject\b|\bQMainWindow\b',
  '\bvk[A-Z_]\w*\b|\bVulkan\b',
  '\bggml\b|\bGGML\b|\bllama\b|\bGGUF\b'
)

function Find-MatchesInFiles {
  param([string[]]$Files, [string[]]$Patterns)
  $hits = @()
  foreach ($file in $Files) {
    $content = Get-Content -LiteralPath $file -Raw -ErrorAction SilentlyContinue
    if ($null -eq $content) { continue }
    foreach ($pat in $Patterns) {
      $ms = [regex]::Matches($content, $pat, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase -bor [System.Text.RegularExpressions.RegexOptions]::Multiline)
      foreach ($m in $ms) {
        $hits += [pscustomobject]@{ File=$file; Pattern=$pat; Index=$m.Index; Snippet=($content.Substring([Math]::Max(0,$m.Index-60), [Math]::Min(200, $content.Length-[Math]::Max(0,$m.Index-60)))) }
      }
    }
  }
  $hits
}

$cmakeFiles = Get-ChildItem -LiteralPath $RepoRoot -Recurse -File -Include "CMakeLists.txt","*.cmake" -ErrorAction SilentlyContinue | ForEach-Object FullName
$codeFiles  = Get-ChildItem -LiteralPath $RepoRoot -Recurse -File -ErrorAction SilentlyContinue |
  Where-Object { $_.Extension -in ".c",".cc",".cpp",".cxx",".h",".hpp",".inl",".ipp",".asm",".inc" } |
  ForEach-Object FullName

$gitmodules = Join-Path $RepoRoot ".gitmodules"

$cmakeHits = Find-MatchesInFiles -Files $cmakeFiles -Patterns $cmakePatterns

$includeHits = @()
foreach ($file in $codeFiles) {
  $lines = Get-Content -LiteralPath $file -ErrorAction SilentlyContinue
  if (-not $lines) { continue }
  for ($i=0; $i -lt $lines.Count; $i++) {
    foreach ($pat in $banIncludePatterns) {
      if ($lines[$i] -match $pat) {
        $includeHits += [pscustomobject]@{ File=$file; Line=($i+1); Pattern=$pat; Text=$lines[$i].Trim() }
      }
    }
  }
}

$stringHits = Find-MatchesInFiles -Files $codeFiles -Patterns $banStringPatterns

$result = [pscustomobject]@{
  RepoRoot    = $RepoRoot
  Timestamp   = (Get-Date).ToString("o")
  GitSubmodules = (Test-Path $gitmodules)
  CMakeHits   = $cmakeHits
  IncludeHits = $includeHits
  StringHits  = $stringHits
}

$result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $OutJson -Encoding UTF8

Write-Host "Wrote: $OutJson" -ForegroundColor Green
Write-Host ("CMake hits    : {0}" -f $cmakeHits.Count)
Write-Host ("Include hits  : {0}" -f $includeHits.Count)
Write-Host ("String hits   : {0}" -f $stringHits.Count)
Write-Host ("Has .gitmodules: {0}" -f (Test-Path $gitmodules))
