#Requires -Version 7.2
<#
.SYNOPSIS
  Provides the Invoke-OllamaGenerate function for interacting with a local Ollama server.
.DESCRIPTION
  This module contains a single, robust function, Invoke-OllamaGenerate,
  which sends prompts to the Ollama /api/generate endpoint. It supports
  both non-streaming (full response) and streaming (token-by-token) output.
  
  Features:
  - Full and streaming response modes
  - Advanced options support (temperature, top_p, top_k, etc.)
  - Robust error handling
  - Silent malformed chunk handling for streaming
  - Timeout protection (5 minutes default)
  
.PARAMETER Model
  The name of the Ollama model to use (e.g., "llama2", "mistral", "qwen2:7b")
  
.PARAMETER Prompt
  The text prompt to send to the model
  
.PARAMETER OllamaUrl
  The base URL for the Ollama API (default: http://localhost:11434/api/generate)
  
.PARAMETER Stream
  If specified, streams the response token-by-token instead of waiting for the complete response
  
.PARAMETER Options
  Optional hashtable of advanced Ollama options:
  - temperature: Controls randomness (0.0-2.0)
  - top_p: Nucleus sampling threshold (0.0-1.0)
  - top_k: Top-k sampling (number of tokens to consider)
  - num_predict: Maximum number of tokens to generate
  - repeat_penalty: Penalty for repetition (1.0+)
  
.EXAMPLE
  Invoke-OllamaGenerate -Model "llama2" -Prompt "What is PowerShell?"
  
.EXAMPLE
  Invoke-OllamaGenerate -Model "mistral" -Prompt "Explain quantum computing" -Stream
  
.EXAMPLE
  Invoke-OllamaGenerate -Model "qwen2:7b" -Prompt "Write a haiku" -Options @{
      temperature = 0.8
      top_p = 0.9
      num_predict = 100
  }
  
.FUNCTIONS
  Invoke-OllamaGenerate
#>

function Invoke-OllamaGenerate {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true, Position = 0)]
        [string]$Model,
        
        [Parameter(Mandatory = $true, Position = 1)]
        [string]$Prompt,
        
        [Parameter(Mandatory = $false)]
        [string]$OllamaUrl = "http://localhost:11434/api/generate",
        
        [Parameter(Mandatory = $false)]
        [switch]$Stream = $false,
        
        [Parameter(Mandatory = $false)]
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
    if ($Options.Count -gt 0) {
        Write-Verbose "Options: $($Options | ConvertTo-Json -Compress)"
    }

    try {
        if ($Stream) {
            # --- Handle Streaming Response ---
            Write-Verbose "Using streaming mode"
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
                            Write-Verbose "Streaming complete. Total length: $($fullResponse.Length) characters"
                            return $fullResponse
                        }
                    } catch { 
                        # Silently ignore malformed chunks during streaming
                        Write-Verbose "Skipping malformed chunk: $_"
                    }
                }
            }
            return $fullResponse
        } else {
            # --- Handle Full Response (Non-Streaming) ---
            Write-Verbose "Using non-streaming mode"
            $Response = Invoke-RestMethod -Uri $OllamaUrl -Method Post -Headers $Headers -Body $BodyJson -TimeoutSec 300
            
            if ($Response.response) {
                Write-Verbose "Response received. Length: $($Response.response.Length) characters"
                return $Response.response
            } elseif ($Response.psobject.Properties['response']) {
                Write-Verbose "Response received from property accessor"
                return $Response.psobject.Properties['response'].Value
            } else {
                Write-Warning "Unexpected response format. Returning as JSON."
                return $Response | ConvertTo-Json -Compress
            }
        }
    }
    catch {
        $errorMsg = $_.Exception.Message
        Write-Error "❌ Ollama API Error: $errorMsg"
        Write-Error "Ensure Ollama server is running at $OllamaUrl and model '$Model' is pulled (ollama pull $Model)."
        throw
    }
}

Export-ModuleMember -Function Invoke-OllamaGenerate

