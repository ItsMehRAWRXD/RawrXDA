# RawrXD Production Deployment Troubleshooting Guide

## Quick Diagnosis

### Check System Status

```powershell
# Check PowerShell version
$PSVersionTable.PSVersion

# Check execution policy
Get-ExecutionPolicy

# Check admin privileges
([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]"Administrator")

# Check .NET Framework
Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\NET Framework Setup\NDP\v4\Full" -Name Release

# Check disk space
Get-PSDrive -Name "C" | Select-Object Used, Free

# Check memory
Get-CimInstance -ClassName Win32_OperatingSystem | Select-Object TotalVisibleMemorySize, FreePhysicalMemory
```

## Common Issues and Solutions

### Issue 1: Permission Denied Errors

**Symptoms:**
- "Access to the path 'C:\RawrXD\Logs' is denied"
- "UnauthorizedAccessException"
- File creation failures

**Solutions:**

1. **Run as Administrator:**
   ```powershell
   # Launch PowerShell as Administrator
   Start-Process powershell -Verb RunAs -ArgumentList "-File C:\RawrXD\Production\Execute-UltimateDeployment.ps1"
   ```

2. **Fix Directory Permissions:**
   ```powershell
   # Grant full control to current user
   $acl = Get-Acl "C:\RawrXD"
   $accessRule = New-Object System.Security.AccessControl.FileSystemAccessRule([System.Security.Principal.WindowsIdentity]::GetCurrent().Name, "FullControl", "ContainerInherit,ObjectInherit", "None", "Allow")
   $acl.SetAccessRule($accessRule)
   Set-Acl "C:\RawrXD" $acl
   ```

3. **Use Alternative Log Path:**
   ```powershell
   # Modify config.json to use user-writable location
   $config = Get-Content "C:\RawrXD\Production\config.json" | ConvertFrom-Json
   $config.deployment.logPath = "$env:USERPROFILE\RawrXD\Logs\deployment.log"
   $config | ConvertTo-Json | Set-Content "C:\RawrXD\Production\config.json"
   ```

### Issue 2: Module Import Failures

**Symptoms:**
- "The specified module was not loaded because no valid module file was found"
- "Import-Module : Could not load file or assembly"
- Syntax errors in module files

**Solutions:**

1. **Verify Module Paths:**
   ```powershell
   # Check if modules exist
   Test-Path "C:\RawrXD\Autonomous\*.psm1"
   Test-Path "C:\RawrXD\Production\*.psm1"
   ```

2. **Fix Syntax Errors:**
   ```powershell
   # Test module syntax
   $modulePath = "C:\RawrXD\Production\RawrXD.DeploymentOrchestrator.psm1"
   $content = Get-Content $modulePath -Raw
   [System.Management.Automation.Language.Parser]::ParseInput($content, [ref]$null, [ref]$null)
   ```

3. **Recreate Corrupted Modules:**
   ```powershell
   # Recreate from backup or regenerate
   Copy-Item "C:\RawrXD\Backups\*.psm1" "C:\RawrXD\Production\" -Force
   ```

### Issue 3: Execution Policy Restrictions

**Symptoms:**
- "File cannot be loaded because running scripts is disabled on this system"
- Execution policy is "Restricted"

**Solutions:**

1. **Set Execution Policy:**
   ```powershell
   # Set to RemoteSigned (recommended)
   Set-ExecutionPolicy RemoteSigned -Scope CurrentUser
   
   # Or set to Unrestricted (less secure)
   Set-ExecutionPolicy Unrestricted -Scope CurrentUser
   ```

2. **Bypass for Current Session:**
   ```powershell
   # Run script with bypass
   powershell -ExecutionPolicy Bypass -File "C:\RawrXD\Production\Execute-UltimateDeployment.ps1"
   ```

### Issue 4: .NET Framework Version Issues

**Symptoms:**
- "This assembly is built with a newer version of .NET"
- Missing .NET Framework dependencies

**Solutions:**

1. **Install .NET Framework 4.7.2+:**
   ```powershell
   # Check installed version
   Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\NET Framework Setup\NDP\v4\Full" -Name Release
   
   # Download and install from Microsoft website if needed
   ```

2. **Update PowerShell:**
   ```powershell
   # Install PowerShell 7+ for better .NET support
   winget install Microsoft.PowerShell
   ```

### Issue 5: Insufficient Resources

**Symptoms:**
- "Insufficient disk space"
- "Out of memory" errors
- Performance degradation

**Solutions:**

