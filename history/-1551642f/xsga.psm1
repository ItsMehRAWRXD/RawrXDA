# RawrXD.ErrorHandling.psm1
# Centralized error capture wrapper for PowerShell operations

function Invoke-RawrXDSafeOperation {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][ScriptBlock]$Script,
        [hashtable]$Context = $null
    )

    $traceId = New-RawrXDTraceId
    $spanId = Start-RawrXDSpan -Name $Name -TraceId $traceId -Attributes $Context

    try {
        $result = & $Script
        Stop-RawrXDSpan -SpanId $spanId -Status 'OK'
        return [ordered]@{ Success = $true; Result = $result; TraceId = $traceId }
    } catch {
        Stop-RawrXDSpan -SpanId $spanId -Status 'ERROR' -Error $_.Exception.Message
        if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) {
            Write-StructuredLog -Level 'ERROR' -Message $_.Exception.Message -Function $Name -Data $Context
        }
        return [ordered]@{ Success = $false; Error = $_.Exception.Message; TraceId = $traceId }
    }
}

Export-ModuleMember -Function Invoke-RawrXDSafeOperation
