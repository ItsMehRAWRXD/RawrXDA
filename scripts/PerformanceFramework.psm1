# Performance Framework for IDE Modules
# Provides caching, resource management, and lightweight profiling

param()

# ============================================================================
# CACHE STATE
# ============================================================================

$script:ModuleCache = @{}
$script:FileCache = @{}
$script:CacheStats = [ordered]@{
    ModuleHits = 0
    ModuleMisses = 0
    FileHits = 0
    FileMisses = 0
}

# ============================================================================
# MODULE CACHE
# ============================================================================

function Import-ModuleCached {
    <#
    .SYNOPSIS
    Imports a module once and reuses it for future calls
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [switch]$Force
    )

    if (-not $Force -and $script:ModuleCache.ContainsKey($Name)) {
        $script:CacheStats.ModuleHits++
        return $script:ModuleCache[$Name]
    }

    $script:CacheStats.ModuleMisses++
    $module = Import-Module -Name $Name -PassThru -Force:$Force -ErrorAction Stop
    $script:ModuleCache[$Name] = $module
    return $module
}

function Clear-ModuleCache {
    <#
    .SYNOPSIS
    Clears cached modules
    #>
    [CmdletBinding()]
    param()
    $script:ModuleCache.Clear()
}

# ============================================================================
# FILE CONTENT CACHE
# ============================================================================

function Get-CachedContent {
    <#
    .SYNOPSIS
    Returns cached file content keyed by path + last write time
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path $Path)) {
        throw "File not found: $Path"
    }

    $info = Get-Item $Path
    $key = "$Path|$($info.LastWriteTime.Ticks)"

    if ($script:FileCache.ContainsKey($key)) {
        $script:CacheStats.FileHits++
        return $script:FileCache[$key]
    }

    $script:CacheStats.FileMisses++
    $script:FileCache[$key] = Get-Content $Path -Raw

    if ($script:FileCache.Count -gt 200) {
        $keysToRemove = $script:FileCache.Keys | Select-Object -First ($script:FileCache.Count - 200)
        foreach ($oldKey in $keysToRemove) {
            $script:FileCache.Remove($oldKey)
        }
    }

    return $script:FileCache[$key]
}

function Clear-FileCache {
    <#
    .SYNOPSIS
    Clears cached file content
    #>
    [CmdletBinding()]
    param()
    $script:FileCache.Clear()
}

# ============================================================================
# LIGHTWEIGHT PROFILING
# ============================================================================

function Measure-Block {
    <#
    .SYNOPSIS
    Measures a scriptblock and returns elapsed milliseconds
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [ScriptBlock]$Block
    )

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $result = & $Block
    $sw.Stop()

    return [PSCustomObject]@{
        Result = $result
        ElapsedMs = $sw.ElapsedMilliseconds
    }
}

function Get-CacheStats {
    <#
    .SYNOPSIS
    Returns cache hit/miss counters
    #>
    [CmdletBinding()]
    param()
    return [PSCustomObject]$script:CacheStats
}

# ============================================================================
# EXPORTS
# ============================================================================

Export-ModuleMember -Function @(
    'Import-ModuleCached',
    'Clear-ModuleCache',
    'Get-CachedContent',
    'Clear-FileCache',
    'Measure-Block',
    'Get-CacheStats'
)
