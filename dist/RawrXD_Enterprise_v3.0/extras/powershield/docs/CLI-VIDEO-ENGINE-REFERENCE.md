# Agentic Video Engine - CLI & Curl Reference
# RawrXD Video Engine External Testing Guide

## Quick CLI Commands

```powershell
# Get help
.\RawrXD.ps1 -CliMode -Command video-help

# Search YouTube
.\RawrXD.ps1 -CliMode -Command video-search -Prompt "python tutorial"

# Play video (opens browser)
.\RawrXD.ps1 -CliMode -Command video-play -URL "https://youtube.com/watch?v=..."

# Download video
.\RawrXD.ps1 -CliMode -Command video-download -URL "https://..." -OutputPath "C:\Videos"

# Navigate browser
.\RawrXD.ps1 -CliMode -Command browser-navigate -URL "https://github.com"
```

## JSON Output Examples

All commands output JSON for scripting:

```powershell
# Get JSON only (pipe to ConvertFrom-Json)
$result = .\RawrXD.ps1 -CliMode -Command video-play -URL "https://..." 2>&1 | 
    Select-String -Pattern '\{.*\}' | 
    ForEach-Object { $_.Line } | 
    ConvertFrom-Json

# Check status
if ($result.status -eq "success") {
    Write-Host "Video playing: $($result.url)"
}
```

## Curl-Like Usage (PowerShell)

```powershell
# Equivalent to curl POST
Invoke-RestMethod -Uri "http://localhost:11434/api/generate" -Method POST -Body @{
    model = "llama2"
    prompt = "search youtube for powershell"
} | ConvertTo-Json

# Run RawrXD command and get JSON
$json = powershell.exe -ExecutionPolicy Bypass -File ".\RawrXD.ps1" -CliMode -Command video-help 2>&1 | 
    Out-String
```

## Batch Testing

```powershell
# Run all tests
.\Test-Video-Engine-CLI.ps1 -RunAll

# Run specific tests
.\Test-Video-Engine-CLI.ps1 -TestHelp
.\Test-Video-Engine-CLI.ps1 -TestPlay
.\Test-Video-Engine-CLI.ps1 -TestSearch

# JSON output only
.\Test-Video-Engine-CLI.ps1 -RunAll -JsonOnly
```

## Available CLI Commands

| Command | Parameters | Description |
|---------|------------|-------------|
| video-search | -Prompt "query" | Search YouTube |
| video-download | -URL "url" [-OutputPath "path"] | Download video |
| video-play | -URL "url" | Play in browser |
| video-help | (none) | Show help |
| browser-navigate | -URL "url" | Open URL |
| browser-screenshot | [-OutputPath "path"] | Capture browser |
| browser-click | -Selector "#id" | Click element |

## Response Format

```json
{
    "status": "success",
    "action": "play",
    "url": "https://...",
    "method": "default_browser"
}
```

## Error Handling

```powershell
$result = .\RawrXD.ps1 -CliMode -Command video-play -URL "invalid" 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "Command failed"
}
```
