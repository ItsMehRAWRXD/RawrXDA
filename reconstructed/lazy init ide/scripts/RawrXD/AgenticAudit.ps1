param(
    [string]$SourcePath = ".\src",
    [string]$OutputPath = ".\audit_report.json"
)

$ErrorActionPreference = 'Stop'

$AuditResults = @{
    Metadata = @{
        ScanDate = Get-Date -Format "yyyy-MM-dd HH:mm"
        TotalFiles = 0
        LinesOfCode = 0
        SourcePath = (Resolve-Path $SourcePath).Path
    }
    Categories = @{
        CoreArchitecture = @{ Score = 0; Max = 100; Missing = @() }
        AIIntegration   = @{ Score = 0; Max = 100; Missing = @() }
        AgentLoop       = @{ Score = 0; Max = 100; Missing = @() }
        ToolSystem      = @{ Score = 0; Max = 100; Missing = @() }
        MemoryContext   = @{ Score = 0; Max = 100; Missing = @() }
        Win32Native     = @{ Score = 0; Max = 100; Missing = @() }
        Performance     = @{ Score = 0; Max = 100; Missing = @() }
        Security        = @{ Score = 0; Max = 100; Missing = @() }
    }
    CriticalGaps = @()
    FilesByCategory = @{}
    PatternHits = @{}
}

# Pattern definitions for autonomous IDE requirements
$Patterns = @{
    # Core Architecture
    CoreArchitecture = @{
        ModularHooks = @( "DLL_EXPORT", "PluginInterface", "SkillRegistry", "AgentConnector" )
        MessageBus   = @( "PostMessage", "WM_USER", "PipeServer", "ZeroMQ", "MessageQueue" )
    }

    # AI Integration
    AIIntegration = @{
        LLMConnectors = @( "Ollama", "OpenAI", "Anthropic", "LocalModel", "GGUF", "ChatCompletion" )
        Streaming     = @( "SSE", "ChunkParser", "TokenStream", "AsyncResponse" )
        ToolCalling   = @( "FunctionCall", "ToolSchema", "JSONSchema", "ExecuteTool" )
    }

    # Agent Loop
    AgentLoop = @{
        AutonomousLoop = @( "AgentLoop", "PlanExecute", "SelfHeal", "DecisionEngine", "GoalStack" )
        StateMachine   = @( "StateMachine", "BehaviorTree", "AgentState", "ContextSwitch" )
    }

    # Tool System
    ToolSystem = @{
        FileTools    = @( "ReadFile", "WriteFile", "SearchCode", "ReplaceText", "GitDiff" )
        BuildTools   = @( "Compile", "MSBuild", "CMake", "Link", "InvokeCompiler" )
        AnalysisTools= @( "StaticAnalysis", "ASTParse", "SymbolTable", "Lint" )
    }

    # Memory/Context
    MemoryContext = @{
        VectorStore = @( "Embedding", "VectorDB", "Chroma", "FAISS", "ContextWindow" )
        Persistence = @( "Checkpoint", "SaveState", "MemoryBank", "ConversationHistory" )
    }

    # Win32 Native
    Win32Native = @{
        Windowing     = @( "CreateWindow", "RegisterClass", "WndProc", "HWND", "DirectComposition" )
        InterProcess  = @( "NamedPipe", "SharedMemory", "MemoryMappedFile", "Semaphore" )
        Rendering     = @( "Direct2D", "DirectWrite", "GDI+", "Vulkan", "GPUContext" )
    }

    # Performance
    Performance = @{
        AsyncPatterns = @( "IOCP", "ThreadPool", "OverlappedIO", "AsyncAwait", "TaskQueue" )
        ZeroCopy      = @( "ZeroCopy", "MemoryPool", "ArenaAllocator", "SIMD", "AVX512" )
    }

    # Security
    Security = @{
        Sandboxing = @( "JobObject", "Sandbox", "TokenImpersonation", "ACL", "RestrictedToken" )
        Validation = @( "InputValidate", "SchemaCheck", "Sanitize", "Escape", "JWT" )
    }
}

# Initialize tracking structures
foreach ($category in $Patterns.Keys) {
    $AuditResults.FilesByCategory[$category] = @()
    $AuditResults.PatternHits[$category] = New-Object System.Collections.Generic.HashSet[string]
}

# Robust file scan across extensions
$extensions = @('.cpp','.cc','.cxx','.h','.hpp','.c','.asm','.ps1','.rs','.json','.yml','.yaml','.toml')
$files = Get-ChildItem -Path $SourcePath -Recurse -File | Where-Object { $_.Extension -in $extensions }

foreach ($file in $files) {
    try {
        $content = Get-Content $file.FullName -Raw -ErrorAction Stop
    } catch {
        continue
    }
    $AuditResults.Metadata.TotalFiles++
    try {
        $AuditResults.Metadata.LinesOfCode += (Get-Content $file.FullName -ErrorAction Stop).Count
    } catch {}

    foreach ($category in $Patterns.Keys) {
        $catFound = $false
        foreach ($groupName in $Patterns[$category].Keys) {
            foreach ($pattern in $Patterns[$category][$groupName]) {
                if ($content -match [Regex]::Escape($pattern)) {
                    [void]$AuditResults.PatternHits[$category].Add($pattern)
                    $catFound = $true
                }
            }
        }
        if ($catFound) {
            $AuditResults.FilesByCategory[$category] += $file.FullName
        }
    }
}

# Calculate scores and identify gaps
foreach ($category in $AuditResults.Categories.Keys) {
    # Flatten patterns list for this category
    $patternList = @()
    foreach ($groupName in $Patterns[$category].Keys) {
        $patternList += $Patterns[$category][$groupName]
    }
    $patternCount = $patternList.Count
    $foundCount = 0
    foreach ($p in $patternList) {
        if ($AuditResults.PatternHits[$category].Contains($p)) { $foundCount++ }
    }

    $score = if ($patternCount -gt 0) { [math]::Min(100, [math]::Round(($foundCount / $patternCount) * 100)) } else { 0 }
    $AuditResults.Categories[$category].Score = $score

    $missing = $patternList | Where-Object { -not $AuditResults.PatternHits[$category].Contains($_) }
    $AuditResults.Categories[$category].Missing = $missing

    if ($score -lt 40) {
        $AuditResults.CriticalGaps += "CRITICAL: $category has only $score% coverage"
    } elseif ($score -lt 70) {
        $AuditResults.CriticalGaps += "WARNING: $category needs work ($score%)"
    }
}

# Export results
$OutputFull = Join-Path (Get-Location) $OutputPath
$AuditResults | ConvertTo-Json -Depth 10 | Out-File -FilePath $OutputFull -Encoding UTF8
Write-Host "Audit complete. Report saved to $OutputFull" -ForegroundColor Green
Write-Host "Critical gaps found: $($AuditResults.CriticalGaps.Count)" -ForegroundColor Red
$AuditResults.CriticalGaps | ForEach-Object { Write-Host "  $_" -ForegroundColor Yellow }
