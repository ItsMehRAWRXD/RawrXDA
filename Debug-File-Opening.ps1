# 🔍 Debug File Opening Issue
# Test the specific file opening functionality

Write-Host "🔍 Testing RawrXD File Opening Debug..." -ForegroundColor Cyan

try {
  # Load the RawrXD script to check for initialization issues
  Write-Host "📋 Loading RawrXD for analysis..." -ForegroundColor Yellow
    
  # Read the RawrXD file
  $rawrContent = Get-Content ".\RawrXD.ps1" -Raw
    
  # Check if key variables are properly initialized
  Write-Host "`n🔧 Checking Key Components:" -ForegroundColor Cyan
    
  # 1. Check if $script:editor is defined
  if ($rawrContent -match '\$script:editor\s*=\s*New-Object') {
    Write-Host "✅ Editor variable initialization found" -ForegroundColor Green
  }
  else {
    Write-Host "❌ Editor variable initialization NOT found" -ForegroundColor Red
  }
    
  # 2. Check if double-click handler exists
  if ($rawrContent -match 'add_NodeMouseDoubleClick') {
    Write-Host "✅ Double-click handler found" -ForegroundColor Green
  }
  else {
    Write-Host "❌ Double-click handler NOT found" -ForegroundColor Red
  }
    
  # 3. Check if security functions exist
  $securityFunctions = @('Test-SessionSecurity', 'Test-InputSafety', 'Write-SecurityLog', 'Write-DevConsole')
  foreach ($func in $securityFunctions) {
    if ($rawrContent -match "function $func") {
      Write-Host "✅ Function $func found" -ForegroundColor Green
    }
    else {
      Write-Host "❌ Function $func NOT found" -ForegroundColor Red
    }
  }
    
  # 4. Check if $global:currentFile is used
  if ($rawrContent -match '\$global:currentFile') {
    Write-Host "✅ Global currentFile variable usage found" -ForegroundColor Green
  }
  else {
    Write-Host "❌ Global currentFile variable usage NOT found" -ForegroundColor Red
  }
    
  # 5. Check if there are any obvious syntax errors around the double-click handler
  Write-Host "`n🔍 Checking for syntax issues..." -ForegroundColor Cyan
  $lines = $rawrContent -split "`n"
  $doubleClickStart = -1
  for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match 'add_NodeMouseDoubleClick') {
      $doubleClickStart = $i
      break
    }
  }
    
  if ($doubleClickStart -gt -1) {
    Write-Host "✅ Double-click handler starts at line: $($doubleClickStart + 1)" -ForegroundColor Green
        
    # Check the next 20 lines for obvious issues
    $checkLines = [Math]::Min(20, $lines.Count - $doubleClickStart)
    for ($j = 0; $j -lt $checkLines; $j++) {
      $line = $lines[$doubleClickStart + $j].Trim()
      if ($line -match '^\s*\$script:editor\.Text\s*=\s*\$content\s*$') {
        Write-Host "✅ Editor text assignment found at line: $($doubleClickStart + $j + 1)" -ForegroundColor Green
        break
      }
    }
  }
    
  # 6. Test with a simple file
  Write-Host "`n📄 Testing with a sample file..." -ForegroundColor Cyan
    
  # Create a test file
  $testFile = ".\test_file_opening.txt"
  "This is a test file for debugging file opening functionality." | Out-File -FilePath $testFile -Encoding UTF8
    
  if (Test-Path $testFile) {
    Write-Host "✅ Test file created: $testFile" -ForegroundColor Green
        
    # Test file reading
    try {
      $testContent = [System.IO.File]::ReadAllText($testFile)
      Write-Host "✅ File reading works: '$($testContent.Trim())'" -ForegroundColor Green
    }
    catch {
      Write-Host "❌ File reading failed: $_" -ForegroundColor Red
    }
        
    # Clean up
    Remove-Item $testFile -Force -ErrorAction SilentlyContinue
  }
    
  # 7. Check for missing dependencies
  Write-Host "`n🔧 Checking Dependencies:" -ForegroundColor Cyan
    
  try {
    [System.Windows.Forms.TreeView] | Out-Null
    Write-Host "✅ System.Windows.Forms.TreeView available" -ForegroundColor Green
  }
  catch {
    Write-Host "❌ System.Windows.Forms.TreeView NOT available" -ForegroundColor Red
  }
    
  try {
    [System.Windows.Forms.RichTextBox] | Out-Null
    Write-Host "✅ System.Windows.Forms.RichTextBox available" -ForegroundColor Green
  }
  catch {
    Write-Host "❌ System.Windows.Forms.RichTextBox NOT available" -ForegroundColor Red
  }
    
  # 8. Specific issue diagnosis
  Write-Host "`n🎯 DIAGNOSIS:" -ForegroundColor Magenta
    
  if ($rawrContent -match 'add_NodeMouseDoubleClick.*{') {
    Write-Host "✅ Double-click handler structure appears correct" -ForegroundColor Green
        
    # Check if the handler has proper error handling
    $handlerSection = $rawrContent.Substring($rawrContent.IndexOf('add_NodeMouseDoubleClick'), 
      [Math]::Min(2000, $rawrContent.Length - $rawrContent.IndexOf('add_NodeMouseDoubleClick')))
        
    if ($handlerSection -match 'try.*catch') {
      Write-Host "✅ Error handling in double-click handler found" -ForegroundColor Green
    }
    else {
      Write-Host "⚠️ Limited error handling in double-click handler" -ForegroundColor Yellow
    }
        
    if ($handlerSection -match '\$script:editor\.Text\s*=\s*\$content') {
      Write-Host "✅ Editor text assignment found in handler" -ForegroundColor Green
    }
    else {
      Write-Host "❌ Editor text assignment NOT found in handler" -ForegroundColor Red
    }
  }
    
  Write-Host "`n💡 RECOMMENDATIONS:" -ForegroundColor Yellow
  Write-Host "1. Check if RawrXD is properly loading all functions before UI creation" -ForegroundColor Gray
  Write-Host "2. Verify that security functions are not causing silent failures" -ForegroundColor Gray
  Write-Host "3. Test with simple files first (small .txt files)" -ForegroundColor Gray
  Write-Host "4. Check the Windows event logs for any access denied errors" -ForegroundColor Gray
  Write-Host "5. Run RawrXD as administrator if needed" -ForegroundColor Gray
    
}
catch {
  Write-Host "❌ Debug analysis failed: $_" -ForegroundColor Red
}

Write-Host "`n🔚 Debug analysis complete!" -ForegroundColor Cyan