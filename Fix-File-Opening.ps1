# 🔧 File Opening Issue Fix
# Create a simplified version of file opening that works

Write-Host "🛠️ Creating Fixed File Opening Handler..." -ForegroundColor Cyan

try {
  # Create test file with absolute path
  $currentDir = Get-Location
  $testFile = Join-Path $currentDir "debug_test.txt"
    
  "This is a test file for debugging the file opening issue in RawrXD." | Set-Content -Path $testFile -Encoding UTF8
    
  Write-Host "✅ Created test file at: $testFile" -ForegroundColor Green
    
  if (Test-Path $testFile) {
    Write-Host "✅ File exists and is accessible" -ForegroundColor Green
        
    # Test reading with different methods
    try {
      $method1 = Get-Content $testFile -Raw
      Write-Host "✅ Method 1 (Get-Content): '$($method1.Trim())'" -ForegroundColor Green
    }
    catch {
      Write-Host "❌ Method 1 failed: $_" -ForegroundColor Red
    }
        
    try {
      $method2 = [System.IO.File]::ReadAllText($testFile)
      Write-Host "✅ Method 2 ([System.IO.File]::ReadAllText): '$($method2.Trim())'" -ForegroundColor Green
    }
    catch {
      Write-Host "❌ Method 2 failed: $_" -ForegroundColor Red
    }
        
    # Now create a simplified double-click handler replacement
    Write-Host "`n🔄 Creating Simplified File Opening Function..." -ForegroundColor Cyan
        
    $fixedHandler = @'
function Open-FileSimple {
    param([string]$FilePath)
    
    try {
        Write-Host "🔍 Opening file: $FilePath" -ForegroundColor Cyan
        
        # Basic validation
        if (-not (Test-Path $FilePath)) {
            Write-Host "❌ File not found: $FilePath" -ForegroundColor Red
            return $false
        }
        
        if (Test-Path $FilePath -PathType Container) {
            Write-Host "⚠️ Selected item is a directory, not a file" -ForegroundColor Yellow
            return $false
        }
        
        # Check file size
        $fileInfo = Get-Item $FilePath
        if ($fileInfo.Length -gt 10MB) {
            Write-Host "⚠️ File too large: $($fileInfo.Length) bytes" -ForegroundColor Yellow
            return $false
        }
        
        # Read content
        $content = [System.IO.File]::ReadAllText($FilePath)
        Write-Host "✅ Content read: $($content.Length) characters" -ForegroundColor Green
        
        # Simulate editor assignment
        Write-Host "📝 Would assign to editor: '$($content.Substring(0, [Math]::Min(50, $content.Length)))...'" -ForegroundColor Yellow
        
        return $true
        
    } catch {
        Write-Host "❌ Error opening file: $_" -ForegroundColor Red
        return $false
    }
}
'@
        
    # Execute the function
    Invoke-Expression $fixedHandler
        
    # Test the simplified function
    $result = Open-FileSimple -FilePath $testFile
        
    if ($result) {
      Write-Host "✅ Simplified file opening works!" -ForegroundColor Green
    }
    else {
      Write-Host "❌ Simplified file opening failed" -ForegroundColor Red
    }
        
  }
  else {
    Write-Host "❌ Test file was not created successfully" -ForegroundColor Red
  }
    
  # Now let's create a FIXED version of the RawrXD double-click handler
  Write-Host "`n🔧 Creating FIXED RawrXD Handler..." -ForegroundColor Magenta
    
  $fixedRawrHandler = @'
# Fixed Double-Click Handler for RawrXD File Browser
$explorer.add_NodeMouseDoubleClick({
    param($sender, $e)
    try {
        $node = $e.Node
        if ($node.Tag -and $node.Tag -ne "DUMMY") {
            $filePath = $node.Tag
            
            Write-Host "🔍 Double-click detected on: $filePath" -ForegroundColor Cyan
            
            # Basic validation first
            if (-not (Test-Path $filePath)) {
                Write-Host "❌ File not found: $filePath" -ForegroundColor Red
                return
            }
            
            if (Test-Path $filePath -PathType Container) {
                Write-Host "ℹ️ Directory double-clicked, expanding..." -ForegroundColor Blue
                return
            }
            
            # Check if it's a file we can handle
            $fileInfo = Get-Item $filePath
            Write-Host "✅ File found: $($fileInfo.Length) bytes" -ForegroundColor Green
            
            # Size check
            if ($fileInfo.Length -gt 10MB) {
                [System.Windows.Forms.MessageBox]::Show("File is too large (>10MB). Please use a different editor.", "File Too Large", "OK", "Warning")
                return
            }
            
            # Extension check for safety
            $extension = [System.IO.Path]::GetExtension($filePath).ToLower()
            $dangerousExts = @('.exe', '.bat', '.cmd', '.com', '.scr', '.msi', '.dll')
            if ($extension -in $dangerousExts) {
                $result = [System.Windows.Forms.MessageBox]::Show("This file type ($extension) may not be suitable for text editing. Open anyway?", "File Type Warning", "YesNo", "Question")
                if ($result -ne "Yes") {
                    return
                }
            }
            
            try {
                # Read the file content
                $content = [System.IO.File]::ReadAllText($filePath)
                Write-Host "✅ File content read successfully: $($content.Length) characters" -ForegroundColor Green
                
                # Assign to editor (this is where the magic happens)
                if ($script:editor) {
                    $script:editor.Text = $content
                    $global:currentFile = $filePath
                    $form.Text = "RawrXD - $([System.IO.Path]::GetFileName($filePath))"
                    Write-Host "🎉 File opened successfully in editor!" -ForegroundColor Green
                } else {
                    Write-Host "❌ Editor not initialized!" -ForegroundColor Red
                    [System.Windows.Forms.MessageBox]::Show("Editor is not properly initialized.", "Editor Error", "OK", "Error")
                }
                
            } catch {
                Write-Host "❌ Error reading file: $_" -ForegroundColor Red
                [System.Windows.Forms.MessageBox]::Show("Error reading file: $($_.Exception.Message)", "File Error", "OK", "Error")
            }
        }
    } catch {
        Write-Host "❌ Error in double-click handler: $_" -ForegroundColor Red
        [System.Windows.Forms.MessageBox]::Show("Error: $($_.Exception.Message)", "Handler Error", "OK", "Error")
    }
})
'@
    
  Write-Host "💾 Fixed handler code ready!" -ForegroundColor Green
  Write-Host "This removes complex security checks that might be causing silent failures." -ForegroundColor Gray
    
}
catch {
  Write-Host "❌ Fix creation failed: $_" -ForegroundColor Red
}
finally {
  # Cleanup
  $testFile = Join-Path (Get-Location) "debug_test.txt"
  if (Test-Path $testFile) {
    Remove-Item $testFile -Force -ErrorAction SilentlyContinue
    Write-Host "🧹 Cleaned up test file" -ForegroundColor Gray
  }
}

Write-Host "`n🎯 RECOMMENDATION:" -ForegroundColor Yellow
Write-Host "Replace the complex double-click handler in RawrXD with the simplified version above." -ForegroundColor Gray
Write-Host "The current handler may have too many security checks causing silent failures." -ForegroundColor Gray