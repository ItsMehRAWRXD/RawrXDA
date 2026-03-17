<#
Fileless Model Manifest Pull System
Purpose: Retrieve model manifests (and optionally layers) entirely in memory with retry resilience.
This is a clean implementation replacing legacy mixed downloader code.
#>

Set-StrictMode -Version Latest
Add-Type -AssemblyName System.Net.Http

$script:ModelRegistry = @{}
$script:ManifestCache = @{}
$script:LayerCache = @{}

function New-ModelHttpClient {
    param(
        [int]$TimeoutSeconds = 120,
        [int]$MaxRetries = 4,
        [int]$RetryDelaySeconds = 2
    )
    $client = [System.Net.Http.HttpClient]::new()
    $client.Timeout = [TimeSpan]::FromSeconds($TimeoutSeconds)
    return [PSCustomObject]@{ Client = $client; MaxRetries = $MaxRetries; Delay = $RetryDelaySeconds }
}

function Get-ModelManifestFileless {
    param(
        [Parameter(Mandatory)][string]$ModelName,
        [string]$Tag = 'latest',
        [string[]]$Registries = @(
            'https://registry.ollama.ai',
            'https://registry.ollama.com'
        ),
        [switch]$ForceRefresh
    )
    $cacheKey = "${ModelName}:${Tag}"
    if (-not $ForceRefresh -and $script:ManifestCache.ContainsKey($cacheKey)) { return $script:ManifestCache[$cacheKey] }
    $http = New-ModelHttpClient
    foreach ($r in $Registries) {
        $url = "$r/v2/library/$ModelName/manifests/$Tag"
        for ($i = 0; $i -lt $http.MaxRetries; $i++) {
            try {
                $req = [System.Net.Http.HttpRequestMessage]::new([System.Net.Http.HttpMethod]::Get, $url)
                $req.Headers.Add('User-Agent', 'Fileless-Model-Puller/1.0')
                $resp = $http.Client.SendAsync($req).GetAwaiter().GetResult()
                if ($resp.IsSuccessStatusCode) {
                    $json = $resp.Content.ReadAsStringAsync().GetAwaiter().GetResult()
                    $manifest = $json | ConvertFrom-Json
                    $script:ManifestCache[$cacheKey] = $manifest
                    return $manifest
                }
            }
            catch {
                Start-Sleep -Seconds $http.Delay
            }
        }
    }
    throw "Unable to retrieve manifest for ${ModelName}:${Tag} from any registry"
}

function Get-ModelLayerFileless {
    param(
        [Parameter(Mandatory)][string]$Digest,
        [string]$Registry = 'https://registry.ollama.ai',
        [switch]$ForceRefresh
    )
    if (-not $ForceRefresh -and $script:LayerCache.ContainsKey($Digest)) { return $script:LayerCache[$Digest] }
    $url = "$Registry/v2/blobs/sha256/$Digest"
    $http = New-ModelHttpClient
    try {
        $req = [System.Net.Http.HttpRequestMessage]::new([System.Net.Http.HttpMethod]::Get, $url)
        $req.Headers.Add('User-Agent', 'Fileless-Model-Puller/1.0')
        $resp = $http.Client.SendAsync($req, [System.Net.Http.HttpCompletionOption]::ResponseHeadersRead).GetAwaiter().GetResult()
        if (-not $resp.IsSuccessStatusCode) { throw "HTTP $($resp.StatusCode)" }
        $stream = $resp.Content.ReadAsStreamAsync().GetAwaiter().GetResult()
        $ms = [System.IO.MemoryStream]::new()
        $buffer = New-Object byte[] 8192
        while (($read = $stream.Read($buffer, 0, $buffer.Length)) -gt 0) { $ms.Write($buffer, 0, $read) }
        $bytes = $ms.ToArray()
        $script:LayerCache[$Digest] = $bytes
        return $bytes
    }
    catch { throw "Layer download failed ($Digest): $_" }
}

function Get-ModelFileless {
    param(
        [Parameter(Mandatory)][string]$ModelName,
        [string]$Tag = 'latest',
        [switch]$IncludeLayers,
        [switch]$ForceRefresh
    )
    $manifest = Get-ModelManifestFileless -ModelName $ModelName -Tag $Tag -ForceRefresh:$ForceRefresh
    $entry = [PSCustomObject]@{
        Name = $ModelName; Tag = $Tag; Manifest = $manifest; PullDate = Get-Date;
        LayersDownloaded = @(); Status = 'Manifest'
    }
    $key = "${ModelName}:${Tag}"; $script:ModelRegistry[$key] = $entry
    if ($IncludeLayers) {
        foreach ($layer in $manifest.layers) {
            try {
                $digest = ($layer.digest -replace 'sha256:')
                [void](Get-ModelLayerFileless -Digest $digest -ForceRefresh:$ForceRefresh)
                $entry.LayersDownloaded += $layer.digest
            }
            catch { $entry.Status = 'Partial' }
        }
        if ($entry.LayersDownloaded.Count -eq $manifest.layers.Count) { $entry.Status = 'Complete' }
    }
    return $entry
}

function Get-CachedModels {
    if (-not $script:ModelRegistry.Count) { Write-Host 'No models cached.'; return }
    foreach ($k in $script:ModelRegistry.Keys) {
        $m = $script:ModelRegistry[$k]
        Write-Host ("{0} {1}:{2} Layers={3}/{4} Status={5}" -f (if ($m.Status -eq 'Complete') { '✓' } elseif ($m.Status -eq 'Partial') { '⚠' } else { '○' }), $m.Name, $m.Tag, $m.LayersDownloaded.Count, $m.Manifest.layers.Count, $m.Status)
    }
}

function Clear-ModelCache { $script:ModelRegistry.Clear(); $script:ManifestCache.Clear(); $script:LayerCache.Clear(); [GC]::Collect() }

function Export-ModelToFile {
    param([Parameter(Mandatory)][string]$ModelName, [string]$Tag = 'latest', [Parameter(Mandatory)][string]$OutputPath)
    $entry = $script:ModelRegistry["${ModelName}:${Tag}"]
    if (-not $entry) { throw "Model not cached" }
    if ($entry.Status -ne 'Complete') { throw "Model not fully downloaded (Status=$($entry.Status))" }
    $ms = [System.IO.MemoryStream]::new()
    foreach ($d in $entry.LayersDownloaded) { $bytes = $script:LayerCache[($d -replace 'sha256:')]; $ms.Write($bytes, 0, $bytes.Length) }
    [IO.File]::WriteAllBytes($OutputPath, $ms.ToArray()); Write-Host "Exported to $OutputPath"
}

function Compress-Data { param([byte[]]$Data, [string]$Path) Add-Type -AssemblyName System.IO.Compression; $fs = [IO.File]::Create($Path); try { $gz = [IO.Compression.GZipStream]::new($fs, [IO.Compression.CompressionMode]::Compress); try { $ms = [IO.MemoryStream]::new($Data); $ms.CopyTo($gz) } finally { $gz.Dispose(); $ms.Dispose() } } finally { $fs.Dispose() } }

Write-Host 'Fileless Model Manifest System ready.' -ForegroundColor Green
Write-Host 'Try: $m = Get-ModelFileless -ModelName qwen3 -Tag 8b -IncludeLayers' -ForegroundColor Cyan
Write-Host '     Get-CachedModels' -ForegroundColor Cyan
Write-Host '     Export-ModelToFile -ModelName qwen3 -Tag 8b -OutputPath model.bin' -ForegroundColor Cyan