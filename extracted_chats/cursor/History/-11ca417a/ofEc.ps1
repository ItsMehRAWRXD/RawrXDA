# Cursor Chat Backup System
# Extracts all Cursor conversations from local storage

param(
    [string]$OutputPath = "D:\01-AI-Models\Chat-History\Downloaded\Cursor",
    [switch]$IncludeCurrent = $true,
    [switch]$DryRun = $false
)

Write-Host "💬 Cursor Chat Backup System" -ForegroundColor Green
Write-Host "============================" -ForegroundColor Cyan

# Create output directories
if (!(Test-Path $OutputPath)) {
    New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
    Write-Host "✓ Created output directory: $OutputPath" -ForegroundColor Green
}

# Cursor storage locations
$cursorPaths = @(
    "$env:APPDATA\Cursor\User\globalStorage",
    "$env:APPDATA\Cursor\logs",
    "$env:APPDATA\Cursor\CachedData",
    "$env:LOCALAPPDATA\Cursor\User\globalStorage"
)

function Get-CursorConversations {
    Write-Host "`n🔍 Searching for Cursor conversations..." -ForegroundColor Yellow
    
    $conversations = @()
    $foundPaths = @()
    
    foreach ($path in $cursorPaths) {
        if (Test-Path $path) {
            Write-Host "  📁 Checking: $path" -ForegroundColor White
            
            # Look for conversation files
            $conversationFiles = Get-ChildItem -Path $path -Recurse -Filter "*.json" | Where-Object {
                $_.Name -match "(conversation|chat|message|history)" -or
                $_.Name -match "cursor" -or
                $_.Name -match "ai"
            }
            
            foreach ($file in $conversationFiles) {
                try {
                    $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
                    if ($content -and $content.Length -gt 100) {
                        $foundPaths += $file.FullName
                        Write-Host "    ✓ Found: $($file.Name)" -ForegroundColor Green
                    }
                } catch {
                    Write-Host "    ⚠️ Error reading $($file.Name): $($_.Exception.Message)" -ForegroundColor Yellow
                }
            }
        }
    }
    
    return $foundPaths
}

function Parse-CursorConversation {
    param([string]$FilePath)
    
    try {
        $content = Get-Content $FilePath -Raw -Encoding UTF8
        $data = $content | ConvertFrom-Json -ErrorAction SilentlyContinue
        
        if ($data) {
            $conversation = @{
                id = [System.Guid]::NewGuid().ToString()
                source = "Cursor"
                timestamp = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ss.fffZ")
                filePath = $FilePath
                messages = @()
                metadata = @{
                    fileName = Split-Path $FilePath -Leaf
                    fileSize = (Get-Item $FilePath).Length
                    lastModified = (Get-Item $FilePath).LastWriteTime
                }
            }
            
            # Try to extract messages from various JSON structures
            if ($data.messages) {
                foreach ($msg in $data.messages) {
                    $conversation.messages += @{
                        role = $msg.role ?? "user"
                        content = $msg.content ?? $msg.text ?? $msg.message ?? ""
                        timestamp = $msg.timestamp ?? $msg.created_at ?? (Get-Date).ToString("yyyy-MM-ddTHH:mm:ss.fffZ")
                    }
                }
            } elseif ($data.conversations) {
                foreach ($conv in $data.conversations) {
                    if ($conv.messages) {
                        foreach ($msg in $conv.messages) {
                            $conversation.messages += @{
                                role = $msg.role ?? "user"
                                content = $msg.content ?? $msg.text ?? $msg.message ?? ""
                                timestamp = $msg.timestamp ?? $msg.created_at ?? (Get-Date).ToString("yyyy-MM-ddTHH:mm:ss.fffZ")
                            }
                        }
                    }
                }
            } else {
                # Try to extract from raw content
                $conversation.rawContent = $content
            }
            
            return $conversation
        }
    } catch {
        Write-Host "    ❌ Error parsing $FilePath`: $($_.Exception.Message)" -ForegroundColor Red
    }
    
    return $null
}

