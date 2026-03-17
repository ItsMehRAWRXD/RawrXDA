# RawrXD-Git.psm1 - Git integration module

function Initialize-GitIntegration {
    Write-RawrXDLog "Initializing Git integration..." -Level INFO -Component "Git"
    
    # Check if Git is available
    $gitAvailable = Test-GitAvailability
    $global:RawrXD.GitAvailable = $gitAvailable
    
    if ($gitAvailable) {
        Write-RawrXDLog "Git integration initialized successfully" -Level SUCCESS -Component "Git"
        
        # Set up Git status monitoring if enabled
        if ($global:RawrXD.Settings.Git.AutoDetectRepos) {
            Setup-GitMonitoring
        }
    }
    else {
        Write-RawrXDLog "Git not available - Git features will be disabled" -Level WARNING -Component "Git"
    }
}

function Test-GitAvailability {
    try {
        $gitVersion = git --version 2>$null
        if ($gitVersion) {
            Write-RawrXDLog "Git detected: $gitVersion" -Level SUCCESS -Component "Git"
            return $true
        }
        return $false
    }
    catch {
        return $false
    }
}

function Setup-GitMonitoring {
    # Create a timer to periodically check Git status
    $global:RawrXD.GitMonitorTimer = New-Object System.Windows.Forms.Timer
    $global:RawrXD.GitMonitorTimer.Interval = 5000  # 5 seconds
    $global:RawrXD.GitMonitorTimer.add_Tick({
        try {
            Update-GitStatusDisplay
        }
        catch {
            Write-RawrXDLog "Error in Git monitoring: $($_.Exception.Message)" -Level ERROR -Component "Git"
        }
    })
    $global:RawrXD.GitMonitorTimer.Start()
    Write-RawrXDLog "Git monitoring started" -Level INFO -Component "Git"
}

function Update-GitStatusDisplay {
    if (-not $global:RawrXD.GitAvailable) { return }
    
    $currentPath = if ($global:RawrXD.CurrentFile) {
        Split-Path $global:RawrXD.CurrentFile -Parent
    }
    elseif ($global:RawrXD.Explorer.CurrentPath) {
        $global:RawrXD.Explorer.CurrentPath
    }
    else {
        (Get-Location).Path
    }
    
    $gitStatus = Get-GitStatus -Path $currentPath
    
    # Update status bar
    if ($global:RawrXD.Components.GitStatusLabel) {
        $statusText = if ($gitStatus.IsGitRepo) {
            $changeIndicator = if ($gitStatus.HasChanges) { " *" } else { "" }
            "Git: $($gitStatus.Branch)$changeIndicator"
        }
        else {
            "Git: ---"
        }
        $global:RawrXD.Components.GitStatusLabel.Text = $statusText
    }
    
    # Update form title if enabled
    if ($global:RawrXD.Settings.Git.ShowBranchInTitle -and $global:RawrXD.Form) {
        $baseTitle = "RawrXD v$($global:RawrXD.Version) - AI-Powered Editor"
        
        if ($global:RawrXD.CurrentFile) {
            $fileName = [System.IO.Path]::GetFileName($global:RawrXD.CurrentFile)
            $baseTitle += " - $fileName"
            if ($global:RawrXD.Editor.IsModified) {
                $baseTitle += " *"
            }
        }
        
        if ($gitStatus.IsGitRepo) {
            $baseTitle += " [$($gitStatus.Branch)]"
        }
        
        $global:RawrXD.Form.Text = $baseTitle
    }
}

function Get-GitBranches {
    param([string]$Path = (Get-Location).Path)
    
    if (-not $global:RawrXD.GitAvailable) {
        return @()
    }
    
    try {
        Push-Location $Path
        $branches = git branch --all 2>$null
        if ($branches) {
            return $branches | ForEach-Object { 
                $branch = $_.Trim()
                if ($branch.StartsWith('*')) {
                    @{ Name = $branch.Substring(2).Trim(); IsCurrent = $true }
                }
                else {
                    @{ Name = $branch.Trim(); IsCurrent = $false }
                }
            }
        }
        return @()
    }
    catch {
        Write-RawrXDLog "Error getting Git branches: $($_.Exception.Message)" -Level ERROR -Component "Git"
        return @()
    }
    finally {
        Pop-Location
    }
}

