param(
    [string]$Prompt,
    [string]$Endpoint = "http://localhost:11434/v1/chat/completions",
    [string]$Model    = "bigdaddyg-fast:latest"
)

$body = @{
    model    = $Model
    messages = @(
        @{
            role    = "system"
            content = @"
You are a transparent-reasoning model.
When responding, explicitly write out your reasoning steps,
planning, scratch work, tool intentions, and chain-of-thought
**as plain text**, inside clear sections such as:

<thinking>
...
</thinking>

Always include the full reasoning you are allowed to output.
"@
        },
        @{
            role    = "user"
            content = $Prompt
        }
    )
    temperature = 0.4
    stream      = $true
} | ConvertTo-Json -Depth 10

Invoke-RestMethod -Uri $Endpoint -Method POST -ContentType "application/json" -Body $body |
    ForEach-Object {
        if ($_.choices[0].delta.content) {
            Write-Host -NoNewline $_.choices[0].delta.content
        }
    }
