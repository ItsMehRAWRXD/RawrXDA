param(
    [string]$RepoRoot = ".",
    [int]$BatchSize = 500
)

Set-Location $RepoRoot

function Get-GitRelativePath {
    param([string]$Path)
    $full = [System.IO.Path]::GetFullPath($Path)
    $root = (Get-Location).Path
    $rootWithSep = $root.TrimEnd('\') + '\'
    if ($full.StartsWith($rootWithSep, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $full.Substring($rootWithSep.Length).Replace('\', '/')
    }
    return $full.Replace('\', '/')
}

function Test-GitTracked {
    param([string]$Path)
    $gitPath = Get-GitRelativePath -Path $Path
    git ls-files --error-unmatch -- "$gitPath" *> $null
    return $LASTEXITCODE -eq 0
}

function Move-DirectFiles {
    param(
        [string]$BaseDir,
        [string[]]$Extensions,
        [string]$RunoffRoot,
        [int]$BatchSize = 500
    )

    if (-not (Test-Path $BaseDir)) {
        Write-Host "[SKIP] Missing directory: $BaseDir"
        return
    }

    $files = Get-ChildItem -Path $BaseDir -File -Force | Where-Object {
        if ($BaseDir -eq "." -and $_.Name -like "CMakeLists*.txt") {
            return $false
        }
        if (-not $Extensions -or $Extensions.Count -eq 0) {
            return $true
        }
        $ext = $_.Extension.ToLowerInvariant()
        return $Extensions -contains $ext
    } | Sort-Object Name

    if ($files.Count -eq 0) {
        Write-Host "[SKIP] No matching files in $BaseDir"
        return
    }

    Write-Host "[MOVE] $($files.Count) files from $BaseDir -> $RunoffRoot"

    $idx = 0
    foreach ($file in $files) {
        $part = [int][math]::Floor($idx / $BatchSize) + 1
        $destDir = Join-Path $BaseDir (Join-Path $RunoffRoot (("part_{0:D4}" -f $part)))
        New-Item -ItemType Directory -Path $destDir -Force | Out-Null

        $src = $file.FullName
        if (-not (Test-Path -LiteralPath $src)) {
            continue
        }

        $targetName = $file.Name
        $dst = Join-Path $destDir $targetName
        if (Test-Path -LiteralPath $dst) {
            $base = [System.IO.Path]::GetFileNameWithoutExtension($targetName)
            $extn = [System.IO.Path]::GetExtension($targetName)
            $suffix = [Guid]::NewGuid().ToString("N").Substring(0, 8)
            $targetName = "${base}_$suffix$extn"
            $dst = Join-Path $destDir $targetName
        }

        $isTracked = Test-GitTracked -Path $src
        if ($isTracked) {
            $srcGit = Get-GitRelativePath -Path $src
            $dstGit = Get-GitRelativePath -Path $dst
            git mv -- "$srcGit" "$dstGit"
            if ($LASTEXITCODE -ne 0) {
                Write-Warning "git mv failed: $src -> $dst"
                continue
            }
        } else {
            try {
                Move-Item -LiteralPath $src -Destination $dst -Force -ErrorAction Stop
            } catch {
                Write-Warning "Move-Item failed: $src -> $dst : $($_.Exception.Message)"
                continue
            }
        }
        $idx++
    }
}

# 1) history: flatten massive loose file pile into runoff batches
Move-DirectFiles -BaseDir "history" -Extensions @() -RunoffRoot "runoff" -BatchSize $BatchSize

# 2) repo root: move doc/text overflow to runoff
Move-DirectFiles -BaseDir "." -Extensions @(".md", ".txt") -RunoffRoot "runoff/root_docs_txt" -BatchSize $BatchSize

# 3) Full Source root: move doc/text/ps1 overflow to runoff
Move-DirectFiles -BaseDir "Full Source" -Extensions @(".md", ".txt", ".ps1") -RunoffRoot "runoff/docs_txt_ps1" -BatchSize $BatchSize

# 4) archived orphans: move non-core extensions to runoff to keep dir under cap
Move-DirectFiles -BaseDir ".archived_orphans" -Extensions @(".hpp", ".asm", ".c") -RunoffRoot "runoff/code_overflow" -BatchSize $BatchSize

Write-Host "[DONE] Overflow packing completed."
