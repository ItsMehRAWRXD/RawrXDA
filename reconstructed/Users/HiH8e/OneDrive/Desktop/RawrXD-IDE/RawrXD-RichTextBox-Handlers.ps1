#Requires -Version 5.1
<#
.SYNOPSIS
    RichTextBox Handlers Module
    
.DESCRIPTION
    Provides event handlers and utilities for WinForms RichTextBox controls
    used in the RawrXD IDE editor.
#>

# ============================================
# MODULE VARIABLES
# ============================================

$script:RichTextBoxControls = @{}
$script:EventHandlers = @{}

# ============================================
# RICHTEXTBOX INITIALIZATION
# ============================================

function Register-RichTextBox {
    <#
    .SYNOPSIS
        Register a RichTextBox control for management
    #>
    param(
        [Parameter(Mandatory = $true)][string]$ControlName,
        [object]$Control
    )
    
    $script:RichTextBoxControls[$ControlName] = @{
        Name = $ControlName
        Control = $Control
        RegisteredAt = Get-Date
        EventHandlers = @()
    }
    
    return @{
        Success = $true
        Message = "RichTextBox registered: $ControlName"
    }
}

function Get-RegisteredRichTextBoxes {
    <#
    .SYNOPSIS
        Get list of registered RichTextBox controls
    #>
    return @{
        Count = $script:RichTextBoxControls.Count
        Controls = $script:RichTextBoxControls.Keys
    }
}

# ============================================
# INITIALIZATION
# ============================================

Write-Host "[RawrXD-RichTextBox-Handlers] Module loaded successfully" -ForegroundColor Green

# Note: This is a dot-sourced PS1 script, not a PSM1 module
# Functions are automatically available in parent scope
