# Agentic Mode Toggle PowerShell Script

This PowerShell script allows you to toggle Agentic Mode from the command line.

## Usage

```powershell
# Enable agentic mode
.\Agentic-Mode-Toggle.ps1 on

# Disable agentic mode
.\Agentic-Mode-Toggle.ps1 off

# Check status
.\Agentic-Mode-Toggle.ps1 status
```

## Configuration

Configuration is saved to: `$env:APPDATA\Ollama\agentic-config.json`

## Parameters

- `on` - Enable agentic mode
- `off` - Disable agentic mode  
- `status` - Show current status

## Example

```powershell
PS C:\local-agentic-copilot> .\Agentic-Mode-Toggle.ps1 on
🚀 Agentic Mode Enabled

PS C:\local-agentic-copilot> .\Agentic-Mode-Toggle.ps1 status
Current Mode: AGENTIC

PS C:\local-agentic-copilot> .\Agentic-Mode-Toggle.ps1 off
⏸️ Standard Mode Enabled
```