1. **Free Disk Space:**
   ```powershell
   # Clean temp files
   Remove-Item "$env:TEMP\*" -Recurse -Force -ErrorAction SilentlyContinue
   
   # Clean Windows temp
   Remove-Item "C:\Windows\Temp\*" -Recurse -Force -ErrorAction SilentlyContinue
   ```

2. **Optimize Memory Usage:**
   ```powershell
   # Clear PowerShell session memory
   Get-Variable | Where-Object { $_.Name -like "temp*" } | Remove-Variable
   [System.GC]::Collect()
   ```

### Issue 6: Configuration File Errors

**Symptoms:**
- "Invalid JSON format"
- "Configuration file not found"
- Missing configuration properties

**Solutions:**

1. **Validate JSON Syntax:**
   ```powershell
   # Test config.json
   try {
       $config = Get-Content "C:\RawrXD\Production\config.json" -Raw | ConvertFrom-Json
       Write-Host "Configuration valid"
   } catch {
       Write-Host "Invalid configuration: $_"
   }
   ```

2. **Regenerate Configuration:**
   ```powershell
   # Create default configuration
   $defaultConfig = @{
       name = "RawrXD Production Deployment"
       version = "3.0.0"
       deployment = @{
           mode = "Maximum"
           sourcePath = "C:\\RawrXD\\Autonomous"
           targetPath = "C:\\RawrXD\\Production"
           logPath = "C:\\RawrXD\\Logs\\deployment.log"
           backupPath = "C:\\RawrXD\\Backups"
       }
   }
   $defaultConfig | ConvertTo-Json | Set-Content "C:\RawrXD\Production\config.json"
   ```

## Advanced Troubleshooting

### Debug Mode Execution

Enable detailed logging and debugging:

```powershell
# Set verbose logging
$VerbosePreference = "Continue"

# Enable debugging
Set-PSDebug -Trace 1

# Run deployment with full output
powershell -File "C:\RawrXD\Production\Execute-UltimateDeployment.ps1" -Verbose
```

### Module-Specific Issues

#### RawrXD.DeploymentOrchestrator Issues

**Problem:** Deployment pipeline failures

**Debugging:**
```powershell
# Test individual phases
Import-Module "C:\RawrXD\Production\RawrXD.DeploymentOrchestrator.psm1" -Force

# Test system validation
Start-Phase1-SystemValidation -Mode "Maximum"

# Test module import
Import-DeploymentOrchestrator -ModulePath "C:\RawrXD\Production\RawrXD.DeploymentOrchestrator.psm1"
```

#### RawrXD.AgenticFunctions Issues

**Problem:** Self-mutation or monitoring failures

**Debugging:**
```powershell
# Test agentic functions
Import-Module "C:\RawrXD\Production\RawrXD.AgenticFunctions.psm1" -Force

# Test self-mutation
Invoke-SelfMutation -Mode "Standard"

# Test monitoring
Start-RealTimeMonitoring -Config @{alert_thresholds=@{cpu_usage_percent=90;memory_usage_percent=85}}
```

### Performance Issues

#### Slow Deployment

**Diagnosis:**
```powershell
# Check system performance
Get-Counter "\Processor(_Total)\% Processor Time" -SampleInterval 1 -MaxSamples 5
Get-Counter "\Memory\Available MBytes" -SampleInterval 1 -MaxSamples 5
Get-Counter "\PhysicalDisk(_Total)\% Disk Time" -SampleInterval 1 -MaxSamples 5
```

**Solutions:**
- Run deployment during off-peak hours
- Close unnecessary applications
- Use "Standard" mode instead of "Maximum"
- Increase system resources

#### Memory Leaks

**Detection:**
```powershell
# Monitor memory usage
$process = Get-Process -Name "powershell"
$process.WorkingSet / 1MB
```

**Solutions:**
- Restart PowerShell session between deployments
- Use `[System.GC]::Collect()` to force garbage collection
- Monitor with performance counters

### Security Issues

#### Permission Problems

**Diagnosis:**
```powershell
# Check file permissions
Get-Acl "C:\RawrXD" | Format-List

# Check service permissions
Get-Service | Where-Object {$_.Name -like "*RawrXD*"}
```

**Solutions:**
- Run with appropriate privileges
- Configure service accounts properly
- Use principle of least privilege

#### Audit Logging Failures

**Diagnosis:**
```powershell
# Check audit configuration
Get-AuditPolicy | Where-Object {$_.Subcategory -like "*File*"}

# Test audit logging
Write-AuditLog -Message "Test audit entry" -Level "Info"
```

