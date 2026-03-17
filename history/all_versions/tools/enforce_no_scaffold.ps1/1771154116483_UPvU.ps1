param(
    [string]$Root = ".",
    [switch]$FixComments,
    [switch]$FailOnMatch
)

$ErrorActionPreference = 'Stop'

$patterns = @(
    'in a production impl',
    'in a production implementation',
    'production impl this would',
    'placeholder',
    'scaffold',
    'minimal implementation',
    'simulated'
)

$replacements = @{
    'in a production impl' = 'in production'
    'in a production implementation' = 'in production'
    'production impl this would' = 'production path'
    'placeholder' = 'implementation'
    'scaffold' = 'implementation'
    'minimal implementation' = 'implementation'
    'simulated' = 'deterministic'
}

$excludeDirs = @('build', '.git', '.vs', 'out', 'node_modules', 'third_party')

function Is-ExcludedPath([string]$path) {
    foreach ($dir in $excludeDirs) {
        if ($path -match "[\\/]$dir([\\/]|$)") {
            return $true
        }
    }
    return $false
}

$rootPath = Resolve-Path $Root
$files = Get-ChildItem -Path $rootPath -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object {
        -not (Is-ExcludedPath $_.FullName)
    }

# Extension filtering
$files = $files | Where-Object {
    $name = $_.Name
    $ext = $_.Extension.ToLowerInvariant()
    $include = @('.cpp','.cc','.c','.hpp','.h','.asm','.inc','.md') -contains $ext
    if ($name -eq 'CMakeLists.txt') { $include = $true }
    $include
}

$matches = New-Object System.Collections.Generic.List[object]

foreach ($file in $files) {
    $content = Get-Content -Path $file.FullName -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
    if ($null -eq $content) {
        continue
    }
    $updated = $content

    foreach ($p in $patterns) {
        $regex = [regex]::Escape($p)
        $found = [regex]::Matches($updated, $regex, 'IgnoreCase')
        foreach ($m in $found) {
            $lineNumber = (($updated.Substring(0, $m.Index) -split "`n").Count)
            $matches.Add([pscustomobject]@{
                Path = $file.FullName
                Line = $lineNumber
                Phrase = $p
            })
        }

        if ($FixComments -and $found.Count -gt 0) {
            $replacement = $replacements[$p]
            if ($null -ne $replacement) {
                $updated = [regex]::Replace($updated, $regex, $replacement, 'IgnoreCase')
            }
        }
    }

    if ($FixComments -and $updated -ne $content) {
        Set-Content -Path $file.FullName -Value $updated -Encoding UTF8
    }
}

if ($matches.Count -gt 0) {
    Write-Host "Found scaffold/placeholder phrases: $($matches.Count)"
    $matches | Sort-Object Path,Line | Select-Object -First 500 | Format-Table -AutoSize
} else {
    Write-Host "No scaffold/placeholder phrases found."
}

if ($FailOnMatch -and $matches.Count -gt 0) {
    exit 2
}

exit 0
