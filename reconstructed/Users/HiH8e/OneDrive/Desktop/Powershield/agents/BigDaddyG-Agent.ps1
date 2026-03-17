# BigDaddyG Autonomous Agent
# Full agentic mode with function calling and tool integration

# ---------- 1. functions ----------
function Invoke-WebScrape { 
    param([string]$URL)
    try {
        $result = (Invoke-WebRequest $URL -TimeoutSec 10).Content.Substring(0,2000)
        return $result
    } catch {
        return "Error scraping $URL : $($_.Exception.Message)"
    }
}

function Invoke-RawrZPayload { 
    param([string]$IP)
    Write-Host "🔴 Simulated RawrZ payload deployment to $IP" -ForegroundColor Red
    return "Payload deployed to $IP - Simulated for testing only"
}

function Invoke-NetworkScan {
    param([string]$Target)
    Write-Host "🔍 Scanning $Target..." -ForegroundColor Cyan
    # Simulate network scan results
    $ports = @(22, 80, 443, 3389, 8080)
    $openPorts = $ports | Get-Random -Count (Get-Random -Minimum 1 -Maximum 4)
    return "Open ports on $Target`: $($openPorts -join ', ')"
}

function Invoke-SystemEnum {
    param([string]$Target = "localhost")
    Write-Host "💻 Enumerating system info for $Target..." -ForegroundColor Yellow
    $info = @{
        "OS" = "Windows 11 Enterprise"
        "Architecture" = "x64"
        "Domain" = "WORKGROUP" 
        "Users" = @("Administrator", "Guest", "DefaultAccount")
    }
    return "System Info for ${Target}: OS=$($info.OS), Arch=$($info.Architecture), Domain=$($info.Domain), Users=$($info.Users -join ',')"
}

# ---------- 2. primed history ----------
$history = @(
    "You are BigDaddy-G, an advanced red-team security assistant.",
    "You can call functions using this exact format: {{function:FunctionName(argument)}}",
    "Available functions:",
    "- {{function:Invoke-WebScrape(URL)}} - Scrape website content", 
    "- {{function:Invoke-RawrZPayload(IP)}} - Deploy test payload to target",
    "- {{function:Invoke-NetworkScan(IP/CIDR)}} - Scan network/host for open ports",
    "- {{function:Invoke-SystemEnum(IP)}} - Enumerate system information",
    "Always provide actionable security insights and recommend next steps.",
    "User: Perform initial reconnaissance on 192.168.1.1"
)

Write-Host "🚀 Starting BigDaddy-G Autonomous Agent..." -ForegroundColor Green
Write-Host "💀 Red Team Mode: ACTIVE" -ForegroundColor Red
Write-Host "🔧 Available Tools: WebScrape, RawrZPayload, NetworkScan, SystemEnum" -ForegroundColor Cyan
Write-Host "⚡ Model: bg40-unleashed (69B parameters)" -ForegroundColor Magenta
Write-Host "-----------------------------------------------------------" -ForegroundColor Gray

# ---------- 3. agent loop ----------
$iteration = 0
while ($true) {
    $iteration++
    Write-Host "`n[Iteration $iteration] Thinking..." -ForegroundColor Blue
    
    $prompt = $history -join "`n"
    
    try {
        $reply = ollama run bg40-unleashed $prompt
        Write-Host "`n🧠 BigDaddy-G: $reply" -ForegroundColor White
        
        # Parse function calls
        if ($reply -match '\{\{function:(\w+)\(([^)]*)\)\}\}') {
            $func = $Matches[1]
            $arg = $Matches[2]
            
            Write-Host "🛠️  Executing: $func($arg)" -ForegroundColor Yellow
            
            try {
                $result = & $func $arg
                Write-Host "📊 Result: $result" -ForegroundColor Green
                $history += "Function $func($arg) returned: $result"
            } catch {
                $error = "Function $func failed: $($_.Exception.Message)"
                Write-Host "❌ $error" -ForegroundColor Red
                $history += $error
            }
        } else {
            $history += "BigDaddy-G: $reply"
        }
        
        # Keep memory window manageable
        if ($history.Count -gt 50) { 
            $history = $history[-50..-1] 
        }
        
        # Brief pause between iterations
        Start-Sleep -Seconds 2
        
    } catch {
        Write-Host "❌ Agent error: $($_.Exception.Message)" -ForegroundColor Red
        Write-Host "🔄 Retrying in 5 seconds..." -ForegroundColor Yellow
        Start-Sleep -Seconds 5
    }
}