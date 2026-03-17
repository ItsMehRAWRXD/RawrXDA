# Test-IDE-Integration.ps1
# Example usage of UnifiedAgentProcessor module in an IDE context

Import-Module 'D:\MyCopilot-IDE\UnifiedAgentProcessor\UnifiedAgentProcessor.psm1'

# Instantiate the agent
$agent = [UnifiedAgentProcessor]::new()

# Test code generation request
$result1 = $agent.ProcessRequest('generate code for Fibonacci')
Write-Host "CodeGen: $($result1.Content)"

# Test cloud resource request
$result2 = $agent.ProcessRequest('deploy aws lambda')
Write-Host "Cloud: $($result2.Content)"

# Test agentic automation request
$result3 = $agent.ProcessRequest('automate agentic task')
Write-Host "Agentic: $($result3.Content)"

# Test context management
$agent.SetContext('User', 'IDEUser')
$user = $agent.GetContext('User')
Write-Host "Context User: $user"

# Test model selection
$bestModel = $agent.SelectBestModel()
Write-Host "Best Model: $($bestModel.Name)"

# Test capabilities reporting
$caps = $agent.GetCapabilities()
Write-Host "Capabilities: $($caps | Out-String)"
