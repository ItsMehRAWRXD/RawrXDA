<#
.SYNOPSIS
    Organizes C# source code files into a structured breakdown.

.DESCRIPTION
    This function processes C# source code files from an input directory and organizes them:
    - Main classes (files containing "public class") are combined into Main.cs
    - Other files are kept as separate modules
    - Creates a ModuleNames.txt file listing all modules

.PARAMETER InputDir
    The directory containing the C# source files to process.

.PARAMETER OutputDir
    The output directory where the organized files will be created. Defaults to "Breakdown".

.EXAMPLE
    Create-CustomWebBrowser -InputDir "C:\YourProject\Source" -OutputDir "Breakdown"

.EXAMPLE
    Create-CustomWebBrowser -InputDir "C:\MyProject\Source"
#>
function Create-CustomWebBrowser {
    param (
        [Parameter(Mandatory = $true)]
        [string]$InputDir,

        [Parameter(Mandatory = $false)]
        [string]$OutputDir = "Breakdown"
    )

    # Validate input directory exists
    if (-not (Test-Path -Path $InputDir -PathType Container)) {
        Write-Error "Input directory does not exist: $InputDir"
        return
    }

    # Create output directory if it doesn't exist
    if (-not (Test-Path -Path $OutputDir -PathType Container)) {
        New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
        Write-Host "Created output directory: $OutputDir" -ForegroundColor Green
    }

    # Get all C# files in the input directory (recursively)
    $files = Get-ChildItem -Path $InputDir -Filter "*.cs" -Recurse -File

    if ($files.Count -eq 0) {
        Write-Warning "No .cs files found in $InputDir"
        return
    }

    # Initialize variables to store main file content and module names
    $mainFileContent = @()
    $moduleNames = @()
    $moduleFiles = @()

    # Iterate through each file
    foreach ($file in $files) {
        $fileContent = Get-Content -Path $file.FullName -Raw

        # Check if the file contains a public class (main class)
        if ($fileContent -match "public\s+class") {
            # This is a main class file - add to main file content
            $mainFileContent += "#region $($file.Name)"
            $mainFileContent += $fileContent
            $mainFileContent += "#endregion"
            $mainFileContent += ""

            $moduleNames += $file.BaseName
            Write-Host "Found main class: $($file.Name)" -ForegroundColor Cyan
        }
        else {
            # This is a module file - copy to output directory
            $moduleName = $file.BaseName
            $moduleNames += $moduleName

            # Preserve relative directory structure
            $relativePath = $file.FullName.Substring($InputDir.Length).TrimStart('\', '/')
            $outputPath = Join-Path -Path $OutputDir -ChildPath $relativePath
            $outputDirPath = Split-Path -Path $outputPath -Parent

            # Create subdirectory if needed
            if (-not (Test-Path -Path $outputDirPath -PathType Container)) {
                New-Item -ItemType Directory -Path $outputDirPath -Force | Out-Null
            }

            # Copy the file
            Copy-Item -Path $file.FullName -Destination $outputPath -Force
            $moduleFiles += $outputPath
            Write-Host "Copied module: $relativePath" -ForegroundColor Yellow
        }
    }

    # Create the main file if we have main class content
    if ($mainFileContent.Count -gt 0) {
        $mainFilePath = Join-Path -Path $OutputDir -ChildPath "Main.cs"
        $mainFileContent | Out-File -FilePath $mainFilePath -Encoding UTF8 -Force
        Write-Host "Created main file: $mainFilePath" -ForegroundColor Green
    }

    # Write the module names to a text file
    $moduleNamesFilePath = Join-Path -Path $OutputDir -ChildPath "ModuleNames.txt"
    $moduleNames | Out-File -FilePath $moduleNamesFilePath -Encoding UTF8 -Force
    Write-Host "Created module names file: $moduleNamesFilePath" -ForegroundColor Green

    # Create a summary
    Write-Host "`n=== Processing Summary ===" -ForegroundColor Magenta
    Write-Host "Total files processed: $($files.Count)" -ForegroundColor White
    Write-Host "Main classes found: $($mainFileContent.Count -gt 0 ? 'Yes' : 'No')" -ForegroundColor White
    Write-Host "Module files copied: $($moduleFiles.Count)" -ForegroundColor White
    Write-Host "Output directory: $OutputDir" -ForegroundColor White
    Write-Host "`nProcessing complete!" -ForegroundColor Green
}

# Note: Export-ModuleMember only works when the file is loaded as a module
# When dot-sourced, functions are automatically available in the parent scope
# Export-ModuleMember -Function Create-CustomWebBrowser

