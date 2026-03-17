$base = 'http://127.0.0.1:8765'

function PostJson($route, $obj) {
    Invoke-RestMethod -Method Post -Uri "$base/$route" -Body ($obj | ConvertTo-Json -Depth 6) -ContentType 'application/json'
}

# List desktop
PostJson 'fs/list' @{ path = "C:\\Users\\HiH8e\\OneDrive\\Desktop" } | Format-List

# Read a file
PostJson 'fs/read' @{ path = "C:\\Users\\HiH8e\\OneDrive\\Desktop\\Powershield\\AGENTIC-GIT-TOOLS.md" } | Format-List

# Write and append file
$target = "D:\\temp\\agenttools-test.txt"
PostJson 'fs/write' @{ path = $target; content = "Hello from AgentTools $(Get-Date)" } | Format-List
PostJson 'fs/append' @{ path = $target; content = "`nAnother line $(Get-Date)" } | Format-List

# Search
PostJson 'grep/search' @{ path = "C:\\Users\\HiH8e\\OneDrive\\Desktop\\Powershield"; pattern = 'AGENTIC'; isRegex = $false; maxResults = 10 } | Format-Table -AutoSize

# Run command
PostJson 'cmd/run' @{ command = 'pwsh'; arguments = @('-NoProfile','-Command','"Write-Output Test OK; $PSVersionTable.PSVersion"') } | Format-List
