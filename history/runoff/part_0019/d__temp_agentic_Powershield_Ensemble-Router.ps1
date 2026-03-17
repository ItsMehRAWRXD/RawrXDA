<#
.SYNOPSIS
    3-Model Ensemble Router — auto-routes queries to the optimal model tier.

.DESCRIPTION
    Analyzes query complexity and routes to:
      SMALL  (3.2B) — fast chat, simple questions, quick lookups
      MEDIUM (8B)   — code generation, scripting, file analysis
      LARGE  (27B)  — deep reasoning, architecture, complex debugging

    Complexity is scored by keyword analysis, query length, code detection,
    and explicit tier override commands.

.PARAMETER Query
    The prompt to route and execute.

.PARAMETER ForceTier
    Override auto-routing: Small, Medium, or Large.

.PARAMETER Interactive
    Enter interactive chat mode with auto-routing.

.PARAMETER Verbose
    Show routing decision details.

.EXAMPLE
    .\Ensemble-Router.ps1 'What is RawrXD?'
    .\Ensemble-Router.ps1 'Write a PowerShell function to hash all files in a directory' -Verbose
    .\Ensemble-Router.ps1 -ForceTier Large 'Design a distributed inference pipeline for 70B models'
    .\Ensemble-Router.ps1 -Interactive
#>