**Solutions:**
- Enable audit policies
- Configure event log settings
- Verify log file permissions

## Recovery Procedures

### Failed Deployment Recovery

1. **Restore from Backup:**
   ```powershell
   # Find latest backup
   $backupDir = Get-ChildItem "C:\RawrXD\Backups" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
   
   # Restore modules
   Copy-Item "$backupDir\*.psm1" "C:\RawrXD\Production\" -Force
   ```

2. **Rollback Configuration:**
   ```powershell
   # Restore configuration from backup
   Copy-Item "C:\RawrXD\Backups\config.json" "C:\RawrXD\Production\config.json" -Force
   ```

3. **Clean Failed Deployment:**
   ```powershell
   # Remove failed deployment artifacts
   Remove-Item "C:\RawrXD\Production\*.tmp" -Force
   Remove-Item "C:\RawrXD\Production\*.bak" -Force
   ```

### System Corruption Recovery

1. **Reinstall Modules:**
   ```powershell
   # Recreate from source
   Copy-Item "C:\RawrXD\Autonomous\*.psm1" "C:\RawrXD\Production\" -Force
   ```

2. **Reset Configuration:**
   ```powershell
   # Use default configuration
   Remove-Item "C:\RawrXD\Production\config.json" -Force
   # Recreate using default template
   ```

3. **Clean System State:**
   ```powershell
   # Clear temporary files
   Remove-Item "C:\RawrXD\Logs\*" -Force
   Remove-Item "C:\RawrXD\Backups\*" -Force -Recurse
   ```

## Prevention Best Practices

### Regular Maintenance

1. **Backup Configuration:**
   ```powershell
   # Regular configuration backups
   $backupDir = "C:\RawrXD\Backups\$(Get-Date -Format 'yyyyMMdd_HHmmss')"
   New-Item -Path $backupDir -ItemType Directory -Force
   Copy-Item "C:\RawrXD\Production\*" $backupDir -Force
   ```

2. **Monitor System Health:**
   ```powershell
   # Regular health checks
   Test-SystemHealth -Config (Get-Content "C:\RawrXD\Production\config.json" | ConvertFrom-Json)
   ```

3. **Update Dependencies:**
   ```powershell
   # Keep PowerShell updated
   winget upgrade Microsoft.PowerShell
   ```

### Security Practices

1. **Regular Security Scans:**
   ```powershell
   # Security validation
   Validate-Security -Config (Get-Content "C:\RawrXD\Production\config.json" | ConvertFrom-Json).security
   ```

2. **Access Control:**
   ```powershell
   # Regular permission audits
   Get-Acl "C:\RawrXD" | Where-Object {$_.Access.IdentityReference -eq "BUILTIN\\Users"}
   ```

3. **Log Monitoring:**
   ```powershell
   # Regular log review
   Get-Content "C:\RawrXD\Logs\*.log" -Tail 100 | Where-Object {$_ -like "*Error*" -or $_ -like "*Warning*"}
   ```

## Support Resources

### Log Analysis

Key log locations:
- `C:\RawrXD\Logs\UltimateDeployment_*.log`
- `C:\RawrXD\Logs\AutonomousAgent_*.log`
- `C:\RawrXD\Logs\audit.log`
- `C:\RawrXD\Logs\monitoring.log`

### Error Code Reference

| Exit Code | Meaning | Action |
|-----------|---------|--------|
| 0 | Success | No action needed |
| 1 | General failure | Check logs for details |
| 2 | Prerequisites failed | Fix system requirements |
| 3 | Module import failure | Verify module files |
| 4 | Deployment pipeline failure | Check deployment logs |
| 5 | Configuration error | Validate config files |

### Contact Information

For additional support:
- Check deployment guide documentation
- Review GitHub repository issues
- Contact development team

## Emergency Procedures

### Immediate System Failure

1. **Stop All Processes:**
   ```powershell
   # Stop PowerShell processes
   Get-Process -Name "powershell" | Where-Object {$_.ProcessName -like "*RawrXD*"} | Stop-Process -Force
   ```

2. **Isolate System:**
   ```powershell
   # Disable network if necessary
   Get-NetAdapter | Disable-NetAdapter -Confirm:$false
   ```

3. **Contact Support:**
   - Provide detailed error information
   - Include relevant log files
   - Describe steps to reproduce

This troubleshooting guide should help resolve most issues encountered during RawrXD production deployment. For persistent problems, consult the detailed deployment guide or contact technical support.