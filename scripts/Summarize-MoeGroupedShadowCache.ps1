# Summarize MoE "shadow" grouped pack-cache telemetry and suggest tuning steps.
#
# Usage (inline snapshot from HUD / debugger):
#   .\Summarize-MoeGroupedShadowCache.ps1 -Hits 1200 -Misses 340 -SyncPackInserts 280 -Fallbacks 1520 `
#       -CacheEvictions 12 -CacheCapacity 16 [-PlanGeneration 7]
#
# Usage (JSON file):
#   .\Summarize-MoeGroupedShadowCache.ps1 -JsonPath D:\telemetry\moe_shadow_snapshot.json
#
# JSON shape: { "hits": 0, "misses": 0, "syncPackInserts": 0, "fallbacks": 0, "cacheEvictions": 0, "cacheCapacity": 16, "planGeneration": 0 }
param(
    [string] $JsonPath = "",
    [long] $Hits = -1,
    [long] $Misses = -1,
    [long] $SyncPackInserts = -1,
    [long] $Fallbacks = -1,
    [long] $CacheEvictions = -1,
    [int] $CacheCapacity = 16,
    [long] $PlanGeneration = -1
)

$ErrorActionPreference = "Stop"

function Read-JsonMetrics([string] $path) {
    $j = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    function Jl([string] $name, [long] $default) {
        if ($null -ne $j.PSObject.Properties[$name]) { return [long]$j.$name }
        return $default
    }
    function Ji([string] $name, [int] $default) {
        if ($null -ne $j.PSObject.Properties[$name]) { return [int]$j.$name }
        return $default
    }
    return @{
        Hits             = Jl "hits" 0
        Misses           = Jl "misses" 0
        SyncPackInserts  = Jl "syncPackInserts" 0
        Fallbacks        = Jl "fallbacks" 0
        CacheEvictions   = Jl "cacheEvictions" -1
        CacheCapacity    = Ji "cacheCapacity" 16
        PlanGeneration   = Jl "planGeneration" -1
    }
}

$m = $null
if ($JsonPath) {
    $m = Read-JsonMetrics $JsonPath
} else {
    if ($Hits -lt 0 -or $Misses -lt 0 -or $SyncPackInserts -lt 0 -or $Fallbacks -lt 0) {
        Write-Host @"
MoE shadow cache summary — provide counters or -JsonPath.

Example:
  .\Summarize-MoeGroupedShadowCache.ps1 -Hits 1200 -Misses 340 -SyncPackInserts 280 -Fallbacks 1520 -CacheEvictions 12 -CacheCapacity 16

Export JSON from your HUD / test harness:
  { "hits": 1200, "misses": 340, "syncPackInserts": 280, "fallbacks": 1520, "cacheEvictions": 12, "cacheCapacity": 16, "planGeneration": 42 }
"@
        exit 0
    }
    $m = @{
        Hits            = $Hits
        Misses          = $Misses
        SyncPackInserts = $SyncPackInserts
        Fallbacks       = $Fallbacks
        CacheEvictions  = $CacheEvictions
        CacheCapacity   = $CacheCapacity
        PlanGeneration  = $PlanGeneration
    }
}

$lookup = $m.Hits + $m.Misses
$hitRate = if ($lookup -gt 0) { 100.0 * $m.Hits / $lookup } else { 0.0 }
$fbPerK = if ($lookup -gt 0) { 1000.0 * $m.Fallbacks / $lookup } else { 0.0 }

Write-Host "=== MoE grouped shadow cache (dry-run path) ==="
if ($m.PlanGeneration -ge 0) { Write-Host ("planGeneration: {0}" -f $m.PlanGeneration) }
Write-Host ("cacheCapacity:  {0}" -f $m.CacheCapacity)
Write-Host ("hits:           {0}" -f $m.Hits)
Write-Host ("misses:         {0}" -f $m.Misses)
Write-Host ("syncPackInsert: {0}" -f $m.SyncPackInserts)
Write-Host ("fallbacks:      {0}" -f $m.Fallbacks)
if ($m.CacheEvictions -ge 0) { Write-Host ("LRU evictions:  {0}" -f $m.CacheEvictions) }
Write-Host ("tryGet hit rate (hits/(hits+misses)): {0:N2}%" -f $hitRate)
Write-Host ("fallbacks per 1000 cache lookups:    {0:N2}" -f $fbPerK)

Write-Host ""
Write-Host "--- Interpretation ---"
if ($lookup -eq 0) {
    Write-Host "No cache lookups recorded; enable moe_down_enable_grouped_integration and run MoE forward."
    exit 0
}

if ($hitRate -lt 5.0 -and $m.Misses -gt 50) {
    Write-Host "* Hit rate is very low — expert mixtures or plan rows churn faster than cache capacity."
    Write-Host "  Try: raise moe_down_grouped_pack_cache_max_entries; enable sync_pack_on_miss to warm entries."
}

if ($m.CacheEvictions -gt 0 -and $m.CacheEvictions -ge [math]::Floor($lookup / 20)) {
    Write-Host "* Evictions are frequent relative to lookups — working set likely exceeds LRU capacity."
    Write-Host "  Try: larger cache cap or tie eviction to planGeneration / resident slice retirement."
}

if ($m.SyncPackInserts -gt 0 -and $m.Hits -lt $m.SyncPackInserts) {
    Write-Host "* Sync packs occurred but hits lag inserts — mixtures may not repeat within cache lifetime."
    Write-Host "  Collect per-layer expert top-K histogram; consider async prepack on prefetch-complete."
}

if ($fbPerK -gt 900) {
    Write-Host "* Fallbacks dominate (expected in shadow mode: math still uses forwardMoEExpertSwiGLU)."
    Write-Host "  Before grouped math: ensure hits stabilize and pack cost (bench) beats looped path."
}

Write-Host ""
Write-Host "Suggested staging exports: moeGroupedPackCacheHits/Misses/SyncPackInserts/Fallbacks + swarm pin/eviction stats."
