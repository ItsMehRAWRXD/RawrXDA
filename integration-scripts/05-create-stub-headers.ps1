# 05-create-stub-headers.ps1
# Create stub headers for files that don't have .hpp in old source

param(
    [string]$TargetRoot = "D:\rawrxd"
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "RawrXD Stub Header Creation Script v1.0" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Headers to create as stubs (don't exist in old source)
$stubHeaders = @(
    @{
        Path = "src/qtapp/alert_system.hpp"
        ClassName = "AlertSystem"
        Include = "alert_system.h"
    },
    @{
        Path = "src/qtapp/dap_handler.hpp"
        ClassName = "DAPHandler"
        Include = "dap_handler.h"
    },
    @{
        Path = "src/qtapp/DebuggerPanel.hpp"
        ClassName = "DebuggerPanel"
        Include = "DebuggerPanel.h"
    },
    @{
        Path = "src/qtapp/TestExplorerPanel.hpp"
        ClassName = "TestExplorerPanel"
        Include = "TestExplorerPanel.h"
    },
    @{
        Path = "src/qtapp/memory_persistence_system.hpp"
        ClassName = "MemoryPersistenceSystem"
        Include = "memory_persistence_system.h"
    },
    @{
        Path = "src/qtapp/language_support_system.hpp"
        ClassName = "LanguageSupportSystem"
        Include = "language_support_system.h"
    },
    @{
        Path = "src/qtapp/ThemeManager.hpp"
        ClassName = "ThemeManager"
        Include = "ThemeManager.h"
    },
    @{
        Path = "src/qtapp/syntax_highlighter.hpp"
        ClassName = "SyntaxHighlighter"
        Include = "syntax_highlighter.h"
    },
    @{
        Path = "src/qtapp/code_minimap.hpp"
        ClassName = "CodeMinimap"
        Include = "code_minimap.h"
    },
    @{
        Path = "src/qtapp/enterprise_tools_panel.hpp"
        ClassName = "EnterpriseToolsPanel"
        Include = "enterprise_tools_panel.h"
    },
    @{
        Path = "src/qtapp/discovery_dashboard.hpp"
        ClassName = "DiscoveryDashboard"
        Include = "discovery_dashboard.h"
    },
    @{
        Path = "src/qtapp/code_completion_provider.hpp"
        ClassName = "CodeCompletionProvider"
        Include = "code_completion_provider.h"
    },
    @{
        Path = "src/qtapp/real_time_refactoring.hpp"
        ClassName = "RealTimeRefactoring"
        Include = "real_time_refactoring.h"
    },
    @{
        Path = "src/qtapp/intelligent_error_analysis.hpp"
        ClassName = "IntelligentErrorAnalysis"
        Include = "intelligent_error_analysis.h"
    },
    @{
        Path = "src/qtapp/latency_monitor.hpp"
        ClassName = "LatencyMonitor"
        Include = "latency_monitor.h"
    }
)

$created = 0
$skipped = 0

foreach ($stub in $stubHeaders) {
    $fullPath = Join-Path $TargetRoot $stub.Path
    
    if (Test-Path $fullPath) {
        Write-Host "  ⏭ SKIP: $($stub.Path) (already exists)" -ForegroundColor Gray
        $skipped++
        continue
    }
    
    # Create header content
    $guardName = ($stub.Path -replace '[/\\]', '_' -replace '\.', '_').ToUpper()
    $content = @"
#ifndef $guardName
#define $guardName

// Forward compatibility stub - redirects to .h file
#include "$($stub.Include)"

#endif // $guardName
"@
    
    try {
        # Ensure directory exists
        $dir = Split-Path $fullPath -Parent
        if (-not (Test-Path $dir)) {
            New-Item -ItemType Directory -Path $dir -Force | Out-Null
        }
        
        # Write stub file
        $content | Out-File -FilePath $fullPath -Encoding UTF8 -NoNewline
        Write-Host "  ✓ CREATED: $($stub.Path)" -ForegroundColor Green
        $created++
    }
    catch {
        Write-Host "  ✗ ERROR: $($stub.Path) - $($_.Exception.Message)" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Stub Header Creation Summary" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Created: $created" -ForegroundColor Green
Write-Host "  Skipped: $skipped" -ForegroundColor Gray
Write-Host ""

exit 0
