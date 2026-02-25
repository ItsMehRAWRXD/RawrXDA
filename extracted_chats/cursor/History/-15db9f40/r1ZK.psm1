# OpenMemory Storage Module
# CRUD for memories & users – JSONL + in-memory index

$script:Memories = @()
$script:Users   = @{}
$script:Vectors = @()
$script:StoreRoot = ""

function Initialize-OMStorage {
    param(
        [string]$Root = "$PSScriptRoot\..\..\Store"
    )
    
    $script:StoreRoot = $Root
    $script:Memories = @()
    $script:Vectors = @()
    
    # Create storage folder if missing
    if (-not (Test-Path $Root)) { 
        New-Item $Root -ItemType Directory -Force | Out-Null 
        Write-Host "[OpenMemory] 📁 Created storage folder: $Root" -ForegroundColor Cyan
    }
    
    # Load memories
    if (Test-Path "$Root\memories.jsonl") {
        Get-Content "$Root\memories.jsonl" -ErrorAction SilentlyContinue | ForEach-Object {
            try {
                $memory = $_ | ConvertFrom-Json
                $script:Memories += $memory
            } catch {
                Write-Warning "[OpenMemory] ⚠️ Skipped corrupt memory line"
            }
        }
        Write-Host "[OpenMemory] 💾 Loaded $($script:Memories.Count) memories" -ForegroundColor Green
    }
    
    # Load users
    if (Test-Path "$Root\users.json") {
        try {
            $usersJson = Get-Content "$Root\users.json" | ConvertFrom-Json
            $script:Users = @{}
            if ($usersJson) {
                $usersJson.PSObject.Properties | ForEach-Object {
                    $script:Users[$_.Name] = $_.Value
                }
            }
            Write-Host "[OpenMemory] 👤 Loaded $($script:Users.Count) users" -ForegroundColor Green
        } catch {
            Write-Warning "[OpenMemory] ⚠️ Failed to load users, starting fresh"
            $script:Users = @{}
        }
    }
    
    # Load embeddings
    if (Test-Path "$Root\embeddings.jsonl") {
        Get-Content "$Root\embeddings.jsonl" -ErrorAction SilentlyContinue | ForEach-Object {
            try {
                $vector = $_ | ConvertFrom-Json
                $script:Vectors += $vector
            } catch {
                Write-Warning "[OpenMemory] ⚠️ Skipped corrupt embedding line"
            }
        }
        Write-Host "[OpenMemory] 🧬 Loaded $($script:Vectors.Count) embeddings" -ForegroundColor Green
    }
    
    Write-Host "[OpenMemory] ✅ Storage initialized" -ForegroundColor Green
}

function Save-OMStorage {
    param([switch]$Quiet)
    
    try {
        # Save memories as JSONL (one JSON object per line)
        $script:Memories | ForEach-Object { $_ | ConvertTo-Json -Compress -Depth 10 } |
            Set-Content "$script:StoreRoot\memories.jsonl" -Encoding UTF8
        
        # Save users as single JSON
        $script:Users | ConvertTo-Json -Compress -Depth 10 |
            Set-Content "$script:StoreRoot\users.json" -Encoding UTF8
        
        # Save embeddings as JSONL
        $script:Vectors | ForEach-Object { $_ | ConvertTo-Json -Compress -Depth 10 } |
            Set-Content "$script:StoreRoot\embeddings.jsonl" -Encoding UTF8
        
        if (-not $Quiet) {
            Write-Host "[OpenMemory] 💾 Storage saved ($($script:Memories.Count) memories)" -ForegroundColor Green
        }
    } catch {
        Write-Error "[OpenMemory] ❌ Failed to save storage: $_"
    }
}

