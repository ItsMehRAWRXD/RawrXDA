#!/usr/bin/env pwsh
# Swarm Control Center - Model Sources Module
# Provides listing and pull commands for models from Local GGUF, Hugging Face, and Blob URLs.

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# Root paths (aligned with swarm_control_center.ps1)
$script:SwarmRoot = "D:\lazy init ide"
$script:ConfigDir = Join-Path $SwarmRoot "logs/swarm_config"
$script:BeaconDir = Join-Path $SwarmRoot "logs/swarm_beacon"
$script:MemoryDir = Join-Path $SwarmRoot "logs/swarm_memory"
$script:ModelsConfigFile = Join-Path $ConfigDir "models.json"
$script:ModelSourcesFile = Join-Path $ConfigDir "model_sources.json"

# Ensure config directory exists
if (-not (Test-Path $ConfigDir)) { New-Item -Path $ConfigDir -ItemType Directory -Force | Out-Null }

# Defaults for model sources
$script:DefaultModelSources = @{
    LocalGGUF = @{
        Roots = @()
    }
    HuggingFace = @{
        Enabled = $true
        Token = ""
        DefaultSearch = "gguf"
        MaxResults = 10
        DownloadDir = ""
    }
    Blobs = @{
        Entries = @()
        DefaultDir = ""
    }
}

function Get-ModelSourcesConfig {
    if (Test-Path $ModelSourcesFile) {
        try { return Get-Content $ModelSourcesFile -Raw | ConvertFrom-Json -AsHashtable } catch {}
    }
    $DefaultModelSources | ConvertTo-Json -Depth 10 | Set-Content -Path $ModelSourcesFile -Encoding UTF8
    return $DefaultModelSources
}

function Save-ModelSourcesConfig {
    param([hashtable]$Sources)
    $Sources | ConvertTo-Json -Depth 10 | Set-Content -Path $ModelSourcesFile -Encoding UTF8
}

function Get-ModelsConfig {
    if (Test-Path $ModelsConfigFile) {
        try { return Get-Content $ModelsConfigFile -Raw | ConvertFrom-Json -AsHashtable } catch {}
    }
    return @{}
}

# ──────────────────────────────────────────────────────────────────────────────
# Local GGUF discovery
# ──────────────────────────────────────────────────────────────────────────────
function Find-LocalGGUFModels {
    param([string[]]$Roots)
    $files = @()
    foreach ($root in $Roots) {
        if (-not $root) { continue }
        if (Test-Path $root) {
            try {
                $files += Get-ChildItem -Path $root -Recurse -File -Filter '*.gguf' -ErrorAction SilentlyContinue | ForEach-Object {
                    [pscustomobject]@{
                        Name = $_.Name
                        FullPath = $_.FullName
                        SizeMB = [math]::Round($_.Length / 1MB, 1)
                    }
                }
            } catch {}
        }
    }
    return $files | Sort-Object Name
}

# ──────────────────────────────────────────────────────────────────────────────
# HuggingFace API search and download (public models)
# ──────────────────────────────────────────────────────────────────────────────
function Search-HuggingFaceModels {
    param(
        [string]$Query = 'gguf',
        [int]$MaxResults = 10,
        [string]$Token = ''
    )
    $headers = @{ 'User-Agent' = 'RawrXD-SwarmControl/1.0' }
    if ($Token) { $headers['Authorization'] = "Bearer $Token" }

    $modelsList = @()
    try {
        $url = "https://huggingface.co/api/models?search=$([uri]::EscapeDataString($Query))"
        $res = Invoke-RestMethod -Uri $url -Headers $headers -TimeoutSec 30 -Method Get
    } catch {
        return @()
    }

    foreach ($m in ($res | Select-Object -First $MaxResults)) {
        $repo = $m.modelId
        if (-not $repo) { $repo = $m.id }
        if (-not $repo) { continue }
        try {
            $md = Invoke-RestMethod -Uri "https://huggingface.co/api/models/$repo" -Headers $headers -TimeoutSec 30 -Method Get
        } catch { continue }
        $ggufs = @($md.siblings | Where-Object { $_.rfilename -like '*.gguf' })
        if ($ggufs.Count -gt 0) {
            $modelsList += [pscustomobject]@{
                Repo = $repo
                Title = $md.cardData.short_description
                GGUFs = $ggufs
            }
        }
    }
    return $modelsList
}

function Download-HuggingFaceFile {
    param(
        [string]$Repo,
        [string]$RFileName,
        [string]$TargetDir,
        [string]$Token = ''
    )
    if (-not (Test-Path $TargetDir)) { New-Item -Path $TargetDir -ItemType Directory -Force | Out-Null }
    $headers = @{ 'User-Agent' = 'RawrXD-SwarmControl/1.0' }
    if ($Token) { $headers['Authorization'] = "Bearer $Token" }

    $urlCandidates = @(
        "https://huggingface.co/$Repo/resolve/main/$RFileName",
        "https://huggingface.co/$Repo/resolve/master/$RFileName"
    )

    $outPath = Join-Path $TargetDir (Split-Path $RFileName -Leaf)
    foreach ($u in $urlCandidates) {
        try {
            Invoke-WebRequest -Uri $u -Headers $headers -OutFile $outPath -UseBasicParsing -TimeoutSec 180
            return $outPath
        } catch {}
    }
    throw "Download failed for $Repo/$RFileName"
}

