# RawrXD.Tracing.psm1
# Lightweight trace/span recorder for PowerShell workflows

$script:TraceState = @{
    Spans = @{}
    TraceLogPath = $null
}

function Get-RawrXDTraceLogPath {
    if ($script:TraceState.TraceLogPath) { return $script:TraceState.TraceLogPath }
    $logDir = Split-Path (Get-RawrXDLogPath) -Parent
    $path = Join-Path $logDir 'traces.jsonl'
    $script:TraceState.TraceLogPath = $path
    return $path
}

function New-RawrXDTraceId {
    return [Guid]::NewGuid().ToString('N')
}

function Start-RawrXDSpan {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [string]$TraceId = $(New-RawrXDTraceId),
        [string]$ParentSpanId = $null,
        [hashtable]$Attributes = $null
    )
    $spanId = [Guid]::NewGuid().ToString('N')
    $script:TraceState.Spans[$spanId] = @{
        SpanId = $spanId
        TraceId = $TraceId
        ParentSpanId = $ParentSpanId
        Name = $Name
        StartTime = (Get-Date).ToUniversalTime().ToString('o')
        Attributes = $Attributes
    }
    return $script:TraceState.Spans[$spanId]
}

function Stop-RawrXDSpan {
    param(
        [Parameter(Mandatory = $true)]$Span,
        [string]$Status = 'OK',
        [string]$Error = $null
    )
    $spanId = if ($Span -is [hashtable]) { $Span.SpanId } else { $Span }
    if (-not $script:TraceState.Spans.ContainsKey($spanId)) { return }
    $spanObj = $script:TraceState.Spans[$spanId]
    $spanObj.EndTime = (Get-Date).ToUniversalTime().ToString('o')
    $spanObj.Status = $Status
    $spanObj.Error = $Error
    $path = Get-RawrXDTraceLogPath
    ($spanObj | ConvertTo-Json -Compress) | Add-Content -Path $path
    $script:TraceState.Spans.Remove($spanId) | Out-Null
}

Export-ModuleMember -Function New-RawrXDTraceId, Start-RawrXDSpan, Stop-RawrXDSpan
