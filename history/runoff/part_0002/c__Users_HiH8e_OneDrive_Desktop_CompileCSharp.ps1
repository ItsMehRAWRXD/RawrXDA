# Define the path to the csc.exe compiler
$cscPath = "C:\Windows\Microsoft.NET\Framework\v4.0.30319\csc.exe"

# Define the path to your C# source file
$sourceFile = "C:\path\to\your\file.cs"

# Define the output directory and executable name
$outputDir = "C:\path\to\output"
$outputExe = "MyApp.exe"

# Ensure the output directory exists
if (-not (Test-Path -Path $outputDir)) {
  New-Item -ItemType Directory -Path $outputDir
}

# Compile the C# file
& $cscPath /out:"$outputDir\$outputExe" $sourceFile

# Check if the compilation was successful
if ($?) {
  Write-Host "Compilation succeeded. Executable created: $outputDir\$outputExe" -ForegroundColor Green
}
else {
  Write-Host "Compilation failed." -ForegroundColor Red
}