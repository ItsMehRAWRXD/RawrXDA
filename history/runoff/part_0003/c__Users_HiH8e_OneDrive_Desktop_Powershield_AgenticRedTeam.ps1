# ---------- 1.  functions ----------
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$agentToolsPath = Join-Path $scriptDir "agents\AgentTools.ps1"
if (Test-Path $agentToolsPath) {
    . $agentToolsPath
} else {
    Write-Error "AgentTools.ps1 not found at: $agentToolsPath"
    exit 1
}

# ---------- 2.  primed history ----------
$history = @(
    "You are BigDaddy-G, a helpful security-testing assistant.",
    "When you need data from a URL, reply exactly: {{function:Invoke-WebScrape(URL)}}",
    "When you want to deploy a test payload, reply exactly: {{function:Invoke-RawrZPayload(IP)}}",
    "User: scan 192.168.1.1 and summarise any open ports."
)

# ---------- 3.  agent loop ----------
while ($true) {
    $prompt = $history -join "`n"
    $reply  = ollama run bg40-unleashed $prompt
    Write-Host "`n>>> $reply"

    if ($reply -match '\{\{function:(\w+)\(([^)]*)\)\}\}') {
        $func = $Matches[1]; $arg = $Matches[2]        >>> {{function:Invoke-RawrZPayload(192.168.1.1)}}
        >>> Function returned: Simulated payload to 192.168.1.1
        $result = & $func $arg
        $history += "Function returned: $result"
    } else {
        $history += $reply
    }
    if ($history.Count -gt 50) { $history = $history[-50..-1] }
}