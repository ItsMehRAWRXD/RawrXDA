# ============================================
# CLI HANDLER: list-models
# ============================================
# Category: ollama
# Command: list-models
# Purpose: List all available Ollama models with details
# ============================================

function Invoke-CliListModels {
    <#
    .SYNOPSIS
        List all available Ollama models with details
    .DESCRIPTION
        Lists all available Ollama models with enhanced security and error handling
    .EXAMPLE
        .\RawrXD.ps1 -CliMode -Command list-models
    .OUTPUTS
        [bool] $true if successful, $false otherwise
    #>
    
    Write-Host "`n=== Available Ollama Models ===" -ForegroundColor Cyan
    
    try {
        $ollamaUri = "http://localhost:11434/api/tags"
        
        $response = Invoke-RestMethod -Uri $ollamaUri -Method Get -TimeoutSec 5 -ErrorAction Stop
        
        if ($response.models -and $response.models.Count -gt 0) {
            $models = $response.models | Sort-Object name
            
            Write-Host "`nFound $($models.Count) model(s):" -ForegroundColor Green
            Write-Host ""
            
            foreach ($model in $models) {
                # Sanitize and validate model data before display
                $modelName = if ($model.name) { $model.name } else { "Unknown" }
                $sizeGB = if ($model.size -and $model.size -gt 0) { 
                    [Math]::Round($model.size / 1GB, 2) 
                } else { 
                    0 
                }
                
                $modified = if ($model.modified_at) { 
                    try {
                        (Get-Date $model.modified_at -Format "yyyy-MM-dd HH:mm:ss") 
                    }
                    catch {
                        "Invalid Date"
                    }
                } else { 
                    "Unknown" 
                }
                
                Write-Host "Model: " -NoNewline -ForegroundColor Yellow
                Write-Host $modelName -ForegroundColor White
                Write-Host "  Size: $sizeGB GB" -ForegroundColor Gray
                Write-Host "  Modified: $modified" -ForegroundColor Gray
                Write-Host ""
            }
        } else {
            Write-Host "No models installed" -ForegroundColor Red
            Write-Host "Install a model: ollama pull llama2" -ForegroundColor Yellow
        }
        
        return $true
    }
    catch [System.Net.WebException] {
        Write-Host "✗ Failed to list models - connection error" -ForegroundColor Red
        Write-Host "  Error: $($_.Exception.Message)" -ForegroundColor Red
        return $false
    }
    catch {
        Write-Host "✗ Failed to list models" -ForegroundColor Red
        Write-Host "  Error: $($_.Exception.Message)" -ForegroundColor Red
        return $false
    }
}

# Export for module loader
if ($MyInvocation.MyCommand.ScriptBlock.Module) {
    Export-ModuleMember -Function Invoke-CliListModels
}
