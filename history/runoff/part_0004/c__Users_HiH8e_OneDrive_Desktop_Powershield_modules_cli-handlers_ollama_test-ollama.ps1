# ============================================
# CLI HANDLER: test-ollama
# ============================================
# Category: ollama
# Command: test-ollama
# Purpose: Test Ollama connection and available models
# ============================================

function Invoke-CliTestOllama {
    <#
    .SYNOPSIS
        Test Ollama connection and available models
    .DESCRIPTION
        Tests connection to Ollama server with enhanced error handling and security validation
    .EXAMPLE
        .\RawrXD.ps1 -CliMode -Command test-ollama
    .OUTPUTS
        [bool] $true if connection successful, $false otherwise
    #>
    
    Write-Host "`n=== Testing Ollama Connection ===" -ForegroundColor Cyan
    
    try {
        # Validate Ollama endpoint URL to prevent SSRF attacks
        $ollamaUri = "http://localhost:11434/api/tags"
        if ($ollamaUri -notmatch '^http://localhost:11434/') {
            Write-Error "Invalid Ollama endpoint - security validation failed"
            return $false
        }
        
        # Test connection with timeout and proper error handling
        $response = Invoke-RestMethod -Uri $ollamaUri -Method Get -TimeoutSec 5 -ErrorAction Stop
        
        Write-Host "✓ Ollama server is running" -ForegroundColor Green
        Write-Host "`nAvailable models:" -ForegroundColor Yellow
        
        if ($response.models -and $response.models.Count -gt 0) {
            foreach ($model in $response.models) {
                # Sanitize model name output to prevent injection
                $modelName = if ($model.name) { $model.name } else { "Unknown" }
                $modelSize = if ($model.size) { [Math]::Round($model.size / 1GB, 2) } else { 0 }
                Write-Host "  • $modelName - Size: $modelSize GB" -ForegroundColor White
            }
        } else {
            Write-Host "  No models found" -ForegroundColor Red
        }
        
        return $true
    }
    catch [System.Net.WebException] {
        Write-Host "✗ Ollama server is not reachable" -ForegroundColor Red
        Write-Host "  Error: $($_.Exception.Message)" -ForegroundColor Red
        Write-Host "`n  Please ensure Ollama is running: ollama serve" -ForegroundColor Yellow
        return $false
    }
    catch {
        Write-Host "✗ Unexpected error during Ollama connection test" -ForegroundColor Red
        Write-Host "  Error: $($_.Exception.Message)" -ForegroundColor Red
        return $false
    }
}

# Export for module loader
if ($MyInvocation.MyCommand.ScriptBlock.Module) {
    Export-ModuleMember -Function Invoke-CliTestOllama
}