# ──────────────────────────────────────────────────────────────────────────────
# Assign downloaded or local file to a model entry
# ──────────────────────────────────────────────────────────────────────────────
function Assign-ModelFileToEntry {
    param([string]$ModelName, [string]$FilePath)
    $models = Get-ModelsConfig
    if (-not $models.ContainsKey($ModelName)) { throw "Unknown model entry: $ModelName" }
    $models[$ModelName].GGUFPath = $FilePath
    $models | ConvertTo-Json -Depth 10 | Set-Content -Path $ModelsConfigFile -Encoding UTF8
}

# ──────────────────────────────────────────────────────────────────────────────
# Interactive UI for managing model sources
# ──────────────────────────────────────────────────────────────────────────────
function Show-ModelSources {
    $sources = Get-ModelSourcesConfig
    $models = Get-ModelsConfig

    while ($true) {
        Clear-Host
        Write-Host ""; Write-Host "╔═══════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor DarkCyan
        Write-Host "║                         MODEL SOURCES MANAGEMENT                              ║" -ForegroundColor DarkCyan
        Write-Host "╚═══════════════════════════════════════════════════════════════════════════════╝" -ForegroundColor DarkCyan
        Write-Host ""
        Write-Host "[1] Local GGUF - Roots: $(@($sources.LocalGGUF.Roots) -join '; ')" -ForegroundColor Yellow
        Write-Host "[2] Hugging Face - Enabled: $($sources.HuggingFace.Enabled) DefaultSearch: $($sources.HuggingFace.DefaultSearch)" -ForegroundColor Yellow
        Write-Host "[3] Blob URLs - Entries: $($sources.Blobs.Entries.Count)" -ForegroundColor Yellow
        Write-Host "[S] Save & Back" -ForegroundColor Gray
        Write-Host ""
        Write-Host "Select option: " -NoNewline -ForegroundColor Cyan
        $opt = Read-Host
        switch ($opt.ToUpper()) {
            '1' {
                Write-Host ""; Write-Host "Local GGUF roots (Enter to skip):" -ForegroundColor Cyan
                $newRoot = Read-Host "Add root path"
                if ($newRoot) { $sources.LocalGGUF.Roots += $newRoot }
                Write-Host "Scanning for .gguf files..." -ForegroundColor Gray
                $files = Find-LocalGGUFModels -Roots $sources.LocalGGUF.Roots
                $i = 1
                foreach ($f in $files) {
                    Write-Host "  [$i] $($f.Name) ($($f.SizeMB) MB)" -ForegroundColor Gray
                    $i++
                }
                if ($files.Count -gt 0) {
                    Write-Host "Select file to assign to model entry (number), or Enter to skip:" -ForegroundColor Cyan
                    $sel = Read-Host
                    if ($sel) {
                        $idx = [int]$sel - 1
                        if ($idx -ge 0 -and $idx -lt $files.Count) {
                            $filePath = $files[$idx].FullPath
                            Write-Host "Available model entries:" -ForegroundColor Yellow
                            $modelNames = @($models.Keys)
                            $j = 1; foreach ($mn in $modelNames) { Write-Host "  [$j] $mn" -ForegroundColor Gray; $j++ }
                            $mSel = Read-Host "Choose target model"
                            $mIdx = [int]$mSel - 1
                            if ($mIdx -ge 0 -and $mIdx -lt $modelNames.Count) {
                                Assign-ModelFileToEntry -ModelName $modelNames[$mIdx] -FilePath $filePath
                                Write-Host "✓ Assigned $filePath" -ForegroundColor Green
                            }
                        }
                    }
                }
            }
            '2' {
                Write-Host ""; Write-Host "Hugging Face settings:" -ForegroundColor Cyan
                $enabled = Read-Host "Enable [true/false] (current: $($sources.HuggingFace.Enabled))"
                if ($enabled) { $sources.HuggingFace.Enabled = [bool]::Parse($enabled) }
                $token = Read-Host "Token (optional, current hidden)"
                if ($token) { $sources.HuggingFace.Token = $token }
                $defSearch = Read-Host "Default search (current: $($sources.HuggingFace.DefaultSearch))"
                if ($defSearch) { $sources.HuggingFace.DefaultSearch = $defSearch }
                $dlDir = Read-Host "Download directory (current: $($sources.HuggingFace.DownloadDir))"
                if ($dlDir) { $sources.HuggingFace.DownloadDir = $dlDir }

                if ($sources.HuggingFace.Enabled) {
                    $query = Read-Host "Search query [${($sources.HuggingFace.DefaultSearch)}]"
                    if (-not $query) { $query = $sources.HuggingFace.DefaultSearch }
                    Write-Host "Searching Hugging Face for '$query' .gguf..." -ForegroundColor Gray
                    $results = Search-HuggingFaceModels -Query $query -MaxResults $sources.HuggingFace.MaxResults -Token $sources.HuggingFace.Token
                    $k = 1
                    foreach ($r in $results) {
                        $ggufNames = ($r.GGUFs | Select-Object -ExpandProperty rfilename)
                        Write-Host "  [$k] $($r.Repo) - $($ggufNames -join ', ')" -ForegroundColor Gray
                        $k++
                    }
                    if ($results.Count -gt 0) {
                        $rSel = Read-Host "Select repo to download (number)"
                        $rIdx = [int]$rSel - 1
                        if ($rIdx -ge 0 -and $rIdx -lt $results.Count) {
                            $repo = $results[$rIdx].Repo
                            $files = $results[$rIdx].GGUFs
                            $fNames = @($files | Select-Object -ExpandProperty rfilename)
                            $l = 1; foreach ($fn in $fNames) { Write-Host "   [$l] $fn" -ForegroundColor Gray; $l++ }
                            $fSel = Read-Host "Select file"
                            $fIdx = [int]$fSel - 1
                            if ($fIdx -ge 0 -and $fIdx -lt $fNames.Count) {
                                $targetDir = $sources.HuggingFace.DownloadDir
                                if (-not $targetDir) { $targetDir = Read-Host "Enter download directory" }
                                $dlPath = Download-HuggingFaceFile -Repo $repo -RFileName $fNames[$fIdx] -TargetDir $targetDir -Token $sources.HuggingFace.Token
                                Write-Host "✓ Downloaded: $dlPath" -ForegroundColor Green
                                # Assign to a model entry
                                $modelNames = @($models.Keys)
                                $j = 1; foreach ($mn in $modelNames) { Write-Host "  [$j] $mn" -ForegroundColor Gray; $j++ }
                                $mSel = Read-Host "Assign to model (number) or Enter to skip"
                                if ($mSel) {
                                    $mIdx = [int]$mSel - 1
                                    if ($mIdx -ge 0 -and $mIdx -lt $modelNames.Count) {
                                        Assign-ModelFileToEntry -ModelName $modelNames[$mIdx] -FilePath $dlPath
                                        Write-Host "✓ Assigned to $($modelNames[$mIdx])" -ForegroundColor Green
                                    }
                                }
                            }
                        }
                    }
                }
            }
            '3' {
                Write-Host ""; Write-Host "Blob URL entries:" -ForegroundColor Cyan
                $entries = @($sources.Blobs.Entries)
                $idx = 1
                foreach ($e in $entries) {
                    Write-Host "  [$idx] $($e.Name) - $($e.Url)" -ForegroundColor Gray
                    $idx++
                }
                Write-Host "[A] Add new entry  [D] Download selected  [B] Back" -ForegroundColor Gray
                $bCmd = Read-Host "Choose"
                switch ($bCmd.ToUpper()) {
                    'A' {
                        $name = Read-Host "Entry name"
                        $url = Read-Host "Blob URL (.gguf)"
                        $dir = Read-Host "Target directory (blank to use default)"
                        $sources.Blobs.Entries += @{ Name = $name; Url = $url; TargetDir = $dir }
                    }
                    'D' {
                        $sel = Read-Host "Select entry number"
                        $eIdx = [int]$sel - 1
                        if ($eIdx -ge 0 -and $eIdx -lt $entries.Count) {
                            $entry = $entries[$eIdx]
                            $dir = $entry.TargetDir
                            if (-not $dir) { $dir = $sources.Blobs.DefaultDir }
                            if (-not $dir) { $dir = Read-Host "Enter download directory" }
                            if (-not (Test-Path $dir)) { New-Item -Path $dir -ItemType Directory -Force | Out-Null }
                            $outPath = Join-Path $dir (Split-Path $entry.Url -Leaf)
                            Invoke-WebRequest -Uri $entry.Url -OutFile $outPath -UseBasicParsing -TimeoutSec 180
                            Write-Host "✓ Downloaded: $outPath" -ForegroundColor Green
                            # Assign
                            $modelNames = @($models.Keys)
                            $j = 1; foreach ($mn in $modelNames) { Write-Host "  [$j] $mn" -ForegroundColor Gray; $j++ }
                            $mSel = Read-Host "Assign to model (number) or Enter to skip"
                            if ($mSel) {
                                $mIdx = [int]$mSel - 1
                                if ($mIdx -ge 0 -and $mIdx -lt $modelNames.Count) {
                                    Assign-ModelFileToEntry -ModelName $modelNames[$mIdx] -FilePath $outPath
                                    Write-Host "✓ Assigned to $($modelNames[$mIdx])" -ForegroundColor Green
                                }
                            }
                        }
                    }
                    default {}
                }
            }
            'S' { Save-ModelSourcesConfig -Sources $sources; break }
            default {}
        }
        Start-Sleep -Milliseconds 300
    }
}
