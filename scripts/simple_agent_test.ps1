# simple_agent_test.ps1
# Simple test script that emits AGENT_DONE completion contract

Write-Host "Starting simple agent test..."
Start-Sleep -Seconds 1
Write-Host "Agent work completed successfully"

# Emit completion contract (direct implementation to avoid sourcing issues)
function Emit-SimpleDone {
    param($Status, $Phase, $TasksCompleted, $TasksTotal, $Commit, $Artifacts, $NextPhase)
    
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
    
    $textBlock = "\n[AGENT_DONE]\n"
    $textBlock += "status=$Status\n"
    $textBlock += "phase=$Phase\n"
    $textBlock += "tasks_completed=$TasksCompleted\n"
    $textBlock += "tasks_total=$TasksTotal\n"
    $textBlock += "commit=$Commit\n"
    $textBlock += "artifacts=$($Artifacts.Count)\n"
    $textBlock += "next=$NextPhase\n"
    $textBlock += "timestamp=$timestamp\n"
    
    return $textBlock
}

$completion = Emit-SimpleDone -Status "success" -Phase "test" -TasksCompleted 3 -TasksTotal 3 `
    -Commit "test123" -Artifacts @("test1.txt", "test2.txt") -NextPhase "next phase"

Write-Host $completion