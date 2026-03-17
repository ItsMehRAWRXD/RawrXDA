<#
.SYNOPSIS
    Digests a codebase by removing duplicates and generating a source summary.
    Mimics a "compiler pre-processing" step to audit the entire project content.

.DESCRIPTION
    1. Scans all files in the target directory recursively.
    2. Calculates SHA256 hashes to identify exact duplicate files.
    3. Removes duplicates (keeps the shortest path/original instance).
    4. Generates a 'digest' report containing:
       - List of unique files.
       - Source code stats (LOC, Size).
       - A concatenated 'mega-file' of all source code for review.
#>

param(
    [string]$TargetDir = "$PSScriptRoot",
    [string]$ReportDir = "$PSScriptRoot\digest_output"
)

# Configuration
$SourceExtensions = @(".cpp", ".c", ".h", ".hpp", ".asm", ".s", ".py", ".js", ".ts", ".ps1", ".bat", ".cmd", ".java", ".cs")
$LogFile = "digest_log.txt"

# Setup
if (!(Test-Path $ReportDir)) { New-Item -ItemType Directory -Path $ReportDir | Out-Null }
$LogPath = Join-Path $ReportDir $LogFile
"Starting Digest at $(Get-Date)" | Out-File $LogPath

function Write-Log {
    param([string]$Message)
    Write-Host $Message
    "$([DateTime]::Now): $Message" | Out-File $LogPath -Append
}

Write-Log "Scanning directory: $TargetDir"

# 1. Collection & Deduplication
Write-Log "Phase 1: collecting files..."
$AllFiles = Get-ChildItem -Path $TargetDir -Recurse -File

Write-Log "Found $($AllFiles.Count) total files. Calculating hashes..."

$HashMap = @{}
$DuplicateCount = 0
$RemovedCount = 0
$UniqueFiles = @()

$TotalFiles = $AllFiles.Count
$Current = 0

foreach ($File in $AllFiles) {
    $Current++
    if ($Current % 1000 -eq 0) { Write-Progress -Activity "Hashing Files" -Status "$Current / $TotalFiles" -PercentComplete (($Current / $TotalFiles) * 100) }
    
    try {
        $Stream = [System.IO.File]::OpenRead($File.FullName)
        $Hash = [BitConverter]::ToString((New-Object System.Security.Cryptography.SHA256Managed).ComputeHash($Stream))
        $Stream.Close()
    }
    catch {
        Write-Log "Error reading $($File.FullName): $_"
        continue
    }

    if ($HashMap.ContainsKey($Hash)) {
        # Duplicate found
        $DuplicateCount++
        $Original = $HashMap[$Hash]
        
        # Heuristic: Keep the one with a shorter path or seemingly more 'canonical' location
        # For now, we keep the first one found, delete the current one.
        Write-Log "Duplicate found: '$($File.FullName)' is same as '$($Original.FullName)'"
        
        try {
            Remove-Item -Path $File.FullName -Force
            $RemovedCount++
            Write-Log "  -> Removed duplicate: $($File.Name)"
        }
        catch {
            Write-Log "  -> Failed to remove: $_"
        }
    }
    else {
        # New unique file
        $HashMap[$Hash] = $File
        $UniqueFiles += $File
    }
}

Write-Log "Deduplication complete."
Write-Log "Original Files: $TotalFiles"
Write-Log "Unique Files: $($UniqueFiles.Count)"
Write-Log "Duplicates Removed: $RemovedCount"

# 2. Digestion (Analysis & Concatenation)
Write-Log "Phase 2: Digesting Source Code..."

$DigestFile = Join-Path $ReportDir "full_source_digest.txt"
$ManifestFile = Join-Path $ReportDir "project_manifest.json"
$Manifest = @()

"PROJECT SOURCE DIGEST - $(Get-Date)" | Out-File $DigestFile
"==========================================" | Out-File $DigestFile -Append

$Current = 0
$TotalUnique = $UniqueFiles.Count

foreach ($File in $UniqueFiles) {
    $Current++
    if ($Current % 500 -eq 0) { Write-Progress -Activity "Digesting Source" -Status "$Current / $TotalUnique" -PercentComplete (($Current / $TotalUnique) * 100) }

    $Ext = $File.Extension.ToLower()
    $IsSource = $SourceExtensions -contains $Ext
    
    $FileInfo = @{
        Path = $File.FullName
        Size = $File.Length
        Extension = $Ext
        IsSource = $IsSource
        LastModified = $File.LastWriteTime
    }
    $Manifest += $FileInfo

    if ($IsSource) {
        try {
            $Content = Get-Content -Path $File.FullName -Raw -ErrorAction SilentlyContinue
            if ($null -ne $Content) {
                # Append to digest file
                "--------------------------------------------------------------------------------" | Out-File $DigestFile -Append
                "FILE: $($File.FullName)" | Out-File $DigestFile -Append
                "SIZE: $($File.Length) bytes" | Out-File $DigestFile -Append
                "--------------------------------------------------------------------------------" | Out-File $DigestFile -Append
                $Content | Out-File $DigestFile -Append
                "`n" | Out-File $DigestFile -Append
            }
        }
        catch {
            Write-Log "Error reading content for digest: $($File.FullName)"
        }
    }
}

# Save Manifest
$Manifest | ConvertTo-Json -Depth 2 | Out-File $ManifestFile

Write-Log "Digest Complete."
Write-Log "Report generated at: $ReportDir"
Write-Log "Full Source Digest: $DigestFile"