function Save-Conversation {
    param([object]$Conversation, [string]$OutputDir)
    
    if (!$Conversation -or $Conversation.messages.Count -eq 0) {
        return
    }
    
    $timestamp = (Get-Date).ToString("yyyyMMdd_HHmmss")
    $baseName = "cursor_conversation_$timestamp"
    
    # Save as JSON
    $jsonFile = Join-Path $OutputDir "$baseName.json"
    $Conversation | ConvertTo-Json -Depth 10 | Set-Content -Path $jsonFile -Encoding UTF8
    
    # Save as Markdown
    $markdownFile = Join-Path $OutputDir "$baseName.md"
    $markdown = @"
# Cursor Conversation - $($Conversation.timestamp)

**Source:** $($Conversation.source)  
**File:** $($Conversation.metadata.fileName)  
**Messages:** $($Conversation.messages.Count)  
**Last Modified:** $($Conversation.metadata.lastModified)

---

"@
    
    foreach ($msg in $Conversation.messages) {
        $role = if ($msg.role -eq "user") { "**You:**" } else { "**Cursor:**" }
        $markdown += @"

$role
```
$($msg.content)
```

---
"@
    }
    
    Set-Content -Path $markdownFile -Value $markdown -Encoding UTF8
    
    Write-Host "  ✓ Saved: $baseName ($($Conversation.messages.Count) messages)" -ForegroundColor Green
}

function Extract-CurrentConversation {
    Write-Host "`n📝 Extracting current conversation..." -ForegroundColor Yellow
    
    # This would need to be run from within Cursor to capture current chat
    # For now, we'll create a template
    $currentConv = @{
        id = "current_cursor_conversation"
        source = "Cursor"
        timestamp = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ss.fffZ")
        messages = @(
            @{
                role = "user"
                content = "This is a placeholder for the current conversation. Run this script from within Cursor to capture the actual conversation."
                timestamp = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ss.fffZ")
            }
        )
        metadata = @{
            note = "Current conversation placeholder - run from Cursor for actual content"
        }
    }
    
    return $currentConv
}

# Main execution
try {
    Write-Host "`n🎯 Starting Cursor chat backup..." -ForegroundColor Cyan
    
    if ($DryRun) {
        Write-Host "🔍 DRY RUN MODE - No files will be saved" -ForegroundColor Yellow
    }
    
    # Find conversation files
    $conversationFiles = Get-CursorConversations
    
    if ($conversationFiles.Count -eq 0) {
        Write-Host "⚠️ No Cursor conversation files found in standard locations" -ForegroundColor Yellow
        Write-Host "   This might be normal if Cursor stores data differently" -ForegroundColor White
    } else {
        Write-Host "`n📊 Found $($conversationFiles.Count) potential conversation files" -ForegroundColor Cyan
    }
    
    # Process each file
    $processedCount = 0
    foreach ($file in $conversationFiles) {
        if (!$DryRun) {
            $conversation = Parse-CursorConversation -FilePath $file
            if ($conversation) {
                Save-Conversation -Conversation $conversation -OutputDir $OutputPath
                $processedCount++
            }
        } else {
            Write-Host "  [DRY RUN] Would process: $file" -ForegroundColor Cyan
            $processedCount++
        }
    }
    
    # Include current conversation if requested
    if ($IncludeCurrent -and !$DryRun) {
        $currentConv = Extract-CurrentConversation
        Save-Conversation -Conversation $currentConv -OutputDir $OutputPath
        $processedCount++
    }
    
    Write-Host "`n🎉 Cursor chat backup complete!" -ForegroundColor Green
    Write-Host "📊 Processed: $processedCount conversations" -ForegroundColor Cyan
    Write-Host "📁 Saved to: $OutputPath" -ForegroundColor White
    
    if ($processedCount -eq 0) {
        Write-Host "`n💡 Tips:" -ForegroundColor Yellow
        Write-Host "   • Make sure Cursor is installed and has been used" -ForegroundColor White
        Write-Host "   • Check if Cursor stores data in a different location" -ForegroundColor White
        Write-Host "   • Try running this script while Cursor is open" -ForegroundColor White
    }
    
} catch {
    Write-Host "❌ Error during backup: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
