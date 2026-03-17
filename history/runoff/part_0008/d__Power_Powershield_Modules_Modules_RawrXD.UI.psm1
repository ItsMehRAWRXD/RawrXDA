# Module variables
$script:ModuleName = 'RawrXD.UI'
$script:ModuleVersion = '1.0.0'

# Import necessary assemblies/modules
Add-Type -AssemblyName System.Windows.Forms
try {
    Import-Module System.Management.Automation.AiAgentToolTuner -ErrorAction Stop
}
catch {
    Write-Verbose "System.Management.Automation.AiAgentToolTuner not available; continuing without tool tuner."
}

# Create a new AI agent tool tuner instance (if available)
$ai = $null
$aiType = [type]::GetType('System.Management.Automation.AiAgentToolTuner', $false)
if ($aiType) {
    $ai = New-Object System.Management.Automation.AiAgentToolTuner
} else {
    Write-Verbose "AiAgentToolTuner type not available; continuing without tool tuner."
}

# Define the secure data storage function
function Secure-DataStorage {
    param (
        [string]$Name,
        [array]$Steps,
        [bool]$IsCritical
    )
    
    # Create a new RSADepartmentalStore instance for secure data storage
    $secureData = New-Object System.Security.Cryptography.RSADepartmentalStore
    
    # Encrypt the tool's steps using the secure departmental store
    $encryptedSteps = $secureData.Encrypt([Convert]::FromBytes($Steps))
    
    return [PSCustomObject]@{
        Name      = $Name
        Steps     = $encryptedSteps
        IsCritical = $IsCritical
    }
}

# Define the UI-related functions

# ============================================
# Function: Update-UndoRedoMenuState
# ============================================
function Update-UndoRedoMenuState {
    $undoItem.Enabled = ($script:undoStack.Count -gt 0)
    $redoItem.Enabled = ($script:redoStack.Count -gt 0)
}

# ============================================
# Function: Show-ExtensionMarketplace
# ============================================
function Show-ExtensionMarketplace {
    <#
    .SYNOPSIS
        Opens the extension marketplace (wrapper for Show-Marketplace)
    .DESCRIPTION
        Provides a standardized function name for opening the extension marketplace
    #>
    [System.Windows.Forms.Application]::RunCommand([System.Reflection.Assembly]::LoadWithPartialName('Microsoft.Win32.Registry'))
}

# ============================================
# Function: Update-FontMenuChecks
# ============================================
function Update-FontMenuChecks {
    param(
        [System.Windows.Forms.ToolStripMenuItem]$SelectedItem,
        [System.Windows.Forms.ToolStripMenuItem[]]$AllItems
    )
    
    foreach ($item in $AllItems) {
        $item.Checked = ($item -eq $SelectedItem)
    }
}

# ============================================
# Function: Update-ScaleMenuChecks
# ============================================
function Update-ScaleMenuChecks {
    param(
        [System.Windows.Forms.ToolStripMenuItem]$SelectedItem,
        [System.Windows.Forms.ToolStripMenuItem[]]$AllItems
    )
    
    foreach ($item in $AllItems) {
        $item.Checked = ($item -eq $SelectedItem)
    }
}

# Export functions
$ExportedFunctions = @(
    'Update-UndoRedoMenuState',
    'Show-ExtensionMarketplace',
    'Update-FontMenuChecks',
    'Update-ScaleMenuChecks'
)

Export-ModuleMember -Function $ExportedFunctions

# Define the AI agent tool tuner functions

# ============================================
# Function: Tune-AiAgent
# ============================================
function Tune-AiAgent {
    param(
        [string]$FunctionName,
        [array]$Parameters
    )
    
    # Create a new AiAgent instance
    $ai = New-Object System.Management.Automation.AiAgent
    
    # Define the AI agent function
    $aiFunction = { 
        param (
            [string]$function_name,
            [array]$parameters
        )
        
        # Call the AI agent function
        $result = $ai.Execute($function_name, $parameters)
        
        return $result
    }
    
    # Set the AI agent instance
    $ai.SetFunction($aiFunction)
}

# Export the Tune-AiAgent function
Export-ModuleMember -Function 'Tune-AiAgent'
