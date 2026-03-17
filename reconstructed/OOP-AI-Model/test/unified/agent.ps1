# Test script for UnifiedAgentProcessor
$ErrorActionPreference = 'Stop'

try {
    # Load the main script
    . .\UnifiedAgentProcessor.ps1

    # Test class instantiation
    $processor = [UnifiedAgentProcessor]::new()
    Write-Host "Class instantiated successfully"

    # Test basic functionality
    $capabilities = $processor.GetCapabilities()
    Write-Host "Capabilities retrieved: $($capabilities.Keys -join ', ')"

    # Test request processing
    $request = [BaseRequest]::new()
    $request.Content = "Create a simple PowerShell function"
    $request.Type = [RequestType]::CodeGeneration

    $response = $processor.ProcessRequest($request)
    Write-Host "Request processed successfully"
    Write-Host "Response success: $($response.Success)"
    Write-Host "Response length: $($response.Content.Length)"

} catch {
    Write-Host "Error: $($_.Exception.Message)"
    Write-Host "Line: $($_.InvocationInfo.ScriptLineNumber)"
    Write-Host "Stack trace: $($_.ScriptStackTrace)"
}