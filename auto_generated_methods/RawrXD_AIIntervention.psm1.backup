# AI intervention logic: analyze, suggest, and document
function Invoke-RawrXD_AIIntervention {
    [CmdletBinding()]
    param(
        [string]$SourceFile,
        [string]$MethodStubPath
    )
    Write-Host "[AIIntervention] Analyzing $SourceFile and $MethodStubPath..."
    $lines = Get-Content $MethodStubPath -ErrorAction Stop
    $analysis = [System.Collections.Generic.List[string]]::new()

    # Detect common patterns and create suggestions
    if ($lines -match 'Write-Host') {
        $analysis.Add('Replace `Write-Host` with `Write-StructuredLog` for structured logging and observability.')
    }
    if ($lines -match 'param\(') {
        $analysis.Add('Add parameter validation and explicit types to exported functions.')
    }
    if ($lines -match 'try\s*\{') {
        $analysis.Add('Error handling present. Ensure exceptions are logged and rethrown where appropriate.')
    } else {
        $analysis.Add('Add try/catch blocks around external calls and long-running operations.')
    }
    if ($lines -match 'Invoke-RestMethod|HttpWebRequest|Get-Response') {
        $analysis.Add('Network calls detected: add timeouts, retry logic, and circuit-breaker behavior.')
    }

    # Build structured suggestion object
    $suggestions = [PSCustomObject]@{
        SourceFile = $SourceFile
        MethodStub  = $MethodStubPath
        Timestamp   = (Get-Date).ToString('o')
        Suggestions = $analysis
    }

    # Write suggestions JSON
    $suggestionsDir = 'D:/lazy init ide/auto_generated_methods/ai_suggestions'
    if (-not (Test-Path $suggestionsDir)) { New-Item -Path $suggestionsDir -ItemType Directory | Out-Null }
    $jsonPath = Join-Path $suggestionsDir ("${SourceFile}_suggestions.json")
    $suggestions | ConvertTo-Json -Depth 5 | Set-Content $jsonPath

    # Generate a simple patch file suggestion (do not modify source automatically)
    $patchDir = 'D:/lazy init ide/auto_generated_methods/ai_patches'
    if (-not (Test-Path $patchDir)) { New-Item -Path $patchDir -ItemType Directory | Out-Null }
    $patchFile = Join-Path $patchDir ("${SourceFile}_suggested.patch.ps1")
    $patchContent = @()
    foreach ($s in $analysis) { $patchContent += "# Suggestion: $s" }
    $patchContent += "# To apply: review the suggestions and edit $MethodStubPath manually or via tooling."
    $patchContent -join "`n" | Set-Content $patchFile

    # Generate AI doc summary
    $doc = "# AI-Generated Documentation for $SourceFile`n`n" + ($analysis -join "`n`n")
    $docPath = "D:/lazy init ide/auto_generated_methods/${SourceFile}_AIDoc.md"
    $doc | Set-Content $docPath

    Write-Host "[AIIntervention] Suggestions written to $jsonPath and patch stub $patchFile"
    return $suggestions
}

Export-ModuleMember -Function Invoke-RawrXD_AIIntervention
