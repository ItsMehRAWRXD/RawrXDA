<#
.SYNOPSIS
    Replaces Qt classes with Win32/STL equivalents in C++ source files.

.DESCRIPTION
    Scans C++ header and source files for common Qt types and replaces them 
    with their Win32/STL counterparts (e.g., QString -> std::string).
    
    Includes predefined mappings for:
    - String types (QString, QByteArray)
    - JSON types (QJsonObject, QJsonDocument)
    - File system (QFile, QDir)
    - Collections (QList, QVector, QMap)

.PARAMETER Path
    The directory or file path to process.

.PARAMETER Recursive
    Search subdirectories recursively.

.PARAMETER DryRun
    Preview changes without modifying files.

.PARAMETER Backup
    Create .bak files before modification.

.EXAMPLE
    .\replace_classes.ps1 -Path "./src" -Recursive -DryRun
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$Path,

    [switch]$Recursive,
    [switch]$DryRun,
    [switch]$Backup
)

$Replacements = @{
    # Strings
    "QString"       = "std::string"
    "QByteArray"    = "std::vector<uint8_t>"
    "QLatin1String" = "std::string"
    "QStringList"   = "std::vector<std::string>"

    # JSON
    "QJsonObject"   = "nlohmann::json"
    "QJsonArray"    = "nlohmann::json"
    "QJsonDocument" = "nlohmann::json"
    "QJsonValue"    = "nlohmann::json"
    "QJsonParseError" = "nlohmann::json::parse_error"

    # Collections
    "QVector"       = "std::vector"
    "QList"         = "std::vector"
    "QMap"          = "std::map"
    "QHash"         = "std::unordered_map"
    "QSet"          = "std::unordered_set"
    "QPair"         = "std::pair"

    # File System
    "QFile"         = "std::ifstream" # Context dependent, but good default
    "QDir"          = "std::filesystem::path"
    "QFileInfo"     = "std::filesystem::directory_entry"
    "QIODevice"     = "std::iostream"

    # Core
    "QObject"       = "void" # Usually removed or replaced with custom base
    "qint64"        = "int64_t"
    "qint32"        = "int32_t"
    "quint64"       = "uint64_t"
    "quint32"       = "uint32_t"
    "qreal"         = "double"
    
    # Macros
    "Q_OBJECT"      = "// Q_OBJECT removed"
    "Q_PROPERTY"    = "// Q_PROPERTY removed"
    "emit "         = "// emit "
    "slots:"        = "// slots:"
    "signals:"      = "// signals:"
}

function Process-File {
    param([string]$FilePath)

    Write-Host "Processing $FilePath..." -ForegroundColor Cyan
    
    $OriginalContent = Get-Content -Path $FilePath -Raw
    $NewContent = $OriginalContent
    $ChangesCount = 0

    foreach ($Key in $Replacements.Keys) {
        $Pattern = "\b" + [Regex]::Escape($Key) + "\b"
        $Value = $Replacements[$Key]
        
        if ($NewContent -match $Pattern) {
            $Count = ([regex]::Matches($NewContent, $Pattern)).Count
            $ChangesCount += $Count
            
            if ($DryRun) {
                Write-Host "  [DryRun] Would replace '$Key' with '$Value' ($Count occurences)" -ForegroundColor Gray
            } else {
                $NewContent = $NewContent -replace $Pattern, $Value
            }
        }
    }

    # Header replacements (approximate)
    if ($NewContent -match "#include <Q\w+>") {
        if ($DryRun) {
             Write-Host "  [DryRun] Would remove Qt includes" -ForegroundColor Gray
        } else {
             $NewContent = $NewContent -replace '#include <Q\w+>.*', '// Removed Qt include'
             
             # Add common replacements
             if ($NewContent -notmatch "#include <string>") { $NewContent = "#include <string>`n" + $NewContent }
             if ($NewContent -notmatch "#include <vector>") { $NewContent = "#include <vector>`n" + $NewContent }
             if ($NewContent -notmatch "#include <nlohmann/json.hpp>" -and ($NewContent -match "nlohmann::json")) { 
                $NewContent = "#include <nlohmann/json.hpp>`n" + $NewContent 
             }
        } 
    }

    if ($ChangesCount -gt 0 -and -not $DryRun) {
        if ($Backup) {
            Copy-Item -Path $FilePath -Destination "$FilePath.bak" -Force
            Write-Host "  Backup created: $FilePath.bak" -ForegroundColor DarkGray
        }
        Set-Content -Path $FilePath -Value $NewContent -Encoding UTF8
        Write-Host "  Modified $FilePath ($ChangesCount replacements)" -ForegroundColor Green
    } elseif ($ChangesCount -eq 0) {
        Write-Host "  No changes needed." -ForegroundColor DarkGray
    }
}

# Main script logic
if (Test-Path -Path $Path -PathType Leaf) {
    Process-File -FilePath $Path
} elseif (Test-Path -Path $Path -PathType Container) {
    # If path is text, fix for Get-ChildItem if implied
    $Filter = if ($Recursive) { @("-Recurse") } else { @() }
    
    $Files = Get-ChildItem -Path $Path -Include *.cpp, *.h, *.hpp @Filter
    
    # If list empty, try without include filter logic if user passed direct files
    if ($Files.Count -eq 0) {
         $Files = Get-ChildItem -Path $Path -Filter "*.*" @Filter | Where-Object { $_.Extension -match "\.(cpp|h|hpp)$" }
    }

    foreach ($File in $Files) {
        Process-File -FilePath $File.FullName
    }
} else {
    Write-Error "Path not found: $Path"
}
