param(
    [string]$Root = ".",
    [switch]$FailOnViolation,
    [switch]$Strict
)

$ErrorActionPreference = 'Stop'

function Test-IncludeViolation {
    param(
        [string]$Path,
        [string]$IncludeLine,
        [string]$LowerPath,
        [string]$LowerInclude,
        [bool]$StrictMode
    )

    $violations = @()

    # UI layer must never bind directly to kernel/transformer internals.
    if ($LowerPath -like '*\src\win32app\*') {
        if ($LowerInclude -match 'inference_kernels\.h' -or
            $LowerInclude -match 'rawrxd_transformer\.(h|hpp|cpp)' -or
            $LowerInclude -match 'engine/transformer\.(h|hpp|cpp)') {
            $violations += 'win32app_direct_compute_dependency'
        }

        # Optional stricter contract for sovereign layering.
        if ($StrictMode -and $LowerInclude -match 'rawrxd_model_loader\.h') {
            $violations += 'win32app_direct_loader_dependency'
        }
    }

    # Agentic layer must not include compute internals or loader internals.
    if ($LowerPath -like '*\src\agentic\*') {
        if ($LowerInclude -match 'inference_kernels\.h' -or
            $LowerInclude -match 'rawrxd_transformer\.(h|hpp|cpp)' -or
            $LowerInclude -match 'rawrxd_model_loader\.h') {
            $violations += 'agentic_direct_compute_or_loader_dependency'
        }
    }

    return $violations
}

$rootPath = (Resolve-Path $Root).Path
$sourceRoot = Join-Path $rootPath 'src'

if (-not (Test-Path -LiteralPath $sourceRoot)) {
    Write-Error "Could not locate src directory under: $rootPath"
    exit 2
}

$files = Get-ChildItem -Path $sourceRoot -Recurse -File |
    Where-Object {
        @('.cpp', '.cc', '.c', '.h', '.hpp') -contains $_.Extension.ToLowerInvariant()
    }

$includeRegex = '^\s*#\s*include\s*[<"]([^>"]+)[>"]'
$hits = New-Object System.Collections.Generic.List[object]

foreach ($file in $files) {
    $rawLines = Get-Content -LiteralPath $file.FullName -Encoding UTF8
    $lowerPath = $file.FullName.ToLowerInvariant()
    for ($i = 0; $i -lt $rawLines.Count; $i++) {
        $line = $rawLines[$i]
        if ($line -notmatch $includeRegex) {
            continue
        }

        $includePath = $matches[1]
        $lowerInclude = $includePath.ToLowerInvariant()
        $violations = Test-IncludeViolation -Path $file.FullName -IncludeLine $line -LowerPath $lowerPath -LowerInclude $lowerInclude -StrictMode:$Strict
        foreach ($v in $violations) {
            $hits.Add([pscustomobject]@{
                Rule = $v
                Path = $file.FullName
                Line = $i + 1
                Include = $includePath
            })
        }
    }
}

if ($hits.Count -eq 0) {
    Write-Host 'OK: module boundary validation passed.'
    exit 0
}

Write-Host "Module boundary violations: $($hits.Count)"
$hits | Sort-Object Rule, Path, Line | Format-Table -AutoSize

if ($FailOnViolation) {
    exit 2
}

exit 0