[CmdletBinding()] param(
  [Parameter(Position = 0)]
  [string]$Query,

  [ValidateSet('Small','Medium','Large','Auto')]
  [string]$ForceTier = 'Auto',

  [switch]$Interactive,

  [string]$OllamaServer = 'localhost:11434',

  [string]$SmallModel  = 'bigdaddyg-tier-small',
  [string]$MediumModel = 'bigdaddyg-tier-medium',
  [string]$LargeModel  = 'bigdaddyg-tier-large',

  # Fallbacks if tier models don't exist
  [string]$SmallFallback  = 'bigdaddyg-comprehensive:latest',
  [string]$MediumFallback = 'qwen2.5-coder:latest',
  [string]$LargeFallback  = 'gemma3:27b'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Continue'

# ─── Helpers ──────────────────────────────────────────────────────────
function Test-OllamaRunning {
  try {
    $null = Invoke-RestMethod -Uri "http://$OllamaServer/api/version" -TimeoutSec 5
    return $true
  } catch { return $false }
}

function Test-ModelExists {
  param([string]$Name)
  try {
    $resp = Invoke-RestMethod -Uri "http://$OllamaServer/api/tags" -TimeoutSec 10
    return ($resp.models | Where-Object { $_.name -eq $Name -or $_.name -eq "$Name`:latest" }).Count -gt 0
  } catch { return $false }
}

function Resolve-Model {
  param([string]$Primary, [string]$Fallback)
  if (Test-ModelExists $Primary) { return $Primary }
  if (Test-ModelExists $Fallback) {
    Write-Host "  ⚠️  $Primary not found, using fallback: $Fallback" -ForegroundColor Yellow
    return $Fallback
  }
  Write-Host "  ❌ Neither $Primary nor $Fallback found!" -ForegroundColor Red
  return $null
}

# ─── Complexity Scoring Engine ────────────────────────────────────────
function Get-QueryComplexity {
  param([string]$Text)

  $score = 0
  $reasons = [System.Collections.ArrayList]::new()

  # Length-based scoring
  $wordCount = ($Text -split '\s+').Count
  if ($wordCount -gt 80)     { $score += 30; [void]$reasons.Add("long query ($wordCount words)") }
  elseif ($wordCount -gt 30) { $score += 15; [void]$reasons.Add("medium query ($wordCount words)") }
  else                        { $score += 0 }

  # Code detection (needs MEDIUM or higher)
  $codePatterns = @(
    'function\s+\w+',
    'def\s+\w+',
    'class\s+\w+',
    '\{[\s\S]*\}',
    'import\s+\w+',
    'require\s*\(',
    '#include\s*[<"]',
    '\$\w+\s*=',
    'for\s*\(',
    'while\s*\(',
    'if\s*\(',
    'try\s*\{',
    'catch\s*\(',
    '=>',
    'async\s+',
    'await\s+'
  )
  $codeHits = 0
  foreach ($p in $codePatterns) {
    if ($Text -match $p) { $codeHits++ }
  }
  if ($codeHits -ge 3)  { $score += 25; [void]$reasons.Add("code block detected ($codeHits patterns)") }
  elseif ($codeHits -ge 1) { $score += 10; [void]$reasons.Add("code reference ($codeHits patterns)") }

  # Keyword-based complexity escalation
  $highComplexityKeywords = @(
    'architect', 'design', 'refactor', 'optimize', 'distributed',
    'trade-off', 'tradeoff', 'compare', 'analyze', 'debug',
    'why does', 'explain why', 'step by step', 'reasoning',
    'vulnerability', 'exploit', 'reverse engineer', 'disassembl',
    'memory leak', 'race condition', 'deadlock', 'concurren',
    'pipeline', 'infrastructure', 'scale', 'performance',
    'multi-gpu', 'multi-drive', 'distributed inference',
    'edge case', 'failure mode', 'root cause'
  )
  $mediumComplexityKeywords = @(
    'write', 'create', 'implement', 'build', 'generate',
    'function', 'script', 'class', 'module', 'component',
    'fix', 'patch', 'update', 'modify', 'change',
    'convert', 'parse', 'transform', 'migrate',
    'test', 'validate', 'benchmark', 'profile',
    'deploy', 'compile', 'package', 'install',
    'powershell', 'python', 'javascript', 'typescript',
    'assembly', 'rust', 'c\+\+', 'csharp'
  )

  $lowerText = $Text.ToLower()
  $highHits = 0
  foreach ($kw in $highComplexityKeywords) {
    if ($lowerText -match $kw) { $highHits++ }
  }
  if ($highHits -ge 3)      { $score += 40; [void]$reasons.Add("high-complexity keywords ($highHits)") }
  elseif ($highHits -ge 1)  { $score += 20; [void]$reasons.Add("some complex keywords ($highHits)") }

  $medHits = 0
  foreach ($kw in $mediumComplexityKeywords) {
    if ($lowerText -match $kw) { $medHits++ }
  }
  if ($medHits -ge 3)       { $score += 15; [void]$reasons.Add("coding keywords ($medHits)") }
  elseif ($medHits -ge 1)   { $score += 5;  [void]$reasons.Add("some coding keywords ($medHits)") }

  # Multi-part question detection
  $questionMarks = ([regex]::Matches($Text, '\?')).Count
  $numberedSteps = ([regex]::Matches($Text, '^\s*\d+[\.\)]\s', [System.Text.RegularExpressions.RegexOptions]::Multiline)).Count
  if ($questionMarks -ge 3 -or $numberedSteps -ge 3) {
    $score += 20; [void]$reasons.Add("multi-part question ($questionMarks questions, $numberedSteps steps)")
  }

  # Explicit tier commands
  if ($lowerText -match '^\s*(hi|hello|hey|sup|yo|thanks|thank you|ok|okay|yes|no|sure)\s*$') {
    $score = 0; $reasons.Clear(); [void]$reasons.Add("simple greeting/acknowledgment")
  }

  # Determine tier
  $tier = if ($score -ge 50) { 'Large' }
          elseif ($score -ge 20) { 'Medium' }
          else { 'Small' }

  return @{
    Score   = $score
    Tier    = $tier
    Reasons = $reasons
  }
}

function Get-TierColor {
  param([string]$Tier)
  switch ($Tier) {
    'Small'  { 'Green' }
    'Medium' { 'Yellow' }
    'Large'  { 'Magenta' }
    default  { 'White' }
  }
}

function Get-TierEmoji {
  param([string]$Tier)
  switch ($Tier) {
    'Small'  { '⚡' }
    'Medium' { '🔧' }
    'Large'  { '🧠' }
    default  { '❓' }
  }
}

function Invoke-ModelQuery {
  param([string]$Model, [string]$Prompt, [string]$Tier)

  $emoji = Get-TierEmoji $Tier
  $color = Get-TierColor $Tier

  Write-Host ""
  Write-Host "  $emoji [$Tier] → $Model" -ForegroundColor $color

  $body = @{
    model   = $Model
    prompt  = $Prompt
    stream  = $false
    options = @{
      temperature = switch ($Tier) { 'Small' { 0.3 } 'Medium' { 0.3 } 'Large' { 0.4 } default { 0.3 } }
      num_predict = switch ($Tier) { 'Small' { 1024 } 'Medium' { 4096 } 'Large' { 4096 } default { 2048 } }
    }
  } | ConvertTo-Json -Depth 3

  $startTime = Get-Date
  try {
    $resp = Invoke-RestMethod -Uri "http://$OllamaServer/api/generate" -Method POST -Body $body -ContentType "application/json" -TimeoutSec 300
    $elapsed = (Get-Date) - $startTime
    $tokPerSec = if ($resp.eval_count -and $resp.eval_duration) {
      [math]::Round($resp.eval_count / ($resp.eval_duration / 1e9), 1)
    } else { '?' }

    Write-Host ""
    Write-Host $resp.response
    Write-Host ""
    Write-Host "  ─── Stats ───────────────────────────────────────────" -ForegroundColor DarkGray
    Write-Host "  Tier: $Tier | Model: $Model" -ForegroundColor DarkGray
    Write-Host "  Time: $([math]::Round($elapsed.TotalSeconds, 1))s | Tokens: $($resp.eval_count) | Speed: $tokPerSec tok/s" -ForegroundColor DarkGray
    Write-Host "  ────────────────────────────────────────────────────" -ForegroundColor DarkGray

    return $resp
  } catch {
    Write-Host "  ❌ Error: $($_.Exception.Message)" -ForegroundColor Red
    return $null
  }
}

# ─── Resolve available models ────────────────────────────────────────
$resolvedSmall  = Resolve-Model $SmallModel  $SmallFallback
$resolvedMedium = Resolve-Model $MediumModel $MediumFallback
$resolvedLarge  = Resolve-Model $LargeModel  $LargeFallback

if (-not $resolvedSmall -and -not $resolvedMedium -and -not $resolvedLarge) {
  Write-Host "❌ No models available. Run Upgrade-Model-Pipeline.ps1 -Mode C first." -ForegroundColor Red
  exit 1
}

# Map tier to resolved model
$tierModelMap = @{
  'Small'  = $resolvedSmall
  'Medium' = $resolvedMedium
  'Large'  = $resolvedLarge
}

# ─── Single Query Mode ───────────────────────────────────────────────
function Invoke-SingleQuery {
  param([string]$Text)

  if ($ForceTier -ne 'Auto') {
    $tier = $ForceTier
    $model = $tierModelMap[$tier]
    if (-not $model) {
      Write-Host "  ❌ No model available for tier: $tier" -ForegroundColor Red
      return
    }
    Write-Host "  🎯 Forced tier: $tier" -ForegroundColor (Get-TierColor $tier)
  } else {
    $analysis = Get-QueryComplexity $Text
    $tier = $analysis.Tier
    $model = $tierModelMap[$tier]

    # Fallback chain: if the ideal tier model is missing, try the next lower
    if (-not $model) {
      if ($tier -eq 'Large' -and $resolvedMedium) {
        $model = $resolvedMedium; $tier = 'Medium'
        Write-Host "  ⚠️  Large model unavailable, falling back to Medium" -ForegroundColor Yellow
      } elseif (($tier -eq 'Large' -or $tier -eq 'Medium') -and $resolvedSmall) {
        $model = $resolvedSmall; $tier = 'Small'
        Write-Host "  ⚠️  Falling back to Small model" -ForegroundColor Yellow
      }
    }

    if ($VerbosePreference -eq 'Continue' -or $PSBoundParameters.ContainsKey('Verbose')) {
      Write-Host ""
      Write-Host "  🔍 Routing Analysis:" -ForegroundColor Cyan
      Write-Host "     Complexity Score: $($analysis.Score)" -ForegroundColor Gray
      Write-Host "     Tier Decision  : $tier" -ForegroundColor (Get-TierColor $tier)
      foreach ($r in $analysis.Reasons) {
        Write-Host "     • $r" -ForegroundColor DarkGray
      }
    }
  }

  Invoke-ModelQuery -Model $model -Prompt $Text -Tier $tier
}

# ─── Interactive Mode ─────────────────────────────────────────────────
function Invoke-InteractiveChat {
  Write-Host ""
  Write-Host "╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
  Write-Host "║  🌐  ENSEMBLE ROUTER — Interactive Mode                     ║" -ForegroundColor Cyan
  Write-Host "║  Queries auto-route to the best model by complexity.        ║" -ForegroundColor Cyan
  Write-Host "╠═══════════════════════════════════════════════════════════════╣" -ForegroundColor Cyan
  Write-Host "║  ⚡ Small  : $($resolvedSmall ?? 'UNAVAILABLE')$((' ' * [Math]::Max(0, 46 - ($resolvedSmall ?? 'UNAVAILABLE').Length)))║" -ForegroundColor Green
  Write-Host "║  🔧 Medium : $($resolvedMedium ?? 'UNAVAILABLE')$((' ' * [Math]::Max(0, 46 - ($resolvedMedium ?? 'UNAVAILABLE').Length)))║" -ForegroundColor Yellow
  Write-Host "║  🧠 Large  : $($resolvedLarge ?? 'UNAVAILABLE')$((' ' * [Math]::Max(0, 46 - ($resolvedLarge ?? 'UNAVAILABLE').Length)))║" -ForegroundColor Magenta
  Write-Host "╠═══════════════════════════════════════════════════════════════╣" -ForegroundColor Cyan
  Write-Host "║  Commands: /small /medium /large — force tier               ║" -ForegroundColor DarkGray
  Write-Host "║            /auto — resume auto-routing                      ║" -ForegroundColor DarkGray
  Write-Host "║            /stats — show session stats                      ║" -ForegroundColor DarkGray
  Write-Host "║            /quit — exit                                     ║" -ForegroundColor DarkGray
  Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
  Write-Host ""

  $sessionStats = @{
    SmallQueries  = 0
    MediumQueries = 0
    LargeQueries  = 0
    TotalTokens   = 0
    TotalTime     = [TimeSpan]::Zero
    StartTime     = Get-Date
  }
  $currentForceTier = 'Auto'

  while ($true) {
    $tierLabel = if ($currentForceTier -ne 'Auto') { "[$currentForceTier]" } else { "[Auto]" }
    Write-Host "  $tierLabel " -NoNewline -ForegroundColor (if ($currentForceTier -ne 'Auto') { Get-TierColor $currentForceTier } else { 'Cyan' })
    $input = Read-Host "You"

    if ([string]::IsNullOrWhiteSpace($input)) { continue }

    # Handle commands
    switch -Regex ($input.Trim()) {
      '^/quit$|^/exit$|^/q$' {
        Write-Host ""
        Write-Host "  👋 Session ended." -ForegroundColor Gray
        Write-Host "     Queries: S=$($sessionStats.SmallQueries) M=$($sessionStats.MediumQueries) L=$($sessionStats.LargeQueries)" -ForegroundColor DarkGray
        Write-Host "     Total tokens: $($sessionStats.TotalTokens)" -ForegroundColor DarkGray
        Write-Host "     Session time: $([math]::Round(((Get-Date) - $sessionStats.StartTime).TotalMinutes, 1)) min" -ForegroundColor DarkGray
        return
      }
      '^/small$'  { $currentForceTier = 'Small';  Write-Host "  ⚡ Tier locked: Small" -ForegroundColor Green;  continue }
      '^/medium$' { $currentForceTier = 'Medium'; Write-Host "  🔧 Tier locked: Medium" -ForegroundColor Yellow; continue }
      '^/large$'  { $currentForceTier = 'Large';  Write-Host "  🧠 Tier locked: Large" -ForegroundColor Magenta; continue }
      '^/auto$'   { $currentForceTier = 'Auto';   Write-Host "  🌐 Auto-routing enabled" -ForegroundColor Cyan;  continue }
      '^/stats$' {
        $elapsed = (Get-Date) - $sessionStats.StartTime
        Write-Host ""
        Write-Host "  📊 Session Statistics:" -ForegroundColor Cyan
        Write-Host "     Small queries  : $($sessionStats.SmallQueries)" -ForegroundColor Green
        Write-Host "     Medium queries : $($sessionStats.MediumQueries)" -ForegroundColor Yellow
        Write-Host "     Large queries  : $($sessionStats.LargeQueries)" -ForegroundColor Magenta
        Write-Host "     Total tokens   : $($sessionStats.TotalTokens)" -ForegroundColor Gray
        Write-Host "     Session time   : $([math]::Round($elapsed.TotalMinutes, 1)) min" -ForegroundColor Gray
        Write-Host ""
        continue
      }
    }

    # Route and execute
    if ($currentForceTier -ne 'Auto') {
      $tier = $currentForceTier
    } else {
      $analysis = Get-QueryComplexity $input
      $tier = $analysis.Tier
      Write-Host "  🔍 Routed → $tier (score: $($analysis.Score))" -ForegroundColor DarkGray
    }

    $model = $tierModelMap[$tier]
    if (-not $model) {
      # Fallback
      $model = $resolvedSmall ?? $resolvedMedium ?? $resolvedLarge
      $tier = 'Small'
    }

    $resp = Invoke-ModelQuery -Model $model -Prompt $input -Tier $tier

    # Update stats
    switch ($tier) {
      'Small'  { $sessionStats.SmallQueries++ }
      'Medium' { $sessionStats.MediumQueries++ }
      'Large'  { $sessionStats.LargeQueries++ }
    }
    if ($resp -and $resp.eval_count) {
      $sessionStats.TotalTokens += $resp.eval_count
    }
  }
}

# ═══════════════════════════════════════════════════════════════════════
#  MAIN
# ═══════════════════════════════════════════════════════════════════════

if (-not (Test-OllamaRunning)) {
  Write-Host "❌ Ollama is not running at $OllamaServer" -ForegroundColor Red
  Write-Host "   Start it with: ollama serve" -ForegroundColor Yellow
  exit 1
}

if ($Interactive) {
  Invoke-InteractiveChat
} elseif (-not [string]::IsNullOrWhiteSpace($Query)) {
  Invoke-SingleQuery $Query
} else {
  Write-Host ""
  Write-Host "  Ensemble Router — No query provided." -ForegroundColor Yellow
  Write-Host ""
  Write-Host "  Usage:" -ForegroundColor Gray
  Write-Host "    .\Ensemble-Router.ps1 'your question here'" -ForegroundColor White
  Write-Host "    .\Ensemble-Router.ps1 -Interactive" -ForegroundColor White
  Write-Host "    .\Ensemble-Router.ps1 -ForceTier Large 'complex question'" -ForegroundColor White
  Write-Host "    .\Ensemble-Router.ps1 -Verbose 'show routing logic'" -ForegroundColor White
  Write-Host ""
  Write-Host "  Available models:" -ForegroundColor Gray
  Write-Host "    ⚡ Small  : $($resolvedSmall ?? '(none)')" -ForegroundColor Green
  Write-Host "    🔧 Medium : $($resolvedMedium ?? '(none)')" -ForegroundColor Yellow
  Write-Host "    🧠 Large  : $($resolvedLarge ?? '(none)')" -ForegroundColor Magenta
  Write-Host ""
}
