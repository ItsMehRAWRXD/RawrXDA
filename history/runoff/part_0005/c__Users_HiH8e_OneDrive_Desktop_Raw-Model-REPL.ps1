<#
.SYNOPSIS
  Ground-up REPL that shows *everything* the model writes.
#>
param(
  [string]$Endpoint = "http://localhost:11430",   # llama.cpp default
  [string]$Model = "llama-3.1-70b-instruct",  # or any name your backend knows
  [int]   $Ctx = 32768
)

function Invoke-RawStream {
  param($Prompt)

  # --- 1. Build *empty* system block so the model starts from zero bias ---
  $payload = @{
    model       = $Model
    messages    = @(
      @{ role = "system"; content = "" },  # <--- intentionally blank
      @{ role = "user"; content = $Prompt }
    )
    temperature = 0.0        # deterministic for demo
    max_tokens  = 20000
    stream      = $true
    stop        = @()        # <--- disable every stop sequence
  } | ConvertTo-Json -Depth 10 -Compress

  # --- 2. Fire request and *never* interpret the bytes ---
  try {
    $client = [System.Net.Http.HttpClient]::new()
    $client.DefaultRequestHeaders.Accept.Clear()
    $client.DefaultRequestHeaders.Accept.Add(
      [System.Net.Http.Headers.MediaTypeWithQualityHeaderValue]::new("text/event-stream")
    )

    $streamTask = $client.PostAsync(
      "$Endpoint/v1/chat/completions",
      [System.Net.Http.StringContent]::new($payload, [System.Text.Encoding]::UTF8, "application/json")
    )
    $stream = $streamTask.GetAwaiter().GetResult().Content.ReadAsStreamAsync().GetAwaiter().GetResult()

    $reader = [System.IO.StreamReader]::new($stream)
    while (-not $reader.EndOfStream) {
      $line = $reader.ReadLine()
      # SSE format: data: {...} or blank line
      if ($line.StartsWith("data: ")) {
        $json = $line.Substring(6)
        if ($json -eq "[DONE]") { break }
        $delta = ($json | ConvertFrom-Json).choices[0].delta.content
        if ($null -ne $delta) { Write-Host -NoNewline $delta }
      }
    }
  }
  finally {
    $client?.Dispose()
  }
}

# --- 3. Simple REPL ---
while ($true) {
  Write-Host "`n>>> " -NoNewline -ForegroundColor Green
  $q = Read-Host
  if ($q -eq "exit") { break }
  Invoke-RawStream -Prompt $q
}