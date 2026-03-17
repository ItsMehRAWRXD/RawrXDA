# Sample PowerShell code for testing the /review command
function Get-FileInfo {
  param($path)
    
  # No error checking - this is a problem!
  $file = Get-Item $path
  $size = $file.Length
  $name = $file.Name
    
  # Poor variable naming
  $x = $file.LastWriteTime
    
  # Missing return statement
  Write-Host "File: $name, Size: $size, Modified: $x"
}

# Hardcoded path - not flexible
$hardcodedPath = "C:\temp\test.txt"
Get-FileInfo $hardcodedPath

# Missing parameter validation
function Calculate-Sum {
  param($a, $b)
  $a + $b  # What if $a or $b are null?
}

$result = Calculate-Sum 5 10
Write-Output $result