function Add-OMMemory {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Content,
        
        [Parameter(Mandatory=$true)]
        [string]$UserId,
        
        [ValidateSet('Semantic','Episodic','Procedural','Emotional','Reflective')]
        [string]$Sector = 'Semantic',
        
        [ValidateRange(0, 10)]
        [int]$Salience = 1,
        
        [hashtable]$Metadata = @{}
    )
    
    $id = [guid]::NewGuid().ToString('n')
    
    $memory = [PSCustomObject]@{
        id       = $id
        content  = $Content
        user_id  = $UserId
        sector   = $Sector
        created  = [datetime]::UtcNow.ToString('o')
        accessed = [datetime]::UtcNow.ToString('o')
        salience = $Salience
        decay    = 1.0
        metadata = $Metadata
    }
    
    $script:Memories += $memory
    
    # Generate and store embedding
    $embedding = Get-OMEmbedding $Content
    $vectorEntry = [PSCustomObject]@{
        id     = $id
        sector = $Sector
        vector = $embedding
    }
    $script:Vectors += $vectorEntry
    
    # Update user summary
    if (-not $script:Users.ContainsKey($UserId)) {
        $script:Users[$UserId] = @{
            summary   = ''
            lastSeen  = [datetime]::UtcNow.ToString('o')
            memoryCount = 0
        }
    }
    
    $script:Users[$UserId].lastSeen = [datetime]::UtcNow.ToString('o')
    $script:Users[$UserId].memoryCount++
    
    Save-OMStorage -Quiet
    
    Write-Host "[OpenMemory] ➕ Added memory: $($Content.Substring(0, [Math]::Min(50, $Content.Length)))..." -ForegroundColor Cyan
    
    return $memory
}

function Get-OMMemory {
    param(
        [string]$Id,
        [string]$UserId,
        [string]$Sector
    )
    
    $filtered = $script:Memories
    
    if ($Id) {
        $filtered = $filtered | Where-Object { $_.id -eq $Id }
    }
    
    if ($UserId) {
        $filtered = $filtered | Where-Object { $_.user_id -eq $UserId }
    }
    
    if ($Sector) {
        $filtered = $filtered | Where-Object { $_.sector -eq $Sector }
    }
    
    return $filtered
}

function Remove-OMMemory {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Id
    )
    
    $script:Memories = $script:Memories | Where-Object { $_.id -ne $Id }
    $script:Vectors = $script:Vectors | Where-Object { $_.id -ne $Id }
    
    Save-OMStorage -Quiet
    Write-Host "[OpenMemory] 🗑️ Removed memory: $Id" -ForegroundColor Yellow
}

function Get-OMUserSummary {
    param(
        [Parameter(Mandatory=$true)]
        [string]$UserId
    )
    
    if ($script:Users.ContainsKey($UserId)) {
        $userMemories = Get-OMMemory -UserId $UserId
        
        return [PSCustomObject]@{
            UserId      = $UserId
            LastSeen    = $script:Users[$UserId].lastSeen
            MemoryCount = $userMemories.Count
            Sectors     = ($userMemories | Group-Object sector | Select-Object Name, Count)
            Summary     = $script:Users[$UserId].summary
        }
    } else {
        Write-Warning "[OpenMemory] ⚠️ User not found: $UserId"
        return $null
    }
}

function Update-OMUserSummary {
    param(
        [Parameter(Mandatory=$true)]
        [string]$UserId,
        
        [Parameter(Mandatory=$true)]
        [string]$Summary
    )
    
    if (-not $script:Users.ContainsKey($UserId)) {
        $script:Users[$UserId] = @{
            summary      = ''
            lastSeen     = [datetime]::UtcNow.ToString('o')
            memoryCount  = 0
        }
    }
    
    $script:Users[$UserId].summary = $Summary
    Save-OMStorage -Quiet
    
    Write-Host "[OpenMemory] 📝 Updated user summary: $UserId" -ForegroundColor Cyan
}

function Clear-OMStorage {
    param(
        [switch]$Force
    )
    
    if (-not $Force) {
        $script:Memories = @()
        $script:Vectors = @()
        $script:Users = @{}
    } else {
        $script:Memories = @()
        $script:Vectors = @()
        $script:Users = @{}
    }
    
    Save-OMStorage
    
    Write-Host "[OpenMemory] 🧹 Storage cleared" -ForegroundColor Yellow
}

# Export functions
Export-ModuleMember -Function @(
    'Initialize-OMStorage',
    'Save-OMStorage',
    'Add-OMMemory',
    'Get-OMMemory',
    'Remove-OMMemory',
    'Get-OMUserSummary',
    'Update-OMUserSummary',
    'Clear-OMStorage'
)

# Expose internal collections for advanced usage
Export-ModuleMember -Variable @('Memories', 'Users', 'Vectors')

