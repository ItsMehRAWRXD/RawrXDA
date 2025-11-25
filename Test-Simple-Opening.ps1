# 🔧 Simple File Opening Test
# Bypass security checks to test core functionality

Write-Host "🧪 Testing Core File Opening Functionality..." -ForegroundColor Cyan

try {
  # Create a simple test file
  $testFile = ".\simple_test.txt"
  "Hello World! This is a simple test file." | Out-File -FilePath $testFile -Encoding UTF8
    
  Write-Host "✅ Created test file: $testFile" -ForegroundColor Green
    
  # Test basic file reading
  $content = [System.IO.File]::ReadAllText($testFile)
  Write-Host "✅ File content read: '$($content.Trim())'" -ForegroundColor Green
    
  # Test Windows Forms components
  Add-Type -AssemblyName System.Windows.Forms
  Add-Type -AssemblyName System.Drawing
    
  Write-Host "✅ Windows Forms assemblies loaded" -ForegroundColor Green
    
  # Create a simple test editor
  $testEditor = New-Object System.Windows.Forms.RichTextBox
  $testEditor.Text = $content
    
  Write-Host "✅ RichTextBox created and content assigned" -ForegroundColor Green
  Write-Host "📄 Editor content: '$($testEditor.Text.Trim())'" -ForegroundColor Yellow
    
  # Test the exact security functions from RawrXD
  Write-Host "`n🔒 Testing Security Functions..." -ForegroundColor Cyan
    
  # Load RawrXD functions
  try {
    . ".\RawrXD.ps1" 2>$null
    Write-Host "✅ RawrXD script loaded" -ForegroundColor Green
        
    # Test each security function individually
    try {
      $sessionResult = Test-SessionSecurity
      Write-Host "✅ Test-SessionSecurity result: $sessionResult" -ForegroundColor Green
    }
    catch {
      Write-Host "❌ Test-SessionSecurity failed: $_" -ForegroundColor Red
    }
        
    try {
      $inputResult = Test-InputSafety -Input $testFile -Type "FilePath"
      Write-Host "✅ Test-InputSafety result: $inputResult" -ForegroundColor Green
    }
    catch {
      Write-Host "❌ Test-InputSafety failed: $_" -ForegroundColor Red
    }
        
    try {
      Write-SecurityLog "Test log entry" "INFO" "Testing security log"
      Write-Host "✅ Write-SecurityLog executed" -ForegroundColor Green
    }
    catch {
      Write-Host "❌ Write-SecurityLog failed: $_" -ForegroundColor Red
    }
        
  }
  catch {
    Write-Host "⚠️ RawrXD script loading failed: $_" -ForegroundColor Yellow
    Write-Host "This is expected if RawrXD has GUI initialization code that can't run in this context" -ForegroundColor Gray
  }
    
  # Test simple file opening logic without security
  Write-Host "`n📝 Testing Simplified File Opening Logic..." -ForegroundColor Cyan
    
  if (Test-Path $testFile -PathType Leaf) {
    $fileInfo = Get-Item $testFile
    Write-Host "✅ File exists, size: $($fileInfo.Length) bytes" -ForegroundColor Green
        
    if ($fileInfo.Length -lt 10MB) {
      Write-Host "✅ File size acceptable" -ForegroundColor Green
            
      $extension = [System.IO.Path]::GetExtension($testFile).ToLower()
      Write-Host "✅ File extension: $extension" -ForegroundColor Green
            
      $fileContent = [System.IO.File]::ReadAllText($testFile)
      Write-Host "✅ File content loaded: $($fileContent.Length) characters" -ForegroundColor Green
            
      # Simulate editor assignment
      $simulatedEditor = New-Object System.Windows.Forms.RichTextBox
      $simulatedEditor.Text = $fileContent
      Write-Host "✅ Editor text assignment successful" -ForegroundColor Green
      Write-Host "📄 Final editor content: '$($simulatedEditor.Text.Trim())'" -ForegroundColor Yellow
            
    }
    else {
      Write-Host "❌ File too large" -ForegroundColor Red
    }
  }
  else {
    Write-Host "❌ File not found or not a file" -ForegroundColor Red
  }
    
}
catch {
  Write-Host "❌ Test failed: $_" -ForegroundColor Red
}
finally {
  # Cleanup
  if (Test-Path ".\simple_test.txt") {
    Remove-Item ".\simple_test.txt" -Force -ErrorAction SilentlyContinue
    Write-Host "🧹 Cleaned up test file" -ForegroundColor Gray
  }
}

Write-Host "`n🎯 CONCLUSION:" -ForegroundColor Magenta
Write-Host "If this test passed, the core file opening logic works." -ForegroundColor Gray
Write-Host "The issue is likely in the security functions or event handler context." -ForegroundColor Gray