function Switch-GitBranch {
    param(
        [string]$BranchName,
        [string]$Path = (Get-Location).Path
    )
    
    if (-not $global:RawrXD.GitAvailable) {
        return $false
    }
    
    try {
        Push-Location $Path
        $result = git checkout $BranchName 2>&1
        
        if ($LASTEXITCODE -eq 0) {
            Write-RawrXDLog "Switched to branch: $BranchName" -Level SUCCESS -Component "Git"
            Update-GitStatusDisplay
            return $true
        }
        else {
            Write-RawrXDLog "Failed to switch branch: $result" -Level ERROR -Component "Git"
            return $false
        }
    }
    catch {
        Write-RawrXDLog "Error switching branch: $($_.Exception.Message)" -Level ERROR -Component "Git"
        return $false
    }
    finally {
        Pop-Location
    }
}

function Create-GitBranch {
    param(
        [string]$BranchName,
        [string]$Path = (Get-Location).Path,
        [bool]$SwitchToBranch = $true
    )
    
    if (-not $global:RawrXD.GitAvailable) {
        return $false
    }
    
    try {
        Push-Location $Path
        
        if ($SwitchToBranch) {
            $result = git checkout -b $BranchName 2>&1
        }
        else {
            $result = git branch $BranchName 2>&1
        }
        
        if ($LASTEXITCODE -eq 0) {
            Write-RawrXDLog "Created branch: $BranchName" -Level SUCCESS -Component "Git"
            Update-GitStatusDisplay
            return $true
        }
        else {
            Write-RawrXDLog "Failed to create branch: $result" -Level ERROR -Component "Git"
            return $false
        }
    }
    catch {
        Write-RawrXDLog "Error creating branch: $($_.Exception.Message)" -Level ERROR -Component "Git"
        return $false
    }
    finally {
        Pop-Location
    }
}

function Add-GitFiles {
    param(
        [string[]]$FilePaths,
        [string]$Path = (Get-Location).Path
    )
    
    if (-not $global:RawrXD.GitAvailable) {
        return $false
    }
    
    try {
        Push-Location $Path
        
        foreach ($file in $FilePaths) {
            $result = git add $file 2>&1
            if ($LASTEXITCODE -ne 0) {
                Write-RawrXDLog "Failed to add file $file`: $result" -Level ERROR -Component "Git"
                return $false
            }
        }
        
        Write-RawrXDLog "Added $($FilePaths.Count) file(s) to Git staging" -Level SUCCESS -Component "Git"
        Update-GitStatusDisplay
        return $true
    }
    catch {
        Write-RawrXDLog "Error adding files to Git: $($_.Exception.Message)" -Level ERROR -Component "Git"
        return $false
    }
    finally {
        Pop-Location
    }
}

function Commit-GitChanges {
    param(
        [string]$Message,
        [string]$Path = (Get-Location).Path,
        [bool]$AddAllFiles = $false
    )
    
    if (-not $global:RawrXD.GitAvailable) {
        return $false
    }
    
    try {
        Push-Location $Path
        
        if ($AddAllFiles) {
            $addResult = git add -A 2>&1
            if ($LASTEXITCODE -ne 0) {
                Write-RawrXDLog "Failed to add files: $addResult" -Level ERROR -Component "Git"
                return $false
            }
        }
        
        $result = git commit -m $Message 2>&1
        
        if ($LASTEXITCODE -eq 0) {
            Write-RawrXDLog "Committed changes: $Message" -Level SUCCESS -Component "Git"
            Update-GitStatusDisplay
            return $true
        }
        else {
            Write-RawrXDLog "Failed to commit: $result" -Level ERROR -Component "Git"
            return $false
        }
    }
    catch {
        Write-RawrXDLog "Error committing changes: $($_.Exception.Message)" -Level ERROR -Component "Git"
        return $false
    }
    finally {
        Pop-Location
    }
}

