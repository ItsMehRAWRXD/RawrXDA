#Requires -Version 7.2
<#
.SYNOPSIS
  Provides the Invoke-OllamaGenerate function for interacting with a local Ollama server.
.DESCRIPTION
  This module contains a single, robust function, Invoke-OllamaGenerate,
  which sends prompts to the Ollama /api/generate endpoint. It supports
  both non-streaming (full response) and streaming (token-by-token) output.
.FUNCTIONS
  Invoke-OllamaGenerate
#>

function Invoke-OllamaGenerate {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Model,
        [Parameter(Mandatory = $true)]
        [string]$Prompt,
        [string]$OllamaUrl = "http://localhost:11434/api/generate",
        [switch]$Stream = $false,
        [hashtable]$Options = @{}
    )

    $Body = @{
        model  = $Model
        prompt = $Prompt
        stream = $Stream.IsPresent
    }

    # Add any additional options (temperature, top_p, etc.)
    if ($Options.Count -gt 0) {
        $Body.options = $Options
    }

    $BodyJson = $Body | ConvertTo-Json -Compress -Depth 10

    $Headers = @{ "Content-Type" = "application/json" }

    Write-Verbose "Calling Ollama model '$Model' at $OllamaUrl"

    try {
        if ($Stream) {
            # --- Handle Streaming Response ---
            $Response = Invoke-RestMethod -Uri $OllamaUrl -Method Post -Headers $Headers -Body $BodyJson -TimeoutSec 300 -Streaming -Raw
            $fullResponse = ""
            $Response.Split("`n") | ForEach-Object {
                if ($_ -ne "") {
                    try {
                        $jsonChunk = $_ | ConvertFrom-Json -ErrorAction Stop
                        if ($jsonChunk.response) {
                            $fullResponse += $jsonChunk.response
                            Write-Output $jsonChunk.response -NoNewline
                        }
                        if ($jsonChunk.done -eq $true) {
                            Write-Output ""
                            return $fullResponse
                        }
                    } catch { 
                        # Silently ignore malformed chunks
                    }
                }
            }
            return $fullResponse
        } else {
            # --- Handle Full Response (Non-Streaming) ---
            $Response = Invoke-RestMethod -Uri $OllamaUrl -Method Post -Headers $Headers -Body $BodyJson -TimeoutSec 300
            if ($Response.response) {
                return $Response.response
            } elseif ($Response.psobject.Properties['response']) {
                return $Response.psobject.Properties['response'].Value
            } else {
                return $Response | ConvertTo-Json -Compress
            }
        }
    }
    catch {
        Write-Error "❌ Ollama API Error: $($_.Exception.Message)"
        Write-Error "Ensure Ollama server is running at $OllamaUrl and model '$Model' is pulled (ollama pull $Model)."
        throw
    }
}

Export-ModuleMember -Function Invoke-OllamaGenerate

