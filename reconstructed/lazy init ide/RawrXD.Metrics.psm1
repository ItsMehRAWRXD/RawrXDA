# RawrXD.Metrics.psm1
# Lightweight metrics collection + Prometheus exposition (PowerShell 5.1 compatible)

$script:MetricsState = @{
    Counters   = @{}
    Gauges     = @{}
    Histograms = @{}
    Started    = $false
    ServerJob  = $null
    ServerPort = 9091
}

function Initialize-RawrXDMetrics {
    $script:MetricsState.Started = $true
    if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) {
        Write-StructuredLog -Level 'INFO' -Message 'Metrics initialized' -Function 'Initialize-RawrXDMetrics'
    }
}

function Get-RawrXDLabelKey {
    param(
        [string]$Name,
        [hashtable]$Labels
    )
    if (-not $Labels) { return $Name }
    $pairs = $Labels.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Name)=$($_.Value)" }
    return "$Name|$($pairs -join ',')"
}

function Increment-RawrXDCounter {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [int]$Value = 1,
        [hashtable]$Labels = $null
    )
    $key = Get-RawrXDLabelKey -Name $Name -Labels $Labels
    if (-not $script:MetricsState.Counters.ContainsKey($key)) {
        $script:MetricsState.Counters[$key] = @{ Name = $Name; Labels = $Labels; Value = 0 }
    }
    $script:MetricsState.Counters[$key].Value += $Value
}

function Set-RawrXDGauge {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [double]$Value,
        [hashtable]$Labels = $null
    )
    $key = Get-RawrXDLabelKey -Name $Name -Labels $Labels
    $script:MetricsState.Gauges[$key] = @{ Name = $Name; Labels = $Labels; Value = $Value }
}

function Observe-RawrXDHistogram {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [double]$Value,
        [double[]]$Buckets = @(5, 10, 25, 50, 100, 250, 500, 1000),
        [hashtable]$Labels = $null
    )
    $key = Get-RawrXDLabelKey -Name $Name -Labels $Labels
    if (-not $script:MetricsState.Histograms.ContainsKey($key)) {
        $script:MetricsState.Histograms[$key] = @{ Name = $Name; Labels = $Labels; Buckets = @{}; Sum = 0; Count = 0 }
        foreach ($bucket in $Buckets) {
            $script:MetricsState.Histograms[$key].Buckets[$bucket] = 0
        }
    }
    $bucketKeys = @($script:MetricsState.Histograms[$key].Buckets.Keys)
    foreach ($bucket in $bucketKeys) {
        if ($Value -le [double]$bucket) {
            $script:MetricsState.Histograms[$key].Buckets[$bucket]++
        }
    }
    $script:MetricsState.Histograms[$key].Sum += $Value
    $script:MetricsState.Histograms[$key].Count++
}

function ConvertTo-PrometheusLabelString {
    param([hashtable]$Labels)
    if (-not $Labels -or $Labels.Count -eq 0) { return '' }
    $encoded = $Labels.GetEnumerator() | Sort-Object Name | ForEach-Object {
        $value = $_.Value -replace '"', '\\"'
        "$($_.Name)=`"$value`""
    }
    return "{$($encoded -join ',')}"
}

function Export-RawrXDPrometheusText {
    param(
        [string]$Path = $(Join-Path (Split-Path (Get-RawrXDLogPath) -Parent) 'metrics.prom')
    )
    $lines = New-Object System.Collections.Generic.List[string]

    foreach ($item in $script:MetricsState.Counters.Values) {
        $labels = ConvertTo-PrometheusLabelString $item.Labels
        $lines.Add("$($item.Name)$labels $($item.Value)")
    }

    foreach ($item in $script:MetricsState.Gauges.Values) {
        $labels = ConvertTo-PrometheusLabelString $item.Labels
        $lines.Add("$($item.Name)$labels $($item.Value)")
    }

    foreach ($item in $script:MetricsState.Histograms.Values) {
        $bucketKeys = @($item.Buckets.Keys) | Sort-Object {[double]$_}
        foreach ($bucket in $bucketKeys) {
            $bucketLabels = @{}
            if ($item.Labels) { $item.Labels.GetEnumerator() | ForEach-Object { $bucketLabels[$_.Name] = $_.Value } }
            $bucketLabels['le'] = $bucket
            $labels = ConvertTo-PrometheusLabelString $bucketLabels
            $lines.Add("$($item.Name)_bucket$labels $($item.Buckets[$bucket])")
        }
        $labels = ConvertTo-PrometheusLabelString $item.Labels
        $lines.Add("$($item.Name)_sum$labels $($item.Sum)")
        $lines.Add("$($item.Name)_count$labels $($item.Count)")
    }

    $lines | Set-Content -Path $Path -Encoding UTF8
    return $Path
}

function Start-RawrXDMetricServer {
    param([int]$Port = 9091)
    if ($script:MetricsState.ServerJob) { return $script:MetricsState.ServerJob }
    $script:MetricsState.ServerPort = $Port
    $metricsPath = (Join-Path (Split-Path (Get-RawrXDLogPath) -Parent) 'metrics.prom')

    # Ensure the file exists so the server doesn't 404 immediately
    Export-RawrXDPrometheusText -Path $metricsPath

    $script:MetricsState.ServerJob = Start-Job -ScriptBlock {
        param($Port, $FilePath)
        $listener = New-Object System.Net.HttpListener
        $listener.Prefixes.Add("http://*:$Port/")
        $listener.Start()
        while ($listener.IsListening) {
            try {
                $context = $listener.GetContext()
                $response = $context.Response
                $path = $context.Request.RawUrl
                if ($path -eq '/metrics') {
                    if (Test-Path $FilePath) {
                        $payload = [System.Text.Encoding]::UTF8.GetBytes((Get-Content $FilePath -Raw))
                        $response.ContentType = 'text/plain; version=0.0.4'
                    } else {
                        $payload = [System.Text.Encoding]::UTF8.GetBytes('# No metrics yet')
                    }
                    $response.ContentLength64 = $payload.Length
                    $response.OutputStream.Write($payload, 0, $payload.Length)
                } else {
                    $payload = [System.Text.Encoding]::UTF8.GetBytes('RawrXD Metrics - use /metrics')
                    $response.ContentLength64 = $payload.Length
                    $response.OutputStream.Write($payload, 0, $payload.Length)
                }
                $response.OutputStream.Close()
            } catch {
                Start-Sleep -Milliseconds 100
            }
        }
        $listener.Stop()
    } -ArgumentList $Port, $metricsPath

    return $script:MetricsState.ServerJob
}

function Stop-RawrXDMetricServer {
    if ($script:MetricsState.ServerJob) {
        Stop-Job -Job $script:MetricsState.ServerJob -ErrorAction SilentlyContinue
        Remove-Job -Job $script:MetricsState.ServerJob -Force -ErrorAction SilentlyContinue
        $script:MetricsState.ServerJob = $null
    }
}

Export-ModuleMember -Function Initialize-RawrXDMetrics, Increment-RawrXDCounter, Set-RawrXDGauge, Observe-RawrXDHistogram, Export-RawrXDPrometheusText, Start-RawrXDMetricServer, Stop-RawrXDMetricServer
