# Test File Generator for Phase 1 Testing
# Creates minimal SafeTensors and PyTorch test files

Write-Host "Creating test files for Phase 1 universal format loader..."

# SafeTensors test file
$safeMagic = [byte[]]@(0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)  # 8 byte metadata
$safeMetadata = [System.Text.Encoding]::UTF8.GetBytes('{"test":"model"}')
$safeTensor = [byte[]]@(0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08)
$safeData = $safeMagic + $safeMetadata + $safeTensor

[System.IO.File]::WriteAllBytes("test_simple.safetensors", $safeData)
Write-Host "✅ Created test_simple.safetensors ($($safeData.Length) bytes)"

# PyTorch test file (minimal ZIP)
$ptMagic = [byte[]]@(0x50, 0x4B, 0x03, 0x04)  # ZIP signature
$ptData = $ptMagic + [byte[]]::new(100)  # Add some padding

[System.IO.File]::WriteAllBytes("test_simple.pt", $ptData)
Write-Host "✅ Created test_simple.pt ($($ptData.Length) bytes)"

Write-Host ""
Write-Host "Test files created successfully!"
Write-Host "- test_simple.safetensors: SafeTensors format with basic structure"
Write-Host "- test_simple.pt: PyTorch format with ZIP signature"
Write-Host ""
Write-Host "Now launch RawrXD-QtShell.exe and test loading these files!"