function Push-GitChanges {
    param(
        [string]$Remote = "origin",
        [string]$Branch = "",
        [string]$Path = (Get-Location).Path
    )
    
    if (-not $global:RawrXD.GitAvailable) {
        return $false
    }
    
    try {
        Push-Location $Path
        
        $pushCommand = if ($Branch) { 
            "git push $Remote $Branch" 
        } else { 
            "git push" 
        }
        
        $result = Invoke-Expression "$pushCommand 2>&1"
        
        if ($LASTEXITCODE -eq 0) {
            Write-RawrXDLog "Pushed changes to remote repository" -Level SUCCESS -Component "Git"
            return $true
        }
        else {
            Write-RawrXDLog "Failed to push: $result" -Level ERROR -Component "Git"
            return $false
        }
    }
    catch {
        Write-RawrXDLog "Error pushing changes: $($_.Exception.Message)" -Level ERROR -Component "Git"
        return $false
    }
    finally {
        Pop-Location
    }
}

function Pull-GitChanges {
    param(
        [string]$Remote = "origin",
        [string]$Branch = "",
        [string]$Path = (Get-Location).Path
    )
    
    if (-not $global:RawrXD.GitAvailable) {
        return $false
    }
    
    try {
        Push-Location $Path
        
        $pullCommand = if ($Branch) { 
            "git pull $Remote $Branch" 
        } else { 
            "git pull" 
        }
        
        $result = Invoke-Expression "$pullCommand 2>&1"
        
        if ($LASTEXITCODE -eq 0) {
            Write-RawrXDLog "Pulled changes from remote repository" -Level SUCCESS -Component "Git"
            Update-GitStatusDisplay
            return $true
        }
        else {
            Write-RawrXDLog "Failed to pull: $result" -Level ERROR -Component "Git"
            return $false
        }
    }
    catch {
        Write-RawrXDLog "Error pulling changes: $($_.Exception.Message)" -Level ERROR -Component "Git"
        return $false
    }
    finally {
        Pop-Location
    }
}

function Show-GitLog {
    param(
        [int]$Count = 10,
        [string]$Path = (Get-Location).Path
    )
    
    if (-not $global:RawrXD.GitAvailable) {
        return @()
    }
    
    try {
        Push-Location $Path
        $logEntries = git log --oneline -n $Count 2>$null
        
        if ($logEntries) {
            return $logEntries | ForEach-Object {
                $parts = $_ -split ' ', 2
                @{
                    Hash = $parts[0]
                    Message = if ($parts.Length -gt 1) { $parts[1] } else { "" }
                }
            }
        }
        return @()
    }
    catch {
        Write-RawrXDLog "Error getting Git log: $($_.Exception.Message)" -Level ERROR -Component "Git"
        return @()
    }
    finally {
        Pop-Location
    }
}

function Initialize-GitRepository {
    param([string]$Path = (Get-Location).Path)
    
    if (-not $global:RawrXD.GitAvailable) {
        return $false
    }
    
    try {
        Push-Location $Path
        $result = git init 2>&1
        
        if ($LASTEXITCODE -eq 0) {
            Write-RawrXDLog "Initialized Git repository in: $Path" -Level SUCCESS -Component "Git"
            Update-GitStatusDisplay
            return $true
        }
        else {
            Write-RawrXDLog "Failed to initialize Git repository: $result" -Level ERROR -Component "Git"
            return $false
        }
    }
    catch {
        Write-RawrXDLog "Error initializing Git repository: $($_.Exception.Message)" -Level ERROR -Component "Git"
        return $false
    }
    finally {
        Pop-Location
    }
}

function Stop-GitIntegration {
    if ($global:RawrXD.GitMonitorTimer) {
        $global:RawrXD.GitMonitorTimer.Stop()
        $global:RawrXD.GitMonitorTimer.Dispose()
        $global:RawrXD.GitMonitorTimer = $null
    }
    Write-RawrXDLog "Git integration stopped" -Level INFO -Component "Git"
}

# Export functions
Export-ModuleMember -Function @(
    'Initialize-GitIntegration',
    'Test-GitAvailability',
    'Update-GitStatusDisplay',
    'Get-GitBranches',
    'Switch-GitBranch',
    'Create-GitBranch',
    'Add-GitFiles',
    'Commit-GitChanges',
    'Push-GitChanges',
    'Pull-GitChanges',
    'Show-GitLog',
    'Initialize-GitRepository',
    'Stop-GitIntegration'
)